#include "Power.h"
#include "App.h"
#include "BoardPins.h"
#include "MeshRadio.h"
#include "Radar.h"
#include "BleDiagnostics.h"
#include "WifiDiagnostics.h"
#if !defined(MM_NRF52)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
namespace {
TaskHandle_t volatile idleTask=nullptr;
uint32_t inputAt=0,idleWaits=0,idleMs=0,radioEvents=0;
constexpr uint32_t IdlePollMs=20; // below the 30 ms button debounce; also drains GPS/USB regularly
}
#if defined(MM_NRF52)
void powerRadioIrq(){
#else
void IRAM_ATTR powerRadioIrq(){
#endif
  // Called only from the RadioLib GPIO ISR. A pending notification also closes the race
  // between checking the radio IRQ and blocking the task. The RTOS controls sleep
  // (together with the SoftDevice on nRF52); other tasks can still run during this wait.
  if(!idleTask)return;
  BaseType_t woken=pdFALSE;vTaskNotifyGiveFromISR(idleTask,&woken);
#if defined(MM_NRF52)
  portYIELD_FROM_ISR(woken);
#else
  if(woken)portYIELD_FROM_ISR();
#endif
}
namespace {
void waitForRadio(bool usbIdle){
  if(!idleTask)idleTask=xTaskGetCurrentTaskHandle();
  if(!usbIdle||!uiScreenOff()||radar.active||radar.csi!=Radar::CsiOff||wifiProbeActive()||bleProbeActive()||!meshRadio.ready||meshRadio.busy()||
     millis()-inputAt<1000||digitalRead(pins::radioIrq)==HIGH){
    ulTaskNotifyTake(pdTRUE,0);delay(2);return;
  }
  // Block the task, rather than putting the CPU to sleep from application context.
  // BLE and USB tasks remain runnable; RX/TX IRQs release this wait immediately.
  uint32_t start=millis();
  if(ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(IdlePollMs)))radioEvents++;
  idleWaits++;idleMs+=millis()-start;
}
}
uint32_t powerIdleWaits(){return idleWaits;}
uint32_t powerIdleMs(){return idleMs;}
uint32_t powerRadioEvents(){return radioEvents;}
#if defined(MM_NRF52)
void powerWake(){inputAt=millis();}
void powerTick(bool usbIdle){waitForRadio(usbIdle);}
uint8_t powerMhz(){return 64;}
// Application wait time is not measured CPU sleep time: keep these ESP32 counters separate.
uint32_t powerSleeps(){return 0;}
uint32_t powerSleptMs(){return 0;}
uint32_t powerSlowMs(){return 0;}
#else
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_bt.h>
#include <WiFi.h>
#include "App.h"
#include "Config.h"
#include "BoardPins.h"
#include "MeshRadio.h"
#include "MeshServer.h"
#include "Radar.h"
#include "Internet.h"
#include "BleDiagnostics.h"
#include "WifiDiagnostics.h"
#if defined(MM_NATIVE_USB)
#include <HWCDC.h>
#endif

