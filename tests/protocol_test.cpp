#include <Mm1Packet.h>
#include <cassert>
#include <cstdio>
#include <random>
using namespace meshmesh;
int main() {
  uint8_t packet[MaxPacket]={},aad[35],iv[16];
  Header h;h.source=0x68ee8f5cea14;h.destination=0x123456abcdef;h.session=19;h.sequence=371;h.length=180;h.network=1234;
  encodeHeader(h,packet);Header got;
  assert(decodeHeader(packet,MaxPacket,got));assert(got.source==h.source && got.destination==h.destination && got.sequence==371 && got.length==180);
  for(size_t size=0;size<MaxPacket;size++)assert(!decodeHeader(packet,size,got));
  authenticatedHeader(packet,aad);nonce(h,iv);assert(get64(iv)==h.source && get32(iv+8)==19 && get32(iv+12)==371);
  packet[33]=1;uint8_t forwarded[35];authenticatedHeader(packet,forwarded);assert(!memcmp(aad,forwarded,35));assert(decodeHeader(packet,MaxPacket,got));
  packet[33]=4;assert(!decodeHeader(packet,MaxPacket,got));packet[33]=0;
  packet[35]=1;assert(!decodeHeader(packet,MaxPacket,got));packet[35]=0;
  packet[3]=5;assert(!decodeHeader(packet,MaxPacket,got));packet[3]=1;
  const char* valid[]={"Hi","Привет, мир!","\xf0\x9f\x98\x80","line\nline"};
  for(auto p:valid)assert(validUtf8((const uint8_t*)p,strlen(p)));
  const char* invalid[]={"\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82","\x80","\x1b"};
  for(auto p:invalid)assert(!validUtf8((const uint8_t*)p,strlen(p)));
  const char* ru="Я";assert(previousCharacter(ru,2)==0);assert(previousCharacter("aЯ",3)==1);assert(previousCharacter("",0)==0);
  SeenCache seen;assert(!seen.contains(h));seen.remember(h);assert(seen.contains(h));h.session++;assert(!seen.contains(h));h.session--;h.source++;assert(!seen.contains(h));
  // Parser robustness on arbitrary truncated/corrupted radio input, under ASan/UBSan.
  std::mt19937 rng(17);for(int i=0;i<100000;i++){for(auto& b:packet)b=uint8_t(rng());decodeHeader(packet,rng()%(MaxPacket+1),got);validUtf8(packet,rng()%(MaxPacket+1));}
  puts("PASS: packet bounds, wire roundtrip, routing AAD, UTF-8, dedup, 100000 malformed inputs");
}
