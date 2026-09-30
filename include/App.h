#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"
String statusJson();
String messagesJson();
String nodesJson();
String configJson(bool includeKey=false);
String applySettings(JsonObjectConst values);
String executeCommand(const String& line);
void uiBegin();void uiTick();void uiKey(int key);
String uiStatus();
void portalBegin();void portalTick();void portalToggle();bool portalActive();String portalPassword();
String connectionCredentials();
void bleToggle();bool bleActive();

uint32_t blePin();