namespace {
constexpr uint32_t Fast=240,Slow=80;
constexpr uint32_t AwakeAfterInput=30000; // no light sleep this long after a key, USB or BLE command
constexpr uint32_t FirstSleep=60000;      // after boot: time for the role choice, the advert and USB checks
constexpr uint32_t SleepMs=500;           // timer wake: keys, room pushes and adverts run at least this often
uint32_t sleeps=0,slowAt=0;uint64_t sleptUs=0,slowMs=0;
void clock(uint32_t mhz){
  if(getCpuFrequencyMhz()==mhz)return;
  if(mhz==Slow)slowAt=millis();else slowMs+=millis()-slowAt;
  setCpuFrequencyMhz(mhz);
#if defined(MM_POWER_DEBUG)
  Serial.printf("PWR clock %u at %lu\n",unsigned(mhz),millis());
#endif
}
// Something needs the full clock or the Wi-Fi radio; the board neither slows down nor sleeps.
// The clock never changes under a running Wi-Fi driver or Bluetooth controller (which, once started,
// stays up until reboot, see Portal.cpp): without this rule a CSI check followed by a map upload over
// the access point ended in a panic on the M9 three times out of three; with it the same run passes.
bool radiosUp(){return WiFi.getMode()!=WIFI_OFF||esp_bt_controller_get_status()!=ESP_BT_CONTROLLER_STATUS_IDLE;}
bool inUse(){
  return !uiScreenOff()||radiosUp()||portalActive()||radar.active||radar.csi!=Radar::CsiOff||wifiProbeActive()||bleProbeActive()
#if !defined(MM_COMPACT)
    ||internet.state!=Internet::Off
#endif
    ||millis()-inputAt<10000;
}
bool maySleep(bool usbIdle){
  if(config.role==RoleNormal||!meshServer.running()||bleActive()||!usbIdle||meshRadio.busy())return false;
  if(millis()<FirstSleep||millis()-inputAt<AwakeAfterInput)return false;
#if defined(MM_NATIVE_USB)
  if(HWCDC::isPlugged())return false; // light sleep would drop the USB Serial/JTAG link of a computer
#endif
  return digitalRead(pins::radioIrq)==LOW; // a received packet waits: handle it first
}
portMUX_TYPE sleepMux=portMUX_INITIALIZER_UNLOCKED;
// As stock MeshCore (ESP32Board::sleep): the radio IRQ pin carries the RadioLib rising-edge interrupt.
// Wake-up needs a level type on it; with interrupts held, a packet arriving meanwhile cannot start
// an endless level interrupt, and the rising edge is restored before they run again.
void lightSleep(){
  const gpio_num_t irq=gpio_num_t(pins::radioIrq);
#if !defined(MM_NATIVE_USB)
  Serial.flush();uart_set_wakeup_threshold(UART_NUM_0,3);esp_sleep_enable_uart_wakeup(0); // the waking bytes are lost
#endif
  esp_sleep_enable_timer_wakeup(SleepMs*1000ULL);
#if defined(MM_POWER_DEBUG)
  if(sleeps<5)Serial.printf("PWR sleep %lu at %lu\n",sleeps,millis());
#endif
  portENTER_CRITICAL(&sleepMux);
  if(gpio_get_level(irq)){portEXIT_CRITICAL(&sleepMux);esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);return;} // a packet came in: handle it first
  gpio_wakeup_enable(irq,GPIO_INTR_HIGH_LEVEL); // SX126x DIO1 / LR1110 IRQ / SX127x DIO0: packet received
#if defined(MM_COMPACT)
  if(pins::button>=0)gpio_wakeup_enable(gpio_num_t(pins::button),GPIO_INTR_LOW_LEVEL); // polled, no interrupt attached
#endif
  esp_sleep_enable_gpio_wakeup();
  int64_t start=esp_timer_get_time();esp_light_sleep_start();
  gpio_wakeup_disable(irq);gpio_set_intr_type(irq,GPIO_INTR_POSEDGE);
#if defined(MM_COMPACT)
  if(pins::button>=0)gpio_wakeup_disable(gpio_num_t(pins::button));
#endif
  bool packet=gpio_get_level(irq);
  portEXIT_CRITICAL(&sleepMux);
  sleptUs+=esp_timer_get_time()-start;sleeps++;
  if(packet)radioIrqPending(); // the edge passed during sleep
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  // USB-UART host: the bytes that woke the board arrive garbled; drop them and stay awake for its commands.
  if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_UART){delay(3);while(Serial.available())Serial.read();inputAt=millis();}
}
}

void powerWake(){inputAt=millis();clock(Fast);}
void powerTick(bool usbIdle){
  bool busy=inUse();clock(busy?Fast:Slow);
  if(!busy&&maySleep(usbIdle)){delay(2);lightSleep();}
  else waitForRadio(usbIdle);
}
uint8_t powerMhz(){return getCpuFrequencyMhz();}
uint32_t powerSleeps(){return sleeps;}
uint32_t powerSleptMs(){return uint32_t(sleptUs/1000);}
uint32_t powerSlowMs(){return uint32_t(slowMs+(getCpuFrequencyMhz()==Slow?millis()-slowAt:0));}
#endif
