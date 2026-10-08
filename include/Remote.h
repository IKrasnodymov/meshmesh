#pragma once
#include <Arduino.h>
// Repeaters and rooms of the mesh from this node's screen and web page (src/Remote.inc in MeshRadio.cpp): login,
// status, CLI commands and path trace, as the MeshCore apps do. Room posts arrive in the chat history as the
// room's conversation; posting is an ordinary message to the room. Passwords that logged in are kept
// (/meshmesh/remote.bin) and used again; an empty password is the guest login.
namespace remote {
enum State:uint8_t {Idle,Waiting,Done,Refused,Silent}; // Refused: login only (a wrong password gets no answer: Silent)
struct Session {
  uint64_t id=0;
  State login=Idle,status=Idle,command=Idle;bool admin=false;uint32_t loginAt=0,statusAt=0,commandAt=0,due=0;
  // The last status: battery mV, uptime s, packets, airtime s, queue, noise floor and last RSSI dBm, SNR x4, room posts.
  uint16_t battery=0,queue=0,posted=0;int16_t noise=0,rssi=0,snr=0;uint32_t uptime=0,received=0,sent=0,airtime=0;bool room=false;
  char reply[3][81]={};uint8_t replies=0; // the last CLI replies, newest first
  char password[16]={}; // the one tried last: saved when the login succeeds
  uint8_t statusTries=0; // a lost status answer is asked once more, as the MeshCore apps do
};
// A path trace through the repeaters of the route and back: each hop's 1-byte hash and the SNR it heard (x4);
// the last value is this node's reception.
struct Trace {uint64_t id=0;State state=Idle;uint32_t at=0,tag=0,due=0;uint8_t hops=0,hashes[16]={};int8_t snr[17]={};};
constexpr unsigned MaxSessions=3;
extern Session sessions[MaxSessions];extern Trace trace;
Session* find(uint64_t id);
bool login(uint64_t id,const String& password,bool useSaved=true); // useSaved: an empty password takes the saved one
bool status(uint64_t id);
bool command(uint64_t id,const String& line); // admin only
bool traceTo(uint64_t id);
bool traceable(uint64_t id); // a route through repeaters, or a repeater itself
bool saved(uint64_t id);     // a password that logged in is kept
void tick();                 // answers that do not come
String json();               // "remote" over USB and the web page
String usbCommand(const String& line); // "remote ..." USB commands
}
