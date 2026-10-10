#if defined(MM_NRF52)
#pragma GCC optimize("Os")
#endif
#include "ChannelPolicy.h"
#include "MeshRadio.h"
#include "BlobStore.h"
namespace channelPolicy {
namespace {struct Entry {uint64_t id=0;channels::Policy policy{};};struct Saved {Entry entries[channels::Max];};constexpr uint32_t Magic=0x4d4d5001;
bool valid(const channels::Policy& p){return p.led<=3&&p.wake<=3&&p.popup<=3&&p.priority<=2&&(p.deviceLimit==0||p.deviceLimit==8||p.deviceLimit==16)&&(p.appLimit==0||p.appLimit==8||p.appLimit==16||p.appLimit==32);}}
void load(){Saved s{};if(!blob::load("/meshmesh/channel-policy",Magic,s))return;for(const auto& e:s.entries){int i=meshRadio.channelIndex(e.id);if(i>=0&&valid(e.policy))meshRadio.channelList[i].policy=e.policy;}}
String set(uint64_t id,JsonObjectConst values){int index=meshRadio.channelIndex(id);if(index<0)return "ERR unknown channel";channels::Policy next=meshRadio.channelList[index].policy;
 for(JsonPairConst pair:values){String key=pair.key().c_str();if(key=="action"||key=="channel")continue;JsonVariantConst value=pair.value();if(!value.is<int>()||value.as<int>()<0||value.as<int>()>255)return "ERR policy integer required";uint8_t n=value.as<int>();
 if(key=="led")next.led=n;else if(key=="wake")next.wake=n;else if(key=="popup")next.popup=n;else if(key=="priority")next.priority=n;else if(key=="device_limit")next.deviceLimit=n;else if(key=="app_limit")next.appLimit=n;else return "ERR unknown policy setting";}
 if(!valid(next))return "ERR policy: modes 0..3, priority 0..2, limits 0|8|16 (app also 32)";
 Saved saved{};for(unsigned i=0;i<meshRadio.channelCount;i++){saved.entries[i].id=meshRadio.channelList[i].id;saved.entries[i].policy=i==unsigned(index)?next:meshRadio.channelList[i].policy;}
 if(!blob::save("/meshmesh/channel-policy",Magic,saved))return "ERR storage: channel policy not saved";
 meshRadio.channelList[index].policy=next;meshRadio.trimHistory();meshRadio.dirty=true;return "OK channel policy saved";
}
}
