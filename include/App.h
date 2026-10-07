#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"
String statusJson();
String messagesJson();
String nodesJson();
// Joined channels and those heard on air; private keys (in "link") only when secrets is true.
String channelsJson(bool secrets);
String channelCommand(JsonObjectConst request); // "channel do {JSON}" and POST /api/channels
String configJson(bool includeKey=false);
String applySettings(JsonObjectConst values);
String executeCommand(const String& line);
// Role changes restart the device; the restart waits for the reply to leave over USB, Wi-Fi or BLE.
const char* roleName(uint8_t role);String setRole(uint8_t role);void restartTick();
void uiBegin();void uiTick();void uiKey(int key);bool uiScreenOff();
void uiFarewell(); // power off (Power.cpp): "the device is off" and how to turn it on, the last frame before the screen goes dark
// Touch (T-Deck; "uitouch" over USB on the 320x240 boards): 't' tap and 'h' hold at x,y; 'u','d','l','r' swipes.
void uiTouch(char gesture,int x,int y);
String uiStatus();
void portalBegin();void portalTick();void portalToggle();bool portalActive();String portalPassword();
String connectionCredentials();
void bleToggle();bool bleActive();
// Radar holders: the web page (Portal.cpp) and the screen pages (Ui.cpp / UiHeltec.cpp).
bool webRadarActive();bool uiRadarPage();
String webRadarCommand(const String& line); // "radar web" and "radar do {JSON}"

uint32_t blePin();
