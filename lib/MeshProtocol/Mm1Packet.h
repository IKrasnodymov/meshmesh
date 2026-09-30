#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace meshmesh {
constexpr size_t HeaderSize=36, TagSize=16, MaxPayload=180, MaxPacket=232;
constexpr uint64_t Broadcast=UINT64_MAX;
enum class Kind:uint8_t { Message=1, Ack=2, Hello=3, Position=4 };
struct Header {
  Kind kind=Kind::Message;
  uint32_t network=0, session=0, sequence=0;
  uint64_t source=0, destination=Broadcast;
  uint8_t maxHops=3, hops=0, length=0;
};
inline void put32(uint8_t* p,uint32_t n) { for(int i=0;i<4;i++) p[i]=uint8_t(n>>(8*i)); }
inline uint32_t get32(const uint8_t* p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
inline void put64(uint8_t* p,uint64_t n) {put32(p,uint32_t(n));put32(p+4,uint32_t(n>>32));}
inline uint64_t get64(const uint8_t* p) {return uint64_t(get32(p))|(uint64_t(get32(p+4))<<32);}
inline void encodeHeader(const Header& h,uint8_t* p) {
  p[0]='M'; p[1]='M'; p[2]=1; p[3]=uint8_t(h.kind);
  put32(p+4,h.network); put64(p+8,h.source); put64(p+16,h.destination);
  put32(p+24,h.session); put32(p+28,h.sequence);
  p[32]=h.maxHops; p[33]=h.hops; p[34]=h.length; p[35]=0;
}
inline bool decodeHeader(const uint8_t* p,size_t size,Header& h) {
  if(size<HeaderSize+TagSize || p[0]!='M' || p[1]!='M' || p[2]!=1 || p[3]<1 || p[3]>4 || p[35]!=0) return false;
  if(p[34]>MaxPayload || size!=HeaderSize+p[34]+TagSize || p[32]>7 || p[33]>p[32]) return false;
  h.kind=Kind(p[3]); h.network=get32(p+4); h.source=get64(p+8); h.destination=get64(p+16);
  h.session=get32(p+24); h.sequence=get32(p+28); h.maxHops=p[32]; h.hops=p[33]; h.length=p[34];
  return h.source!=0 && h.source!=Broadcast && h.sequence!=0 && h.session!=0;
}
inline void nonce(const Header& h,uint8_t* p) { put64(p,h.source); put32(p+8,h.session); put32(p+12,h.sequence); }
inline void authenticatedHeader(const uint8_t* header,uint8_t* aad) {
  memcpy(aad,header,33); memcpy(aad+33,header+34,2); // only current hop count changes in transit
}
inline bool validUtf8(const uint8_t* p,size_t n) {
  for(size_t i=0;i<n;) {
    uint8_t a=p[i++];
    if(a<0x80) { if(a<0x20 && a!='\n' && a!='\t') return false; continue; }
    unsigned k; uint32_t cp, minimum;
    if(a>=0xc2 && a<=0xdf) {k=1;cp=a&31;minimum=0x80;}
    else if(a>=0xe0 && a<=0xef) {k=2;cp=a&15;minimum=0x800;}
    else if(a>=0xf0 && a<=0xf4) {k=3;cp=a&7;minimum=0x10000;}
    else return false;
    if(i+k>n) return false;
    while(k--) {uint8_t b=p[i++]; if((b&0xc0)!=0x80) return false; cp=(cp<<6)|(b&63);}
    if(cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return false;
  }
  return true;
}
inline size_t previousCharacter(const char* p,size_t size) {
  if(!size) return 0;
  size_t i=size-1; while(i>0 && (uint8_t(p[i])&0xc0)==0x80) --i; return i;
}
struct SeenCache {
  struct Entry {uint64_t source=0;uint32_t session=0,sequence=0;} entries[128];
  size_t next=0;
  bool contains(const Header& h) const {
    for(const auto& e:entries) if(e.source==h.source && e.session==h.session && e.sequence==h.sequence) return true;
    return false;
  }
  void remember(const Header& h) {entries[next]={h.source,h.session,h.sequence}; next=(next+1)%128;}
};
}
