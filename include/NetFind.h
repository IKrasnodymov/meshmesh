#pragma once
#include <Arduino.h>
#include "Config.h"
// Network search (docs/networks.md): which radio profile of the networks catalogue (NetCatalog.h, built in from
// site/networks.json) has MeshCore repeaters within reach. The board tunes to each candidate in turn without saving
// it (MeshRadio::tune), sends a zero-hop node discovery for repeaters (as the region search) and listens 8 s: the
// repeaters answer, other packets heard count too. Then the saved profile comes back. A profile found is applied
// only by the user, as a test (radio try). Normal mode only; while it runs our messages wait, relaying is off and
// the network of the saved profile is not heard.
// Automatic (net_auto): with a trusted position (a GPS fix, the phone's position from the app) within 150 km of a
// catalogue place whose networks do not include the current profile, the search of the nearby places' profiles
// runs by itself, once per place and boot; a network found is offered on the screen.
namespace netfind {
constexpr unsigned MaxResults=32;
// One candidate: the profile, its catalogue place (255: none, e.g. the current one), the repeaters that answered,
// the other packets heard and the best SNR of the answers (quarter dB, -128: none).
struct Result {RadioProfile radio;uint8_t place=255,repeaters=0;uint16_t packets=0;int8_t snr=-128;bool current=false,done=false;};
struct State {
  enum Phase:uint8_t {Idle,Running,Done} phase=Idle;
  bool automatic=false,located=false; // located: the candidates are the places near the position, nearest first
  uint8_t count=0,step=0,best=255;    // best: the result to offer (never the current profile), 255 - none
  uint16_t found=0;                   // grows when an automatic search finds a network: the screens tell it once
  Result* results=nullptr;            // MaxResults on the heap from the first search
};
extern State state;
// all: every profile of the catalogue, the most common first; else the places near the position (lat/lon given or
// the board's own) and, without one, every profile too. False: not in the normal mode, the radio busy or offline.
bool start(bool all,const float* lat=nullptr,const float* lon=nullptr);
void stop();     // the saved profile comes back; the results so far stay
void cancel();   // applyConfig: the settings are applied anew, nothing to restore
inline bool running(){return state.phase==State::Running;}
void tick();
String json();
String placeName(uint8_t place);
String profileText(const RadioProfile& r); // "868.731 SF7 BW62.5"
String use(uint8_t i);                       // radio try with that result: the reply of the command
}
