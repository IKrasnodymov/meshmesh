#pragma once
#include <Arduino.h>
// MeshCore group channels: names, keys, invitation links and the QR code of a link.
// No radio here: the screens, USB commands and the host preview share these rules.
namespace channels {
constexpr unsigned Max=8;      // Public and seven more (MAX_GROUP_CHANNELS)
constexpr unsigned NameBytes=31;
// region: its flood scope (Regions.h) - "" the default region of the settings, "*" none, else a region name.
struct Policy {uint8_t led=3,wake=3,popup=3,priority=1,deviceLimit=0,appLimit=0;}; // 3 inherit; limits 0 default
struct Channel {uint64_t id=0;char name[NameBytes+1]={};uint8_t secret[16]={};char region[31]={};Policy policy{};};
// Public: the stock MeshCore channel every node knows; its messages keep the destination "ALL".
extern const uint8_t publicSecret[16];
// Channel destinations in the chat history: 0xFF in the top byte (node keys never start with it,
// MeshCore reserves that hash); Public is meshmesh::Broadcast.
inline bool isChannel(uint64_t id){return (id>>56)==0xFF;}
uint64_t idOf(const uint8_t secret[16]);
uint8_t hashOf(const uint8_t secret[16]); // the byte group packets carry
bool isPublic(const uint8_t secret[16]);
// "#name" as MeshCore apps derive it: without leading '#' and spaces, Latin letters in lower case;
// empty when nothing is left or the name is too long. Key: the first 16 bytes of SHA-256("#name").
String hashtag(const String& raw);
void hashtagSecret(const String& tag,uint8_t out[16]);
bool isHashtag(const Channel& c); // its key is the one its "#name" gives
bool validName(const String& name); // 1..31 bytes of UTF-8 without control characters
// 32 hexadecimal digits or base64 of 16 bytes (MeshCore keys are 128-bit).
bool parseKey(String text,uint8_t out[16]);
String keyHex(const uint8_t secret[16]);
// meshcore://channel/add?name=<url-encoded>&secret=<32 hex>[&region_scope=<region>] (docs.meshcore.io/qr_codes), the format
// of the MeshCore app; found anywhere in a text, so a received invitation works as well. compact keeps
// non-ASCII letters as they are (a Cyrillic name takes a third of the room): for invitations that would
// not fit a message otherwise; MeshMesh reads both.
String link(const Channel& c,bool compact=false);
bool parseLink(const String& text,String& name,uint8_t secret[16],String* region=nullptr); // region: its region_scope, raw
bool hasLink(const String& text);
// Hashtags tried against heard packets of channels this node has not joined.
extern const char* const commonTags[];
extern const unsigned commonTagCount;
// QR code of a short text (byte mode, error correction M, versions 1-10): side 21..57, 0 when too long.
// Modules row by row, a bit each (1 = dark); out holds at least 57*57/8+1 bytes.
int qr(const String& text,uint8_t* out);
inline bool qrDark(const uint8_t* m,int size,int x,int y){int i=y*size+x;return m[i>>3]>>(i&7)&1;}
constexpr size_t QrBytes=57*57/8+1;
}
