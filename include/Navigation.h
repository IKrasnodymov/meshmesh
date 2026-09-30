#pragma once
#include <Arduino.h>
class Navigation {
 public:
  bool calibrating=false,calibrated=false;float heading=0;bool headingValid=false;
  uint32_t samples=0;float minimum[3],maximum[3];
  void begin();void tick();void start();bool finish();String info();
};
extern Navigation navigation;
