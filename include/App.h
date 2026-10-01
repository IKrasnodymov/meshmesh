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
// Role changes restart the device; the restart waits for the reply to leave over USB, Wi-Fi or BLE.
const char* roleName(uint8_t role);String setRole(uint8_t role);void restartTick();
void uiBegin();void uiTick();void uiKey(int key);
String uiStatus();
void portalBegin();void portalTick();void portalToggle();bool portalActive();String portalPassword();
String connectionCredentials();
void bleToggle();bool bleActive();
// Radar holders: the web page (Portal.cpp) and the screen pages (Ui.cpp / UiHeltec.cpp).
bool webRadarActive();bool uiRadarPage();
String webRadarCommand(const String& line); // "radar web" and "radar do {JSON}"

uint32_t blePin();
