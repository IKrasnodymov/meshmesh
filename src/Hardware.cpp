#include "Version.h"
#include "Hardware.h"
#include "BoardPins.h"
#include "Config.h"
#include <Wire.h>
#include <SD.h>
#include <LittleFS.h>
#include <soc/usb_serial_jtag_reg.h>
#include <driver/rtc_io.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
Hardware hardware;
// The keyboard controller: 0x6c on the M9 V1.0, 0x6d on a later board (the stock firmware names V1.0 and V1.1).
// The first address that answers is kept and names the revision (boardName).
static uint8_t keyboardAt=pins::keyboardAddress;
bool Hardware::read(TwoWire& bus,uint8_t addr,uint8_t reg,uint8_t* bytes,uint8_t size,bool stop) {
  bus.beginTransmission(addr);bus.write(reg);if(bus.endTransmission(stop)) return false;
  if(bus.requestFrom(int(addr),int(size))!=size) return false;
  for(unsigned i=0;i<size;i++) bytes[i]=bus.read();return true;
}
bool Hardware::probeKeyboard() {
  for(uint8_t at:{uint8_t(pins::keyboardAddress),uint8_t(pins::keyboardAddress+1)})if(read(Wire1,at,0,&keyboardHw,1,true)){keyboardAt=at;boardName=at==0x6d?"ThinkNode M9 v1.1":"ThinkNode M9 v1.0";micKey=at!=0x6d;return true;}
  return false;
}
bool Hardware::write(uint8_t addr,uint8_t reg,uint8_t value) {
  Wire.beginTransmission(addr);Wire.write(reg);Wire.write(value);return Wire.endTransmission()==0;
}
// Dark: the panel sleeps too (DISPOFF, SLPIN). With only the backlight off it kept driving the last frame
// for hours, and the static lock-screen clock stayed on as a faint ghost over the next pages.
// ST7789: 120 ms between SLPIN and SLPOUT, 5 ms after SLPOUT; the frame memory is kept.
void Hardware::brightness(uint8_t level) {
  static bool asleep=false;static uint32_t sleptAt=0;
  if(!level){ledcWrite(7,255);if(!asleep&&canvas){display.enableDisplay(false);display.enableSleep(true);asleep=true;sleptAt=millis();}return;}
  if(asleep){uint32_t slept=millis()-sleptAt;if(slept<120)delay(120-slept);display.enableSleep(false);delay(5);display.enableDisplay(true);asleep=false;shownValid=false;}
  ledcWrite(7,255-level);
}
// V1.1 (keyboard at 0x6d, Airoha AG3335): the enable is active HIGH, GPIO10 is pulsed after power-on and
// $PAIR650 (backup mode) is sent before power-off, as the stock firmware does. V1.0: the enable is active LOW.
void Hardware::setGps(bool enabled) {
  bool v11=keyboardAt==0x6d;
  if(!enabled&&gpsEnabled&&v11){Serial1.print("$PAIR650,0*25\r\n");Serial1.flush();delay(40);}
  gpsEnabled=enabled;
  digitalWrite(pins::gpsEnable,enabled==v11?HIGH:LOW);
  digitalWrite(pins::gpsReset,LOW);
  if(enabled&&v11){delay(50);digitalWrite(pins::gpsWake,HIGH);delay(5);digitalWrite(pins::gpsWake,LOW);}
  // 115200 baud: the default 256-byte buffer holds 22 ms, less than a map frame; lost sentences fail their checksums.
  if(enabled) {Serial1.setRxBufferSize(2048);Serial1.begin(115200,SERIAL_8N1,pins::gpsRx,pins::gpsTx);lastGpsBaud=millis();}
  else Serial1.end();
}
void Hardware::begin() {
  REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG,USB_SERIAL_JTAG_USB_PAD_ENABLE);
  gpio_deep_sleep_hold_dis();
  for(int pin:{pins::backlight,pins::peripheralPower}) {rtc_gpio_hold_dis(gpio_num_t(pin));pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
  for(int pin:{pins::radioCs,pins::lcdCs,pins::sdCs}) {gpio_hold_dis(gpio_num_t(pin));pinMode(pin,OUTPUT);digitalWrite(pin,HIGH);}
  pinMode(pins::gpsEnable,OUTPUT);pinMode(pins::gpsReset,OUTPUT);pinMode(pins::gpsWake,OUTPUT);pinMode(pins::buzzer,OUTPUT);
  digitalWrite(pins::buzzer,LOW);digitalWrite(pins::gpsWake,LOW);
  SPI.begin(pins::spiClock,pins::spiMiso,pins::spiMosi);
  display.init(240,320,SPI_MODE0);display.setSPISpeed(80000000); // APB/1; the next step down is 40 MHz
  display.setRotation(1);display.fillScreen(0x0862);
  ledcSetup(7,5000,8);ledcAttachPin(pins::backlight,7);brightness(config.brightness);
  canvas=new GFXcanvas16(320,240); // Arduino ESP32 allocates this large buffer from PSRAM when available.
  if(!canvas || !canvas->getBuffer()) {Serial.println("FATAL framebuffer allocation");while(true) delay(1000);}
  font.begin(*canvas);font.setFont(u8g2_font_6x13_t_cyrillic);font.setFontMode(1);
  canvas->fillScreen(0x0862);text(14,38,MESHMM_FIRMWARE,0x07ff);text(14,65,"Starting ThinkNode M9...");flush();
  Wire.begin(pins::sda,pins::scl,100000);Wire.setTimeOut(20);
  Wire1.begin(pins::keyboardSda,pins::keyboardScl,100000);Wire1.setTimeOut(20);
  keyboardOk=probeKeyboard();
  if(keyboardOk) read(Wire1,keyboardAt,0xfe,&keyboardFw,1,true);
  setGps(config.gps); // after the keyboard: its address tells the board revision
  rtcOk=rtc.begin(&Wire);
  if(rtcOk && !rtc.lostPower()) {
    rtc.start();DateTime date=rtc.now();rtcValid=date.year()>=2025 && date.year()<2100;
    if(rtcValid)setUtc(date.unixtime(),"RTC");
  }
  // Never auto-format an existing filesystem. Create ours only on an erased partition.
  fsOk=LittleFS.begin(false,"/littlefs",10,"littlefs");
  sdOk=SD.begin(pins::sdCs,SPI,4000000);
  uint8_t id=0;
  if(read(Wire,0x7c,0,&id,1) && id==0x90) {
    compassOk=write(0x7c,0x0b,0x80)&&write(0x7c,0x0b,0);delay(10);
    compassOk=compassOk&&write(0x7c,0x0b,0x30)&&write(0x7c,0x0a,0x41);
  }
  if(read(Wire,0x6b,0,&id,1) && id==5) {
    imuOk=write(0x6b,0x02,0x40)&&write(0x6b,0x03,0x07)&&write(0x6b,0x04,0)&&write(0x6b,0x06,1)&&write(0x6b,0x08,1);
  }
  Serial.printf("HW %s keyboard=%d at=%02x hw=%02x fw=%02x rtc=%d valid=%d sd=%d fs=%d compass=%d imu=%d psram=%u\n",boardName,keyboardOk,keyboardAt,keyboardHw,keyboardFw,rtcOk,rtcValid,sdOk,fsOk,compassOk,imuOk,ESP.getPsramSize());
}
void Hardware::tick() {
  if(gpsEnabled) {
    unsigned budget=1024;
    while(Serial1.available() && budget--) {gps.encode(Serial1.read());gpsBytes++;}
    static bool fallback=false;
    if(!gpsBytes && !fallback && millis()-lastGpsBaud>6000) {Serial1.updateBaudRate(9600);fallback=true;}
    if(gpsTime() && gps.time.isUpdated()) {
      DateTime now(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());
      bool accepted=setUtc(now.unixtime(),"GPS");
      static uint32_t synced=0;if(accepted && rtcOk && (!rtcValid || millis()-synced>3600000)) {rtc.adjust(now);rtcValid=true;synced=millis();}
    }
  }
  if(millis()-lastSample<1000) return;lastSample=millis();
  uint32_t sum=0;unsigned count=0;
  analogReadResolution(12);analogSetPinAttenuation(pins::battery,ADC_11db);
  for(int i=0;i<6;i++) {unsigned mv=analogReadMilliVolts(pins::battery);if(mv>=1200 && mv<=2500) {sum+=mv;count++;}}
  if(count) batteryMv=2*sum/count;
  uint8_t status=0,data[6];
  if(compassOk && read(Wire,0x7c,9,&status,1) && (status&1) && !(status&2) && read(Wire,0x7c,1,data,6)) {
    for(int i=0;i<3;i++) mag[i]=int16_t(uint16_t(data[2*i])|(uint16_t(data[2*i+1])<<8))/1000.0f;compassSample=true;
  }
  if(imuOk && read(Wire,0x6b,0x2e,&status,1) && (status&1) && read(Wire,0x6b,0x35,data,6)) {
    for(int i=0;i<3;i++) accel[i]=int16_t(uint16_t(data[2*i])|(uint16_t(data[2*i+1])<<8))/16384.0f;imuSample=true;
  }
  if(!keyboardOk && millis()-lastProbe>2000) {lastProbe=millis();keyboardOk=probeKeyboard();if(keyboardOk&&gpsEnabled)setGps(true);}
}
int Hardware::readKey() {
  static uint32_t poll=0;if(millis()-poll<15 || !keyboardOk) return 0;poll=millis();
  uint8_t key=0;if(!read(Wire1,keyboardAt,1,&key,1,true)) return 0;
  if(key==0 || key==255) return 0;lastKey=key;keyCount++;brightness(config.brightness);return key;
}
// Off: the backlight and the screen, GPS and sensor supply off (both active LOW), the LR1110 deselected.
// The M9 has no button on a GPIO: RESET or the power slider turns it on.
void Hardware::powerDown(bool started) {
  if(started){ledcDetachPin(pins::backlight);setGps(false);}
  const int levels[][2]={{pins::backlight,HIGH},{pins::peripheralPower,HIGH},{pins::radioCs,HIGH}};
  for(auto& l:levels){gpio_hold_dis(gpio_num_t(l[0]));pinMode(l[0],OUTPUT);digitalWrite(l[0],l[1]);gpio_hold_en(gpio_num_t(l[0]));}
}
int Hardware::wakePin() const {return -1;}
void Hardware::beep() {if(config.sound) {tone(pins::buzzer,2200,60);}}
void Hardware::ping(uint16_t hz,uint16_t ms) {if(config.sound) {tone(pins::buzzer,hz,ms);}}
void Hardware::flush() {flushChangedRows(*this);}
void Hardware::text(int x,int y,const String& value,uint16_t color) {font.setForegroundColor(color);font.setCursor(x,y);font.print(value);}
void Hardware::line(int y,const String& value,uint16_t color) {text(12,y,value,color);}
