#if defined(MM_NRF52)
#pragma GCC optimize("Os")
#endif
#include "QuickSend.h"
#include "MeshRadio.h"
#include "Hardware.h"
#include "BlobStore.h"
#include "I18n.h"
namespace quickSend {
State state;
namespace {constexpr uint32_t Magic=0x4d4d5101;
void defaults(State& s){s.to=meshmesh::Broadcast;s.gps=false;memset(s.text,0,sizeof s.text);const char* text[]={"OK","ping",tr("Got it","Принято"),tr("On my way","Иду"),tr("Arrived","На месте"),tr("Need help","Нужна помощь"),tr("Wait for me","Подождите меня"),tr("All clear","Всё в порядке"),tr("Running late","Опаздываю"),tr("Battery low","Батарея садится")};for(unsigned i=0;i<Count;i++)strlcpy(s.text[i],text[i],sizeof s.text[i]);}
bool hexId(const String& raw){if(!raw.length()||raw.length()>16)return false;for(unsigned i=0;i<raw.length();i++)if(!isxdigit(uint8_t(raw[i])))return false;return true;}
bool valid(const State& s){if(!s.to)return false;for(const auto& text:s.text){size_t n=strnlen(text,sizeof text);if(n==sizeof text||!meshmesh::validUtf8(reinterpret_cast<const uint8_t*>(text),n))return false;}return true;}}
void begin(){if(!blob::load("/meshmesh/quick",Magic,state)||!valid(state))defaults(state);}
bool targetValid(uint64_t to){if(channels::isChannel(to))return meshRadio.channel(to)!=nullptr;const Peer* p=meshRadio.findContact(to);return p&&(p->type==1||p->type==3)&&to!=meshRadio.nodeId;}
unsigned targetCount(){unsigned n=meshRadio.channelCount;for(unsigned i=0;i<meshRadio.peerCount;i++)if(targetValid(meshRadio.peers[i].id))n++;return n;}
uint64_t target(unsigned index){if(index<meshRadio.channelCount)return meshRadio.channelList[index].id;index-=meshRadio.channelCount;for(unsigned i=0;i<meshRadio.peerCount;i++)if(targetValid(meshRadio.peers[i].id)){if(!index)return meshRadio.peers[i].id;index--;}return 0;}
String targetName(){if(auto* c=meshRadio.channel(state.to))return c->name;if(const Peer* p=meshRadio.findContact(state.to))return p->name;return String(tr("Choose recipient","Выберите получателя"));}
String send(unsigned index,uint64_t to){if(index>=Count)return "ERR quick index 0..9";if(!to)to=state.to;if(!targetValid(to))return "ERR quick recipient unavailable: choose a channel or contact";String text=state.text[index];
 if(state.gps){if(!hardware.clockTrusted||hardware.clockConflict||!hardware.gpsFix()||!hardware.gpsTime())return "ERR trusted GPS position unavailable";text+=" @"+String(hardware.gps.location.lat(),5)+","+String(hardware.gps.location.lng(),5);}
 if(!text.length()||text.length()>meshRadio.messageLimit(to))return "ERR quick text exceeds recipient UTF-8 byte limit";
 return meshRadio.sendMessage(text,to)?"OK quick message queued":"ERR quick send: queue full or radio offline";
}
String json(){DynamicJsonDocument d(2560);d["to"]=meshRadio.idText(state.to);d["name"]=targetName();d["valid"]=targetValid(state.to);d["gps"]=state.gps;d["limit"]=meshRadio.messageLimit(state.to);JsonArray a=d.createNestedArray("presets");for(const auto& text:state.text)a.add(text);String out;serializeJson(d,out);return out;}
String command(JsonObjectConst values){String action=values["action"]|"";std::unique_ptr<State> draft(new(std::nothrow) State(state));if(!draft)return "ERR quick memory";State& next=*draft;
 if(action=="send"){if(!values["index"].is<unsigned>()||values["index"].as<unsigned>()>=Count)return "ERR quick index 0..9";uint64_t to=0;if(values.containsKey("to")){String raw=values["to"]|"";char* end=nullptr;to=raw=="ALL"?meshmesh::Broadcast:strtoull(raw.c_str(),&end,16);if(!to||(raw!="ALL"&&(!end||*end||!hexId(raw))))return "ERR quick recipient";}return send(values["index"],to);}
 if(action=="reset")defaults(next);
 else if(action=="set"){if(!values["index"].is<unsigned>()||values["index"].as<unsigned>()>=Count||!values["text"].is<const char*>())return "ERR quick index and text";String text=values["text"].as<String>();if(text.length()>TextBytes)return "ERR quick text up to 160 UTF-8 bytes";strlcpy(next.text[values["index"].as<unsigned>()],text.c_str(),TextBytes+1);}
 else if(action=="target"){String raw=values["to"]|"";char* end=nullptr;next.to=raw=="ALL"?meshmesh::Broadcast:strtoull(raw.c_str(),&end,16);if(!next.to||(raw!="ALL"&&(!end||*end||!hexId(raw)))||!targetValid(next.to))return "ERR quick recipient unavailable";}
 else if(action=="gps"){if(!values["enabled"].is<bool>())return "ERR quick GPS boolean";next.gps=values["enabled"];}
 else return "ERR quick action set|target|send|gps|reset";
 if(!valid(next))return "ERR quick UTF-8 text";if(!blob::save("/meshmesh/quick",Magic,next))return "ERR quick storage";state=next;meshRadio.dirty=true;return "OK quick settings saved";
}
}
