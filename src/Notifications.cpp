#if defined(MM_NRF52)
#pragma GCC optimize("Os")
#endif
#include "Notifications.h"
#include "Config.h"
#include "I18n.h"
namespace notifications {
namespace {struct AwaitEcho {uint32_t due=0;ChatMessage message;};AwaitEcho echoWait[8];Event queue[8];unsigned head=0,size=0;uint32_t lost=0;
// UTF-8 case folding for Latin and Cyrillic names; other scripts retain their code points.
String lower(const char* text){String s=text,out;for(unsigned i=0;i<s.length();){uint32_t c=utf8Next(s,i);
 if(c>='A'&&c<='Z')c+=32;else if(c>=0x410&&c<=0x42f)c+=32;else if(c==0x401)c=0x451;else if(c>=0xc0&&c<=0xde&&c!=0xd7)c+=32;
 if(c<128)out+=char(c);else if(c<2048){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}else if(c<65536){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}}
 return out;}
}
bool mentioned(const char* text,const char* name,const char* words){String s=lower(text),n=lower(name);if(n.length()&&s.indexOf("@["+n+"]")>=0)return true;
 String list=lower(words);unsigned at=0;while(at<list.length()){int end=list.indexOf(',',at);if(end<0)end=list.length();String w=list.substring(at,end);w.trim();if(w.length()&&s.indexOf(w)>=0)return true;at=end+1;}return false;}
void emit(Kind kind,const ChatMessage& message){
 if(kind==Transmitted&&channels::isChannel(message.destination)){AwaitEcho* slot=nullptr;for(auto& e:echoWait)if(!e.due){slot=&e;break;}if(!slot){slot=&echoWait[0];lost++;}slot->message=message;slot->due=millis()+30000;}
 if(kind==Repeat)for(auto& e:echoWait)if(e.message.source==message.source&&e.message.session==message.session&&e.message.id==message.id)e.due=0;if(size==8){head=(head+1)%8;size--;lost++;}Event& e=queue[(head+size++)%8];e={};e.kind=kind;e.message=message;e.mention=kind==Incoming&&mentioned(message.text,config.name,config.notifyWords);}
void repeatPacket(uint32_t packet){if(packet)for(auto& e:echoWait)if(e.due&&e.message.packet==packet)e.due=0;}
void incoming(const ChatMessage& message){emit(Incoming,message);}
void tick(){uint32_t now=millis();for(auto& e:echoWait)if(e.due&&int32_t(now-e.due)>=0){e.due=0;emit(Unconfirmed,e.message);}}
bool take(Event& event){if(!size)return false;event=queue[head];head=(head+1)%8;size--;return true;}
uint32_t dropped(){return lost;}
bool enabled(uint8_t mode,bool mention){return mode==All||(mode==Mentions&&mention);}
uint8_t mode(const ChatMessage& message,uint8_t global,unsigned effect){const auto* c=meshRadio.channel(message.destination);if(!c)return global;uint8_t own=effect==0?c->policy.led:effect==1?c->policy.wake:c->policy.popup;return own==Inherit?global:own;}
}
