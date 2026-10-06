#include "Hardware.h"
#include "Config.h"
#include "OledLevel.h"
#include <LittleFS.h>
#include <driver/rtc_io.h>
#include <sys/time.h>
Hardware hardware;
int heltecFemTx=46;
// Level 0 switches the OLED panel off (no static image left to burn in); any other level dims it.
void Hardware::brightness(uint8_t level) {if(!level){display.ssd1306_command(SSD1306_DISPLAYOFF);return;}display.ssd1306_command(SSD1306_DISPLAYON);oledLevel(level,false,[this](uint8_t c){display.ssd1306_command(c);});}
void Hardware::setGps(bool enabled) {
  gpsEnabled=enabled;pinMode(pins::gpsEnable,OUTPUT);digitalWrite(pins::gpsEnable,enabled?LOW:HIGH);
  if(enabled)Serial1.begin(9600,SERIAL_8N1,pins::gpsRx,pins::gpsTx);else Serial1.end();
}
void Hardware::begin() {
  for(int pin:{pins::peripheralPower,pins::femPower,pins::femEnable}) {gpio_hold_dis(gpio_num_t(pin));if(pin<=21)rtc_gpio_hold_dis(gpio_num_t(pin));}
  gpio_deep_sleep_hold_dis();
  pinMode(pins::peripheralPower,OUTPUT);digitalWrite(pins::peripheralPower,LOW);
  pinMode(pins::femPower,OUTPUT);digitalWrite(pins::femPower,HIGH);delay(5);
  // GC1109 CSD pulls down, KCT8103L CSD pulls up. V4.3 and R8 use CTX GPIO5.
  pinMode(pins::femEnable,INPUT);delay(2);
  bool kct=digitalRead(pins::femEnable)==HIGH;
#if defined(MM_HELTEC_R8)
  kct=true;
#endif
  heltecFemTx=kct?5:46;
  pinMode(pins::femEnable,OUTPUT);digitalWrite(pins::femEnable,HIGH);
  pinMode(heltecFemTx,OUTPUT);digitalWrite(heltecFemTx,LOW);
  pinMode(pins::radioCs,OUTPUT);digitalWrite(pins::radioCs,HIGH);
  SPI.begin(pins::spiClock,pins::spiMiso,pins::spiMosi);
  Wire.begin(pins::sda,pins::scl,400000);Wire.setTimeOut(20);
  display.begin(SSD1306_SWITCHCAPVCC,0x3c,true,false);display.clearDisplay();display.setRotation(0);brightness(config.brightness);
  canvas=new GFXcanvas16(128,64);if(!canvas || !canvas->getBuffer()) {Serial.println("FATAL framebuffer");while(true)delay(1000);}
  font.begin(*canvas);font.setFont(u8g2_font_6x13_t_cyrillic);font.setFontMode(1);canvas->fillScreen(0);text(0,13,"MeshMesh");flush();
  pinMode(pins::button,INPUT_PULLUP);
  if(pins::adcEnable>=0) {pinMode(pins::adcEnable,OUTPUT);digitalWrite(pins::adcEnable,HIGH);}
  setGps(config.gps);fsOk=LittleFS.begin(false,"/littlefs",10,"littlefs");
  Serial.printf("HW Heltec FEM=%s tx_pin=%d psram=%u fs=%d\n",kct?"KCT8103L":"GC1109",heltecFemTx,ESP.getPsramSize(),fsOk);
}
void Hardware::tick() {
  if(gpsEnabled) {unsigned budget=1024;while(Serial1.available() && budget--){gps.encode(Serial1.read());gpsBytes++;}}
  if(millis()-lastSample<1000)return;lastSample=millis();
  analogReadResolution(12);analogSetPinAttenuation(pins::battery,ADC_0db);
  uint32_t sum=0;for(int i=0;i<6;i++)sum+=analogReadMilliVolts(pins::battery);batteryMv=uint16_t((sum/6)*4.9f);
  if(gpsTime()) {
    DateTime now(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());setUtc(now.unixtime(),"GPS");
  }
}
int Hardware::readKey() {
  static bool held=false,longSent=false;static uint32_t down=0;
  bool pressed=digitalRead(pins::button)==LOW;
  if(pressed && !held) {held=true;longSent=false;down=millis();}
  if(pressed && held && !longSent && millis()-down>=1200) {longSent=true;keyCount++;lastKey=0xa3;return 0xa3;}
  if(!pressed && held) {held=false;if(!longSent && millis()-down>30){keyCount++;lastKey=13;return 13;}}
  return 0;
}
void Hardware::beep() {} // Standard V4 board has no buzzer.
void Hardware::ping(uint16_t,uint16_t) {}
void Hardware::flush() {
  display.clearDisplay();for(int y=0;y<64;y++)for(int x=0;x<128;x++)if(canvas->getPixel(x,y))display.drawPixel(x,y,SSD1306_WHITE);display.display();
}
void Hardware::text(int x,int y,const String& value,uint16_t color) {font.setForegroundColor(color);font.setCursor(x,y);font.print(value);}
void Hardware::line(int y,const String& value,uint16_t color) {text(0,y,value,color);}
