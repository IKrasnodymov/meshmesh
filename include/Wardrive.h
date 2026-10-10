#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Modules.h"
// Wardriving (docs/wardrive.md): a log of where the mesh is heard. It records only with a trusted position: a GPS
// fix (gpsFix: a date past the build, no conflict with the phone/NTP clock) or the phone's position sent by the app
// within the last 30 s. Three parts, each switched on its own:
// - passive: the packets this node hears, with the last repeater of a flood packet (or the sender of a zero-hop
//   advert) and the signal; the same repeater is logged again only after 1/4 of the ping distance or 2 minutes;
// - ping: a short message to a channel (#wardrive, joined on the first ping) every `distance` metres, at most once
//   per `interval` seconds; the copies repeaters pass on within 30 s are its echoes (MeshRadio::echo), and none means
//   no repeater heard us there. The message carries the coordinates only with `coords`: the channel then learns them;
// - nets (ESP32): the radar's Wi-Fi access points and Bluetooth devices, a MAC again after 100 m or 5 minutes; the
//   radar stays open meanwhile (Wi-Fi then waits for the access point, the probe and the internet client).
// Each log is two files of MeshMesh storage, the current one and the previous one; a full file becomes the previous.
// Records wait in RAM for up to 16 or 30 s. Nothing of the log goes on air.
namespace wardrive {
enum Kind:uint8_t {Rx,Ping,Echo};
enum Flag:uint8_t {FromPhone=1,SenderKey=2,Direct=4};
// time: UTC (0 while the clock is unset); position in 1e-6 degrees; snr in quarter dB. Rx: payload type, hops and
// the last hop's hash (SenderKey: the advert's key prefix); Ping: count repeaters, the best one's hash and SNR;
// Echo: one repeater that passed a ping on, at the ping's position.
struct Point {uint32_t time;int32_t lat,lon;uint8_t kind,type,hops,size;int8_t snr,rssi;uint8_t hash[3],count,flags,spare;};
// A Wi-Fi access point (kind 0: auth mode in `auth`) or a Bluetooth device (kind 1); name: SSID or advertised name.
struct Net {uint32_t time;int32_t lat,lon;uint8_t mac[6],kind,channel;int8_t rssi;uint8_t auth,flags;char name[33];};
static_assert(sizeof(Point)==24&&sizeof(Net)==56,"log record sizes are part of the file format");
struct Settings {bool passive=false,ping=false,coords=false,nets=false;uint16_t distance=200,interval=60;uint64_t channel=0;};
extern Settings settings;
// The last finished ping: when, repeaters heard, the best one.
struct Result {uint32_t at=0;uint8_t count=0,size=0,hash[3]={};int8_t snr=-128;bool done=false;};
// Repeaters heard while it runs (RAM, this boot): by passive packets and as ping echoes.
struct Repeater {uint8_t hash[3],size;int8_t snr;uint16_t rx,echoes;uint32_t at;};
// The newest points for the maps: kind, position and quality (best SNR; -128: a ping nobody repeated).
struct Recent {int32_t lat,lon;int8_t snr;uint8_t kind;};
void begin();void tick();void flush();
bool running();bool holdsRadar();
bool position(int32_t& lat,int32_t& lon,bool& phone);
// USB, web and the app: status, settings and actions, and the log in parts.
String json();String command(JsonObjectConst values);String log(const String& args);
bool ping(); // now, regardless of distance; false while one is under way or without a position
const Result& last();bool pinging();
unsigned repeaters(const Repeater*& out);unsigned recent(const Recent*& out);
uint32_t points();uint32_t nets();
// MeshRadio: every packet received (hash: the start of its packet hash) and the copies of our ping.
void heard(uint32_t hash,uint8_t type,bool flood,uint8_t pathLen,const uint8_t* path,const uint8_t* payload,size_t length,float snr,float rssi);
void echo(uint32_t packet,uint8_t pathLen,const uint8_t* path,float snr);
void probeSent(uint32_t id,uint32_t hash);void probeFailed(uint32_t id);
// Radar (ESP32): a Wi-Fi access point of a sweep, a Bluetooth advertisement.
void wifi(const uint8_t* mac,const char* ssid,uint8_t channel,uint8_t auth,int rssi);
void ble(uint64_t address,const char* name,int rssi);
}
