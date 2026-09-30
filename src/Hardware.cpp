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
bool Hardware::read(TwoWire& bus,uint8_t addr,uint8_t reg,uint8_t* bytes,uint8_t size,bool stop) {
  bus.beginTransmission(addr);bus.write(reg);if(bus.endTransmission(stop)) return false;
  if(bus.requestFrom(int(addr),int(size))!=size) return false;
  for(unsigned i=0;i<size;i++) bytes[i]=bus.read();return true;
}
bool Hardware::write(uint8_t addr,uint8_t reg,uint8_t value) {
  Wire.beginTransmission(addr);Wire.write(reg);Wire.write(value);return Wire.endTransmission()==0;
}
void Hardware::brightness(uint8_t level) {ledcWrite(7,255-level);}
void Hardware::setGps(bool enabled) {
  gpsEnabled=enabled;
  digitalWrite(pins::gpsEnable,enabled?LOW:HIGH);
  digitalWrite(pins::gpsReset,LOW);
  if(enabled) {Serial1.begin(115200,SERIAL_8N1,pins::gpsRx,pins::gpsTx);lastGpsBaud=millis();}
  else Serial1.end();
}
void Hardware::begin() {
  REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG,USB_SERIAL_JTAG_USB_PAD_ENABLE);
  gpio_deep_sleep_hold_dis();
  for(int pin:{pins::backlight,pins::peripheralPower}) {rtc_gpio_hold_dis(gpio_num_t(pin));pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
  for(int pin:{pins::radioCs,pins::lcdCs,pins::sdCs}) {gpio_hold_dis(gpio_num_t(pin));pinMode(pin,OUTPUT);digitalWrite(pin,HIGH);}
  pinMode(pins::gpsEnable,OUTPUT);pinMode(pins::gpsReset,OUTPUT);pinMode(pins::buzzer,OUTPUT);
  digitalWrite(pins::buzzer,LOW);setGps(config.gps);
  SPI.begin(pins::spiClock,pins::spiMiso,pins::spiMosi);
  display.init(240,320,SPI_MODE0);display.setSPISpeed(40000000);display.setRotation(1);display.fillScreen(0x0862);
  ledcSetup(7,5000,8);ledcAttachPin(pins::backlight,7);brightness(config.brightness);
  canvas=new GFXcanvas16(320,240); // Arduino ESP32 allocates this large buffer from PSRAM when available.
  if(!canvas || !canvas->getBuffer()) {Serial.println("FATAL framebuffer allocation");while(true) delay(1000);}
  font.begin(*canvas);font.setFont(u8g2_font_6x13_t_cyrillic);font.setFontMode(1);
  canvas->fillScreen(0x0862);text(14,38,MESHMM_FIRMWARE,0x07ff);text(14,65,"Starting ThinkNode M9...");flush();
  Wire.begin(pins::sda,pins::scl,100000);Wire.setTimeOut(20);
  Wire1.begin(pins::keyboardSda,pins::keyboardScl,100000);Wire1.setTimeOut(20);
  keyboardOk=read(Wire1,pins::keyboardAddress,0,&keyboardHw,1,true);
  if(keyboardOk) read(Wire1,pins::keyboardAddress,0xfe,&keyboardFw,1,true);
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
  Serial.printf("HW keyboard=%d hw=%02x fw=%02x rtc=%d valid=%d sd=%d fs=%d compass=%d imu=%d psram=%u\n",keyboardOk,keyboardHw,keyboardFw,rtcOk,rtcValid,sdOk,fsOk,compassOk,imuOk,ESP.getPsramSize());
}
void Hardware::tick() {
  if(gpsEnabled) {
    unsigned budget=1024;
    while(Serial1.available() && budget--) {gps.encode(Serial1.read());gpsBytes++;}
    static bool fallback=false;
    if(!gpsBytes && !fallback && millis()-lastGpsBaud>6000) {Serial1.updateBaudRate(9600);fallback=true;}
    if(gps.date.isValid() && gps.time.isValid() && gps.time.isUpdated() && gps.time.age()<10000 && gps.date.age()<10000 && gps.location.isValid() && gps.location.age()<10000 && gps.date.year()>=2025) {
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
  if(!keyboardOk && millis()-lastProbe>2000) {lastProbe=millis();keyboardOk=read(Wire1,pins::keyboardAddress,0,&keyboardHw,1,true);}
}
int Hardware::readKey() {
  static uint32_t poll=0;if(millis()-poll<15 || !keyboardOk) return 0;poll=millis();
  uint8_t key=0;if(!read(Wire1,pins::keyboardAddress,1,&key,1,true)) return 0;
  if(key==0 || key==255) return 0;lastKey=key;keyCount++;brightness(config.brightness);return key;
}
void Hardware::beep() {if(config.sound) {tone(pins::buzzer,2200,60);}}
void Hardware::flush() {display.drawRGBBitmap(0,0,canvas->getBuffer(),320,240);}
void Hardware::text(int x,int y,const String& value,uint16_t color) {font.setForegroundColor(color);font.setCursor(x,y);font.print(value);}
void Hardware::line(int y,const String& value,uint16_t color) {text(12,y,value,color);}
