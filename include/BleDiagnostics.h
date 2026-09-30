#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
String startBleProbe(JsonObjectConst options);
String bleProbeResult();
bool bleProbeActive();
