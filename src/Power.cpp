#include "Power.h"
#include "App.h"
#include "People.h"
#include "Wardrive.h"
#include "BoardPins.h"
#include "MeshRadio.h"
#include "Radar.h"
#include "BleDiagnostics.h"
#include "WifiDiagnostics.h"
#include "Hardware.h"
#include "ChessNet.h"
#include "ChessTour.h"
#include "Pet.h"
#include "Dice.h"
#if defined(MM_NRF52)
#include <esp_system.h>
#include <nrf_soc.h>
#include <nrf_sdm.h>
#include <flash/flash_nrf5x.h>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <WiFi.h>
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
namespace {
uint32_t offAt=0;
#if !defined(MM_NRF52)
// The wake button pressed at boot: on after it is held 0.7 s (with the boot, about a second), then released.
bool wakeHeld(int pin){
  uint32_t start=millis();while(millis()-start<700){if(digitalRead(pin)==HIGH)return false;delay(5);}
  start=millis();while(digitalRead(pin)==LOW&&millis()-start<4000)delay(10);return true;
}
#endif
[[noreturn]] void sleepNow(int pin);
}
String powerOff(){
  if(!offAt){offAt=millis()+1500;meshRadio.event="Turning off";meshRadio.dirty=true;}
  int way=powerOnWay();
  return way>=0?"OK turning off; hold the button to turn on":way==-2?"OK turning off; the power key turns it on":"OK turning off; RESET or the power switch turns it on";
}
bool powerOffPending(){return offAt!=0;}
int powerOnWay(){return hardware.wakePin();}
void powerOffTick(){
  if(!offAt||int32_t(millis()-offAt)<0)return;
  if(meshRadio.busy()&&millis()-offAt<5000)return;
  people::flush();wardrive::flush();meshRadio.flush();chessNet.flush();tour::net.flush();creature.flush();dicer.flush(); // counters wait 2 s before writing
  Serial.println("OFF");Serial.flush();
  uiFarewell();uint32_t shown=millis();
  int pin=hardware.wakePin();
  // A button still held (the hold that chose "Turn off") would wake the board at once.
  if(pin>=0){while(digitalRead(pin)==LOW&&millis()-shown<10000)delay(10);delay(50);}
  while(millis()-shown<3000)delay(10); // time to read it
  if(radar.active)radar.release();
  meshRadio.sleep();
#if !defined(MM_NRF52)
  if(WiFi.getMode()!=WIFI_OFF)WiFi.mode(WIFI_OFF);
#endif
  hardware.powerDown(true);
  sleepNow(pin);
}
#if defined(MM_NRF52)
// Soft off. A wake from System OFF is a reset with the button still held, and the T114 bootloader takes a
// held USER key at reset for its Bluetooth OTA update mode ("HT-n5262-OTA"): the board stayed there with
// no USB, and a RESET pressed then ended in lost storage (docs/verification.md). Here the CPU stays in
// System ON: this task polls the button from FreeRTOS sleep (tickless idle) and nothing else runs. After
// a 1 s hold the LED lights; the board restarts the ordinary way once the button is released.
namespace {
void sleepNow(int pin){
  bleSilence();flash_nrf5x_flush();
  if(pin<0){while(true)delay(1000);}
  pinMode(pin,INPUT_PULLUP);uint32_t down=0;
  while(true){
    delay(50);
    if(digitalRead(pin)==HIGH){down=0;continue;}
    if(!down){down=millis()|1;continue;}
    if(millis()-down<1000)continue;
    if(pins::led>=0){pinMode(pins::led,OUTPUT);digitalWrite(pins::led,pins::ledOn);}
    while(digitalRead(pin)==LOW)delay(20);
    delay(50);ESP.restart(); // released: the bootloader starts the application
  }
}
}
void powerBootCheck(){}
#else
namespace {
void sleepNow(int pin){
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if(pin>=0){gpio_num_t g=gpio_num_t(pin);esp_sleep_enable_ext0_wakeup(g,0);rtc_gpio_pullup_en(g);rtc_gpio_pulldown_dis(g);}
  gpio_deep_sleep_hold_en(); // the levels Hardware::powerDown() held
  esp_deep_sleep_start();
}
}
void powerBootCheck(){
  if(esp_sleep_get_wakeup_cause()!=ESP_SLEEP_WAKEUP_EXT0)return;
  int pin=hardware.wakePin();if(pin<0)return;
  rtc_gpio_deinit(gpio_num_t(pin));pinMode(pin,INPUT_PULLUP);if(wakeHeld(pin))return;
  hardware.powerDown(false);sleepNow(pin);
}
#endif
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
#include "People.h"
#include "Config.h"
#include "BoardPins.h"
#include "MeshRadio.h"
#include "MeshServer.h"
#include "Radar.h"
#include "Internet.h"
#include "People.h"
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
