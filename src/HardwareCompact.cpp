// One-button boards other than Heltec V4: Heltec V3 and Wireless Tracker, LilyGO T-Beam,
// T-Beam Supreme, T3-S3 and T-LoRa, XIAO ESP32S3 + Wio-SX1262, Station G2, ThinkNode M2.
// The interface draws a 128x64 canvas (UiHeltec.cpp); this file puts it on the board's
// screen, powers the peripherals and reads the button, battery and GPS.
#include "Hardware.h"
#include "Config.h"
#include "GpsProbe.h"
#include "Storage.h"
#include <LittleFS.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/rtc_io.h>
#if defined(MM_PANEL_SSD1306)
#include <Adafruit_SSD1306.h>
#elif defined(MM_PANEL_SH1106)
#include <Adafruit_SH110X.h>
#elif defined(MM_PANEL_ST7735)
#include <Adafruit_ST7735.h>
#endif
#if defined(MM_PMU)
#include <XPowersLib.h>
#endif
Hardware hardware;
namespace {
#if defined(MM_PANEL_SSD1306)
Adafruit_SSD1306 oled(128,64,&Wire,pins::oledReset);
#elif defined(MM_PANEL_SH1106)
Adafruit_SH1106G oled(128,64,&Wire,pins::oledReset);
#elif defined(MM_PANEL_ST7735)
// Wireless Tracker: 0.96" 160x80 ST7735 on its own SPI bus. The 128x64 canvas is centred.
SPIClass tftBus(HSPI);
class TrackerTft:public Adafruit_ST7735 {
 public:
  TrackerTft():Adafruit_ST7735(&tftBus,pins::tftCs,pins::tftDc,pins::tftReset) {}
  void begin() {initR(INITR_MINI160x80_PLUGIN);setColRowStart(26,1);setRotation(1);} // Heltec's panel offsets
};
TrackerTft tft;
#endif
#if defined(MM_PMU)
XPowersLibInterface* pmu=nullptr;
#endif
bool adcActive=HIGH;
#if defined(MM_BOARD_HELTEC_TRACKER)
GpsProbe gpsPort(pins::gpsRx,pins::gpsTx,115200);
#else
GpsProbe gpsPort(pins::gpsRx,pins::gpsTx,9600);
#endif
bool probe(TwoWire& bus,uint8_t address) {bus.beginTransmission(address);return bus.endTransmission()==0;}
void output(int pin,int level) {if(pin<0)return;gpio_hold_dis(gpio_num_t(pin));pinMode(pin,OUTPUT);digitalWrite(pin,level);}
#if defined(MM_PMU)
// Rails as in MeshCore's TBeamBoard: AXP192 (T-Beam up to v1.1) or AXP2101 (T-Beam v1.2, Supreme).
void gpsRail(bool on) {
  if(!pmu)return;
  uint8_t rail=pmu->getChipModel()==XPOWERS_AXP192?XPOWERS_LDO3:
#if defined(MM_BOARD_TBEAM_SUPREME)
    XPOWERS_ALDO4;
#else
    XPOWERS_ALDO3;
#endif
  if(on){pmu->setPowerChannelVoltage(rail,3300);pmu->enablePowerOutput(rail);}else pmu->disablePowerOutput(rail);
}
void beginPmu() {
#if defined(MM_BOARD_TBEAM_SUPREME)
  TwoWire& bus=Wire1;
#else
  TwoWire& bus=Wire;
#endif
  pmu=new XPowersAXP2101(bus,pins::pmuSda,pins::pmuScl,AXP2101_SLAVE_ADDRESS);
  if(!pmu->init()){delete pmu;pmu=new XPowersAXP192(bus,pins::pmuSda,pins::pmuScl,AXP192_SLAVE_ADDRESS);if(!pmu->init()){delete pmu;pmu=nullptr;Serial.println("ERR PMU not found");return;}}
  if(pmu->getChipModel()==XPOWERS_AXP192) {
    pmu->setPowerChannelVoltage(XPOWERS_LDO2,3300);pmu->enablePowerOutput(XPOWERS_LDO2);   // LoRa
    pmu->setPowerChannelVoltage(XPOWERS_DCDC1,3300);pmu->enablePowerOutput(XPOWERS_DCDC1); // OLED
    pmu->setProtectedChannel(XPOWERS_DCDC1);pmu->setProtectedChannel(XPOWERS_DCDC3);       // ESP32
    pmu->disablePowerOutput(XPOWERS_DCDC2);
  } else {
#if defined(MM_BOARD_TBEAM_SUPREME)
    pmu->setPowerChannelVoltage(XPOWERS_ALDO3,3300);pmu->enablePowerOutput(XPOWERS_ALDO3); // LoRa
    pmu->setPowerChannelVoltage(XPOWERS_DCDC3,3300);pmu->enablePowerOutput(XPOWERS_DCDC3); // M.2
    if(esp_reset_reason()==ESP_RST_POWERON) {   // restart the sensors, OLED and SD card
      for(uint8_t rail:{XPOWERS_ALDO1,XPOWERS_ALDO2,XPOWERS_BLDO1})pmu->disablePowerOutput(rail);delay(250);
    }
    for(uint8_t rail:{XPOWERS_ALDO1,XPOWERS_ALDO2,XPOWERS_BLDO1,XPOWERS_BLDO2}){pmu->setPowerChannelVoltage(rail,3300);pmu->enablePowerOutput(rail);}
    for(uint8_t rail:{XPOWERS_DCDC2,XPOWERS_DLDO1,XPOWERS_DLDO2})pmu->disablePowerOutput(rail);
#else
    for(uint8_t rail:{XPOWERS_DCDC2,XPOWERS_DCDC3,XPOWERS_DCDC4,XPOWERS_DCDC5,XPOWERS_ALDO1,XPOWERS_ALDO4,XPOWERS_BLDO1,XPOWERS_BLDO2,XPOWERS_DLDO1,XPOWERS_DLDO2})pmu->disablePowerOutput(rail);
    pmu->setPowerChannelVoltage(XPOWERS_VBACKUP,3300);pmu->enablePowerOutput(XPOWERS_VBACKUP); // GPS backup
    pmu->setPowerChannelVoltage(XPOWERS_ALDO2,3300);pmu->enablePowerOutput(XPOWERS_ALDO2);     // LoRa
#endif
  }
  pmu->disableIRQ(pmu->getChipModel()==XPOWERS_AXP192?uint64_t(XPOWERS_AXP192_ALL_IRQ):uint64_t(XPOWERS_AXP2101_ALL_IRQ));pmu->clearIrqStatus();
  pmu->disableTSPinMeasure();pmu->enableBattVoltageMeasure();pmu->enableVbusVoltageMeasure();pmu->enableSystemVoltageMeasure();
  delay(20); // rails settle before the radio and the screen start
}
#endif
}
void Hardware::brightness(uint8_t level) {
#if defined(MM_PANEL_SSD1306)
  if(!panel)return;if(!level){oled.ssd1306_command(SSD1306_DISPLAYOFF);return;}oled.ssd1306_command(SSD1306_DISPLAYON);oled.ssd1306_command(SSD1306_SETCONTRAST);oled.ssd1306_command(level);
#elif defined(MM_PANEL_SH1106)
  if(!panel)return;if(!level){oled.oled_command(SH110X_DISPLAYOFF);return;}oled.oled_command(SH110X_DISPLAYON);oled.setContrast(level);
#elif defined(MM_PANEL_ST7735)
  ledcWrite(6,level);
#else
  (void)level;
#endif
}
void Hardware::setGps(bool enabled) {
  gpsEnabled=enabled&&pins::gpsRx>=0;if(pins::gpsRx<0)return;
  output(pins::gpsEnable,enabled?pins::gpsOn:!pins::gpsOn);output(pins::gpsReset,HIGH);
#if defined(MM_PMU)
  gpsRail(enabled);
#endif
  if(enabled)gpsPort.open(gps);else gpsPort.close();
}
void Hardware::begin() {
  gpio_deep_sleep_hold_dis();
  // Peripheral power: a short off period resets screens that kept their state over a reset.
  if(pins::vext>=0){output(pins::vext,!pins::vextOn);delay(20);digitalWrite(pins::vext,pins::vextOn);delay(120);}
#if defined(MM_PMU)
  beginPmu();
#endif
  output(pins::radioCs,HIGH);
  SPI.begin(pins::spiClock,pins::spiMiso,pins::spiMosi);
  if(pins::sda>=0){Wire.begin(pins::sda,pins::scl,400000);Wire.setTimeOut(20);}
#if defined(MM_PANEL_SSD1306) || defined(MM_PANEL_SH1106)
  uint8_t address=probe(Wire,0x3c)?0x3c:probe(Wire,0x3d)?0x3d:0;
#if defined(MM_PANEL_SSD1306)
  if(address&&oled.begin(SSD1306_SWITCHCAPVCC,address,true,false))panel=&oled;
#else
  if(address&&oled.begin(address,true))panel=&oled;
#endif
  if(panel){oled.clearDisplay();oled.display();}
#elif defined(MM_PANEL_ST7735)
  output(pins::tftLight,LOW);tftBus.begin(pins::tftClock,-1,pins::tftData,pins::tftCs);tft.begin();tft.fillScreen(0);panel=&tft;
  ledcSetup(6,5000,8);ledcAttachPin(pins::tftLight,6);
#endif
  brightness(config.brightness);
  canvas=new GFXcanvas16(128,64);if(!canvas || !canvas->getBuffer()) {Serial.println("FATAL framebuffer");while(true)delay(1000);}
  font.begin(*canvas);font.setFont(u8g2_font_6x13_t_cyrillic);font.setFontMode(1);canvas->fillScreen(0);text(0,13,"MeshMesh");flush();
  pinMode(pins::button,INPUT_PULLUP);
  if(pins::adcEnable>=0){pinMode(pins::adcEnable,INPUT);delay(2);adcActive=!digitalRead(pins::adcEnable);output(pins::adcEnable,!adcActive);} // the enable level differs between Heltec revisions
  if(pins::buzzer>=0)output(pins::buzzer,LOW);
  setGps(config.gps);fsOk=mountStorage();
  Serial.printf("HW %s screen=%d pmu=%d psram=%u fs=%d\n",MM_BOARD_NAME,panel!=nullptr,
#if defined(MM_PMU)
    pmu!=nullptr,
#else
    0,
#endif
    ESP.getPsramSize(),fsOk);
}
void Hardware::tick() {
  if(gpsEnabled) {
    gpsBytes+=gpsPort.tick(gps);
  }
  if(millis()-lastSample<1000)return;lastSample=millis();
#if defined(MM_PMU)
  if(pmu&&pmu->isBatteryConnect())batteryMv=pmu->getBattVoltage();else batteryMv=0;
#endif
#if !defined(MM_EMULATOR) // QEMU builds: the emulator has no ADC and a conversion never completes
  if(pins::battery>=0) {
    if(pins::adcEnable>=0)digitalWrite(pins::adcEnable,adcActive);
    analogReadResolution(12);analogSetPinAttenuation(pins::battery,ADC_11db);
    uint32_t sum=0;for(int i=0;i<6;i++)sum+=analogReadMilliVolts(pins::battery);batteryMv=uint16_t((sum/6)*pins::batteryScale);
    if(pins::adcEnable>=0)digitalWrite(pins::adcEnable,!adcActive);
  }
#endif
  if(gps.date.isValid() && gps.time.isValid() && gps.time.age()<10000 && gps.date.age()<10000 && gps.location.isValid() && gps.location.age()<10000 && gps.date.year()>=2025) {
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
void Hardware::beep() {if(pins::buzzer>=0&&config.sound)tone(pins::buzzer,2200,60);}
void Hardware::ping(uint16_t hz,uint16_t ms) {if(pins::buzzer>=0&&config.sound)tone(pins::buzzer,hz,ms);}
void Hardware::flush() {
#if defined(MM_PANEL_SSD1306) || defined(MM_PANEL_SH1106)
  if(!panel)return;oled.clearDisplay();for(int y=0;y<64;y++)for(int x=0;x<128;x++)if(canvas->getPixel(x,y))oled.drawPixel(x,y,1);oled.display();
#elif defined(MM_PANEL_ST7735)
  tft.drawRGBBitmap(16,8,canvas->getBuffer(),128,64);
#endif
}
void Hardware::text(int x,int y,const String& value,uint16_t color) {font.setForegroundColor(color);font.setCursor(x,y);font.print(value);}
void Hardware::line(int y,const String& value,uint16_t color) {text(0,y,value,color);}
