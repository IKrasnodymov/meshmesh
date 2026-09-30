#include "Navigation.h"
#include "Hardware.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <math.h>
Navigation navigation;
void Navigation::begin(){Preferences p;if(!p.begin("meshmesh-nav",true))return;calibrated=p.getBool("cal",false);if(p.getBytesLength("min")==sizeof(minimum))p.getBytes("min",minimum,sizeof(minimum));else calibrated=false;if(p.getBytesLength("max")==sizeof(maximum))p.getBytes("max",maximum,sizeof(maximum));else calibrated=false;p.end();}
void Navigation::start(){calibrating=true;calibrated=false;samples=0;headingValid=false;for(int i=0;i<3;i++){minimum[i]=INFINITY;maximum[i]=-INFINITY;}}
bool Navigation::finish(){if(!calibrating||samples<20||maximum[0]-minimum[0]<.1f||maximum[1]-minimum[1]<.1f)return false;Preferences p;if(!p.begin("meshmesh-nav",false))return false;bool ok=p.putBytes("min",minimum,sizeof(minimum))==sizeof(minimum)&&p.putBytes("max",maximum,sizeof(maximum))==sizeof(maximum)&&p.putBool("cal",true)==sizeof(bool);p.end();if(ok){calibrating=false;calibrated=true;}return ok;}
void Navigation::tick(){static uint32_t at=0;if(millis()-at<1000)return;at=millis();headingValid=false;if(!hardware.compassSample)return;
 if(calibrating){for(int i=0;i<3;i++){minimum[i]=min(minimum[i],hardware.mag[i]);maximum[i]=max(maximum[i],hardware.mag[i]);}samples++;}
 if(calibrated){float x=(hardware.mag[0]-(maximum[0]+minimum[0])/2)/(maximum[0]-minimum[0]),y=(hardware.mag[1]-(maximum[1]+minimum[1])/2)/(maximum[1]-minimum[1]);float flat=sqrtf(hardware.accel[0]*hardware.accel[0]+hardware.accel[1]*hardware.accel[1]);if(isfinite(x)&&isfinite(y)&&flat<.35f){heading=fmodf(atan2f(y,x)*180/M_PI+360,360);headingValid=true;}}
}
String Navigation::info(){StaticJsonDocument<512>d;d["available"]=hardware.compassOk;d["calibrated"]=calibrated;d["calibrating"]=calibrating;d["samples"]=samples;d["heading_valid"]=headingValid;if(headingValid)d["magnetic_heading"]=heading;d["note"]="Magnetic heading; hold device flat. Calibration and axis orientation need a physical rotation check.";String s;serializeJson(d,s);return s;}
