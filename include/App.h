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
// Apps of the screen menu (Apps.cpp): uiApps lists the IDs of this interface (Ui.cpp, UiHeltec.cpp) in the
// default order; config.apps holds the chosen order and folders ("apps" setting): "-" before an ID hides it,
// "{NAME id id}" is a folder ("_" for a space in the name, "games" the games folder), "-{NAME ...}" hides it.
constexpr unsigned AppsMax=24,FoldersMax=6,FolderNameMax=24; // FolderNameMax: bytes with the final 0
constexpr unsigned AppEntriesMax=AppsMax+FoldersMax;
constexpr uint8_t AppFolder=0x80; // an entry of the menu: an index into uiApps, or AppFolder|folder
struct AppMenu { // plain data: AppMenu() is all zero
  uint8_t top[AppEntriesMax];uint8_t topCount;
  struct Folder{char name[FolderNameMax];bool hidden;uint8_t apps[AppsMax];uint8_t count;} folder[FoldersMax];uint8_t folders;
  bool hidden[AppsMax];
};
extern const char* const uiApps[];extern const uint8_t uiAppCount;
bool appsParse(const char* text,AppMenu& menu,bool strict); // strict: unknown or repeated IDs, bad folders fail
String appsText(const char* text); // every ID once, in the chosen order and folders
const AppMenu& appsMenu();         // config.apps parsed
// The shown entries (AppEntriesMax) at the top (folder -1) or in a folder: apps, and the folders with a shown app.
unsigned appsShown(uint8_t* out,int folder=-1);
int appsFolderOf(uint8_t app);     // the folder holding an app, or -1
String appsFolderName(uint8_t folder); // as shown: spaces, the games folder in the interface language
bool appsOldDefault(const char* text); // the saved order is only a default before 0.24.0 (Config::load resets it once)
void appsAddGames(char* text,size_t cap); // 0.33.0: the games of an order saved before go to the games folder (once)
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
// The advertised name, "MeshCore-<node name>" as on stock firmware: MeshCore apps find the board by it.
String bleName();void bleRename(); // a new node name: advertised at once
#if defined(MM_NRF52)
void bleSilence(); // power off: Bluetooth quiet, the setting kept
#endif
// Radar holders: the web page (Portal.cpp) and the screen pages (Ui.cpp / UiHeltec.cpp).
bool webRadarActive();bool uiRadarPage();
String webRadarCommand(const String& line); // "radar web" and "radar do {JSON}"

uint32_t blePin();
