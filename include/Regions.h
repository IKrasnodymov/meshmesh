#pragma once
#include <Arduino.h>
// MeshCore regions (flood scope): a flood packet may carry a transport code made with a region's key; repeaters
// configured with "region def" / "region put" + "allowf" pass on only the regions they serve (and unscoped floods
// while "allowf *" stays). The names and keys share the settings, channels, screens and the host preview; the search
// runs in MeshRadio.cpp (src/RegionSearch.inc).
namespace regions {
constexpr unsigned NameBytes=30; // RegionEntry::name[31] of MeshCore
// A region name as repeaters keep it: without a leading '#', letters (any case, also non-Latin), digits, '-' and
// the other characters RegionMap::is_name_char takes; "" when nothing valid is left. Case matters, as on repeaters.
// '$' (a private region) has no key of its name and is refused; "*" means "no region" and is no name either.
String normalize(const String& raw);
// The region's key: the first 16 bytes of SHA-256("#name"), as RegionMap's implicit hashtag regions.
void keyOf(const String& name,uint8_t key[16]);
// Channel setting: "" - the default region of the settings, "*" - none (an ordinary flood), else a region name.
constexpr const char* Unscoped="*";
// Search: repeaters that hear this node directly (MeshCore node discovery, zero hop) are asked which regions they
// pass on (an anonymous "regions" request, as the MeshCore app's region search). Normal mode only.
constexpr unsigned MaxFound=12,MaxRepeaters=8;
struct Found {char name[NameBytes+1]={};uint8_t repeaters=0;};
// found: MaxFound entries on the heap from the first search (the classic ESP32 boards have no static DRAM to spare).
struct Search {
  enum State:uint8_t {Idle,Listening,Asking,Done} state=Idle;
  uint8_t repeaters=0,answered=0,count=0;Found* found=nullptr;uint32_t at=0; // at: millis of the start
};
extern Search search;
bool find();   // starts a search (up to about a minute; late answers count 30 s more); false without a radio in the normal mode
void tick();
String json(); // {"state","repeaters","answered","found":[{"name","repeaters"}],"age"}
inline bool searching(){return search.state==Search::Listening||search.state==Search::Asking;}
// The next choice on a screen: none ("") - for a channel also "*" - then the regions found, then the value itself
// when it is none of these (typed or set by an app).
String cycle(const String& value,int dir,bool channel);
}
