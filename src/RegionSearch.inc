// Region search (include/Regions.h), included at the end of MeshRadio.cpp after Remote.inc: a zero-hop node
// discovery for repeaters, then each one that answered gets the anonymous "regions" request (zero hop, the reply
// comes back the same way) and lists the regions it passes on, as the MeshCore app's region search.
// The repeaters need not be contacts: MeshCoreBackend opens their answers with the keys kept here (peers()).
struct RegionAccess {
 static MeshCoreBackend* core(){return meshRadio.core;}
};
namespace regions {
Search search;
namespace {
enum:uint8_t {CtlNodeDiscoverReq=0x80,CtlNodeDiscoverResp=0x90,AnonReqRegions=0x01};
// A busy repeater queues its answer behind its own traffic and airtime budget: late answers still count for a while.
// One that stays silent is asked once more (a stock repeater takes 4 anonymous requests per 3 minutes).
constexpr uint32_t ListenMs=8000,AskEveryMs=3000,LastWaitMs=12000,LateMs=30000;
// The repeaters asked (tags of both attempts): on the heap from the first search, as Search::found.
struct Scan {uint8_t keys[MaxRepeaters][32];bool answered[MaxRepeaters];uint32_t tags[MaxRepeaters][2];};
Scan* scan=nullptr;uint32_t discoverTag=0,due=0,doneAt=0;uint8_t asked=0;bool rediscovered=false;
MeshCoreBackend* core(){return RegionAccess::core();}
bool waiting(){return scan&&(search.state==Search::Asking||(search.state==Search::Done&&doneAt));}
void note(const char* name){
 String n=normalize(name);if(!n.length()||n!=name)return; // "*" (unscoped floods) and odd names are not regions to choose
 for(unsigned i=0;i<search.count;i++)if(n==search.found[i].name){search.found[i].repeaters++;return;}
 if(search.count<MaxFound){auto& f=search.found[search.count++];strlcpy(f.name,name,sizeof(f.name));f.repeaters=1;}
}
void finish(){search.state=Search::Done;discoverTag=0;doneAt=millis();meshRadio.event="Regions found: "+String(search.count);meshRadio.dirty=true;}
// The request goes to the key alone (zero hop, as the repeater heard us), not to a contact.
// Node discovery of repeaters, zero hop (a new tag each time: repeaters drop a packet they have seen).
bool discover(){
 uint8_t data[10]={CtlNodeDiscoverReq,uint8_t(1<<ADV_TYPE_REPEATER)};coreRandom.random(data+2,4);memcpy(&discoverTag,data+2,4); // since: 0, every repeater
 mesh::Packet* p=core()->createControlData(data,sizeof(data));if(!p)return false;core()->sendZeroHop(p);return true;
}
bool ask(uint8_t i,uint8_t attempt){
 ContactInfo to={};to.id=mesh::Identity(scan->keys[i]);to.out_path_len=0;to.type=ADV_TYPE_REPEATER;
 const uint8_t req[2]={AnonReqRegions,0}; // the reply path: none, it heard us directly
 uint32_t timeout;return core()->sendAnonReq(to,req,sizeof(req),scan->tags[i][attempt],timeout)!=MSG_SEND_FAILED;
}
}
bool find(){
 auto* d=core();if(!d||!meshRadio.ready||!config.bootCounter)return false;
 if(search.state==Search::Listening||search.state==Search::Asking)return true;
 if(!scan){scan=new(std::nothrow) Scan;search.found=new(std::nothrow) Found[MaxFound];if(!scan||!search.found){delete scan;scan=nullptr;delete[] search.found;search.found=nullptr;return false;}}
 Found* list=search.found;search=Search();search.found=list;memset(scan,0,sizeof(Scan));asked=0;doneAt=0;rediscovered=false;
 if(!discover())return false;
 search.state=Search::Listening;search.at=millis();due=millis()+ListenMs;meshRadio.event="Regions: searching";meshRadio.dirty=true;return true;
}
void tick(){
 uint32_t now=millis();
 if(doneAt&&now-doneAt>=LateMs)doneAt=0;
 if(search.state==Search::Listening&&int32_t(now-due)>=0){ // nobody answered: the discovery goes out once more
  if(!search.repeaters&&!rediscovered){if(meshRadio.busy())return;rediscovered=true;if(discover()){due=now+ListenMs;return;}}
  if(!search.repeaters){finish();return;}search.state=Search::Asking;due=now;}
 if(search.state!=Search::Asking||int32_t(now-due)<0)return;
 unsigned n=search.repeaters;while(asked<2*n&&scan->answered[asked%n])asked++; // the second round: the silent ones only
 if(asked<2*n){if(meshRadio.busy())return;ask(asked%n,asked/n);asked++;
  unsigned k=asked;while(k<2*n&&k!=n&&scan->answered[k%n])k++;
  due=now+(k==n||k>=2*n?LastWaitMs:AskEveryMs);meshRadio.dirty=true;return;}
 finish();
}
// The repeaters' answers to the discovery; false: not this search's (the app may get it).
bool onControl(const mesh::Packet* p){
 if((p->payload[0]&0xf0)!=CtlNodeDiscoverResp||!discoverTag||p->payload_len<6)return false;
 uint32_t tag;memcpy(&tag,p->payload+2,4);if(tag!=discoverTag)return false;
 if(search.state!=Search::Listening||(p->payload[0]&0x0f)!=ADV_TYPE_REPEATER||p->payload_len<6+32)return true;
 const uint8_t* key=p->payload+6;if(core()&&!memcmp(key,core()->self_id.pub_key,32))return true;
 for(uint8_t i=0;i<search.repeaters;i++)if(!memcmp(scan->keys[i],key,32))return true;
 if(search.repeaters<MaxRepeaters){memcpy(scan->keys[search.repeaters++],key,32);meshRadio.dirty=true;}
 return true;
}
// The repeaters asked whose hash a packet names (for MeshCoreBackend::searchPeersByHash); their keys.
int peers(const uint8_t* hash,uint8_t* out,int max){
 int n=0;if(!waiting())return 0;
 for(uint8_t k=0;k<search.repeaters&&n<max;k++)if(scan->tags[k][0]&&mesh::Identity(scan->keys[k]).isHashMatch(hash))out[n++]=k;
 return n;
}
const uint8_t* peerKey(uint8_t i){return scan&&i<search.repeaters?scan->keys[i]:nullptr;}
// A repeater's list: tag, its clock, then the names it floods, separated by commas ("*" for unscoped floods).
bool onAnswer(const uint8_t* key,const uint8_t* data,uint8_t len){
 if(!waiting()||len<8)return false;bool late=search.state==Search::Done;
 uint32_t tag;memcpy(&tag,data,4);int i=-1;
 for(uint8_t k=0;k<search.repeaters;k++)if(tag&&(scan->tags[k][0]==tag||scan->tags[k][1]==tag)&&!memcmp(scan->keys[k],key,32)){i=k;break;}
 if(i<0)return false;if(scan->answered[i])return true; // the answer to the other attempt
 scan->answered[i]=true;search.answered++;
 char list[MAX_PACKET_PAYLOAD]={};memcpy(list,data+8,min<size_t>(len-8,sizeof(list)-1));
 for(char* name=strtok(list,",");name;name=strtok(nullptr,","))note(name);
 if(late){meshRadio.event="Regions found: "+String(search.count);meshRadio.dirty=true;}
 else if(search.answered>=search.repeaters)finish();else meshRadio.dirty=true;
 return true;
}
bool onResponse(const ContactInfo& c,const uint8_t* data,uint8_t len){return onAnswer(c.id.pub_key,data,len);}
String json(){
 StaticJsonDocument<1024> d;const char* states[]={"idle","listening","asking","done"};d["state"]=states[search.state];
 d["repeaters"]=search.repeaters;d["answered"]=search.answered;if(search.at)d["age"]=(millis()-search.at)/1000;
 JsonArray a=d.createNestedArray("found");for(unsigned i=0;i<search.count;i++){JsonObject o=a.createNestedObject();o["name"]=search.found[i].name;o["repeaters"]=search.found[i].repeaters;}
 String s;serializeJson(d,s);return s;
}
}
