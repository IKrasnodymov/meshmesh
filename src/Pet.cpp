#define PET_SPRITES_DEFINE // the species drawings live in this file (PetSprites.h)
#include "Pet.h"
#include "Config.h"
#include "Hardware.h"
#include "MeshRadio.h"
#include "ChessRating.h"
#include <ArduinoJson.h>
#if !defined(MM_UI_PREVIEW)
#include <esp_system.h>
#endif
#include <math.h>
#include <time.h>
pet::Pet creature;
namespace pet {
namespace {
const char* const PetFile="/meshmesh/pet.bin";const char* const PetTemp="/meshmesh/pet.tmp";
// The egg (its spots in the colour of the species inside) and the grave: 'o' outline, 'b' shell or stone, 'l' spots or cross.
const char* const SpriteEgg[Size]={
 "................","................","......oooo......",".....obbbbo.....","....obblbbbo....","....obbbbbbo....",
 "...obbbbbbllo...","...obllbbbblo...","...obllbbbbbo...","...obbbbblbbo...","...obbbbbbbbo...","...obbllbbbbo...",
 "....obbbbbbo....",".....oooooo.....","................","................"};
const char* const SpriteGrave[Size]={
 "................","................","................",".....oooooo.....","....obbbbbbo....","...obbbllbbbo...",
 "...obbbllbbbo...","...obllllllbo...","...obllllllbo...","...obbbllbbbo...","...obbbllbbbo...","...obbbbbbbbo...",
 "...obbbbbbbbo...",".oooooooooooooo.",".obbbbbbbbbbbbo.",".oooooooooooooo."};
const char* const names[]={"Ping","Bitty","Lora","Pixel","Blip","Nibble","Echo","Hopper","Chirp","Spark","Byte","Mote","Pip","Zuzu","Taro","Kiwi","Momo","Bolt","Nori","Fizz"};
#if defined(MM_UI_PREVIEW)
uint32_t entropy(){return rand();}
#else
uint32_t entropy(){return esp_random();} // the hardware generator: each egg its own species
#endif
uint32_t fnv(const uint8_t* p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
uint32_t mix(uint64_t v){uint32_t h=uint32_t(v^(v>>32))*2654435761u;return h^(h>>15);}
String t(const char* en,const char* ru){return tr(en,ru);}
}
uint16_t clampPoints(int v){return v<0?0:v>Full?Full:v;}

void Pet::begin(){
  State* stored=new State;
  bool ok=readStored(PetFile,PetTemp,stored,sizeof(State))&&stored->version==1&&stored->check==fnv((const uint8_t*)stored,offsetof(State,check))
    &&stored->stage<StageCount&&stored->species<Species&&stored->graveCount<=Graves&&stored->friendNext<FriendSlots&&memchr(stored->name,0,sizeof stored->name);
  if(ok)s=*stored;
  else{s=State();s.species=entropy()%Species;hatchName();}
  delete stored;lastTick=millis();lastSave=lastTick;events++;
}
void Pet::hatchName(){strlcpy(s.name,names[entropy()%(sizeof names/sizeof *names)],sizeof s.name);}
unsigned Pet::level() const{unsigned n=1;while(n<99&&s.xp>=levelXp(n+1))n++;return n;}
bool Pet::asleep() const{
  if(!MeshRadio::clockSet()||!alive()||s.stage==Egg)return false;
  time_t now=time(nullptr)+config.utcOffset*60;tm v=*gmtime(&now);return v.tm_hour>=23||v.tm_hour<7;
}
Mood Pet::mood() const{
  if(!alive())return Gone;if(s.stage==Egg)return Content;if(s.health<400)return Sick;
  if(effect==Food&&millis()-effectAt<3000)return Eating;
  if(asleep())return Sleepy;if(s.food<250)return Hungry;if(s.joy<250)return Lonely;
  return s.joy>=650&&s.food>=450?Happy:Content;
}
uint32_t Pet::hatchLeft() const{return s.stage==Egg&&s.age<900?900-s.age:0;}
bool Pet::needsCare() const{return alive()&&s.stage!=Egg&&(s.food<250||s.joy<250||s.health<400);}
void Pet::say(const String& text,Effect e){speech=text;speechAt=millis();chatAt=speechAt;if(e!=NoEffect){effect=e;effectAt=speechAt;}events++;}
void Pet::gain(int food,int joy,uint32_t xp){
  if(!alive()||s.stage==Egg)return;
  unsigned before=level();s.food=clampPoints(s.food+food);s.joy=clampPoints(s.joy+joy);s.xp+=xp;
  unsigned after=level();if(after>before)say(t("Level ","Уровень ")+String(after),Sparkle);
  // The stage follows the level, but no faster than the age allows: a busy repeater does not grow up in an hour.
  uint8_t byLevel=after>=10?Adult:after>=6?Teen:after>=3?Child:Baby,byAge=s.age>=3*86400?Adult:s.age>=86400?Teen:s.age>=7200?Child:Baby;
  uint8_t next=min(byLevel,byAge);if(next>s.stage){s.stage=next;say(t("I grew up: ","Я подрос: ")+stageName(next),Sparkle);save(true);}
  events++;
}
// Time with the device on: hunger and boredom grow, health follows them; at night it sleeps and needs less.
void Pet::elapse(uint32_t seconds){
  if(!alive()||!seconds)return;s.age+=seconds;
  if(s.stage==Egg){if(s.age>=900){s.stage=Baby;s.food=s.joy=800;say(t("Hello! I am ","Привет! Я ")+String(s.name),Sparkle);save(true);}events++;return;}
  bool sleeping=asleep(),server=config.role!=RoleNormal;
  auto drain=[&](uint16_t& value,uint32_t& rest,uint32_t perHour){rest+=perHour*seconds;uint32_t points=rest/3600;rest%=3600;value=clampPoints(int(value)-int(min<uint32_t>(points,Full)));};
  drain(s.food,foodRest,sleeping?15:40);drain(s.joy,joyRest,sleeping?0:server?12:30);
  int hurt=(s.food==0?15:0)+(s.joy==0?15:0);
  if(hurt)drain(s.health,healthRest,hurt);
  else if(s.food>=300&&s.joy>=300){healthRest+=25*seconds;s.health=clampPoints(s.health+int(min<uint32_t>(healthRest/3600,Full)));healthRest%=3600;}
  // A crumb found in the air every six hours: a quiet network with a caring owner still feeds it.
  crumbRest+=seconds;if(crumbRest>=6*3600){crumbRest-=6*3600;if(s.snacks<9){s.snacks++;say(t("Found a crumb in the air","Нашёл крошку в эфире"));}}
  // No owner in the server roles: it eats a snack itself when hungry.
  if(server&&s.food<250&&s.snacks){s.snacks--;s.food=clampPoints(s.food+300);say(t("Yum","Ням"),Food);}
  if(s.health==0){if(s.mortal)die();else s.health=1;}
  events++;
}
void Pet::die(){
  Grave g={};strlcpy(g.name,s.name,sizeof g.name);g.age=s.age;g.level=level();g.cause=s.food==0?Hunger:Loneliness;g.species=s.species;
  memmove(s.graves+1,s.graves,sizeof(Grave)*(Graves-1));s.graves[0]=g;if(s.graveCount<Graves)s.graveCount++;
  s.cause=g.cause;s.stage=Dead;say(String(s.name)+t(" is gone"," больше нет"));save(true);
}
bool Pet::known(uint32_t id) const{for(auto v:s.friendIds)if(v==id)return true;return false;}
void Pet::tick(){
  uint32_t now=millis();
  if(!primed){primed=true;seenRx=meshRadio.rxCount;seenRelayed=meshRadio.relayed;seenReceived=meshRadio.received;seenDelivered=meshRadio.delivered;}
  if(!winsPrimed&&rating::book.ready){winsPrimed=true;seenWins=rating::book.players[0].wins;} // the book loads after the radio
  if(now-lastTick<1000)return;uint32_t seconds=(now-lastTick)/1000;lastTick+=seconds*1000;
  elapse(seconds);
  // Food from the air. Counters that went back (statistics cleared) start again from there.
  auto delta=[](uint32_t current,uint32_t& seen){uint32_t d=current>=seen?current-seen:0;seen=current;return d;};
  uint32_t rx=delta(meshRadio.rxCount,seenRx),relayed=delta(meshRadio.relayed,seenRelayed),received=delta(meshRadio.received,seenReceived),delivered=delta(meshRadio.delivered,seenDelivered);
  bool server=config.role!=RoleNormal;
  if(rx){glowAt=now;s.packets+=rx;snackPackets+=rx;gain(2*rx,server?int(rx):0,rx);events++;}
  if(relayed){s.relays+=relayed;snackPackets+=relayed;gain(2*relayed,2*relayed,relayed);}
  while(snackPackets>=25){snackPackets-=25;if(s.snacks<9&&alive()&&s.stage!=Egg)s.snacks++;}
  if(received&&alive()&&s.stage!=Egg){s.messages+=received;gain(15*received,40*received,5*received);say(t("A message, how tasty","Сообщение! Вкусно"),Hearts);}
  if(delivered&&alive()&&s.stage!=Egg){s.acks+=delivered;if(s.snacks<9)s.snacks++;gain(0,40*delivered,10*delivered);say(t("Delivered, a snack for me","Доставлено! Мне вкусняшка"),Hearts);}
  if(winsPrimed){uint32_t wins=rating::book.players[0].wins;if(wins>seenWins&&alive()&&s.stage!=Egg){s.wins+=wins-seenWins;gain(0,200,50);say(t("We won at chess","Мы выиграли в шахматы"),Sparkle);}seenWins=wins;}
  // New friends: nodes heard on air for the first time in its life.
  if(alive()&&s.stage!=Egg)for(unsigned i=0;i<meshRadio.peerCount;i++){const Peer& p=meshRadio.peers[i];if(!p.heard)continue;uint32_t id=mix(p.id)|1;if(known(id))continue;
    s.friendIds[s.friendNext]=id;s.friendNext=(s.friendNext+1)%FriendSlots;s.friends++;gain(0,150,30);say(t("New friend: ","Новый друг: ")+String(p.name),Hearts);save(true);break;}
  // A walk: the IMU (M9) feels the device carried about.
  if(hardware.imuSample&&alive()&&s.stage!=Egg){float a=sqrtf(hardware.accel[0]*hardware.accel[0]+hardware.accel[1]*hardware.accel[1]+hardware.accel[2]*hardware.accel[2]);
    if(fabsf(a-1)>0.25f&&++walkSeconds>=30){walkSeconds=0;s.walks++;gain(0,60,3);say(t("A walk, wheee","Гуляем! Ура"),Hearts);}}
  if(alive()&&!asleep()&&now-chatAt>120000)chatter();
  if(saveDue&&now>=saveDue)save();else if(now-lastSave>=600000)save();
}
void Pet::chatter(){
  uint32_t r=random(1000);String line;
  switch(mood()){
  case Happy:{static const char* const lines[][2]={{"Packets are tasty today","Пакеты сегодня вкусные"},{"I love this mesh","Обожаю эту сеть"},{"Beep-boop","Бип-буп"}};line=tr(lines[r%3][0],lines[r%3][1]);break;}
  case Content:line=r%2?t("Listening to the air...","Слушаю эфир..."):t("Pet me?","Погладишь?");break;
  case Hungry:line=r%2?t("Hungry: the air is empty","Голодно: эфир пуст"):t("A snack?","Вкусняшку?");break;
  case Lonely:line=r%2?t("Write to someone?","Напиши кому-нибудь?"):t("It's lonely here","Мне одиноко");break;
  case Sick:line=t("I feel bad... heal me","Мне плохо... полечи");break;
  default:if(s.stage==Egg)line=t("*tap tap*","*тук-тук*");break;
  }
  if(line.length())say(line);else chatAt=millis();
}
String Pet::cuddle(){
  if(!alive())return t("It is gone","Питомца нет");if(s.stage==Egg){say(t("*tap tap*","*тук-тук*"));return speech;}
  if(millis()-cuddleAt<20000&&cuddleAt){say(t("Enough for now","Хватит пока"));return speech;}
  cuddleAt=millis();s.cuddles++;gain(0,asleep()?30:100,1);say(asleep()?t("Zzz... purr","Хрр... мурр"):t("Purr","Мурр"),Hearts);save(true);return speech;
}
String Pet::feed(){
  if(!alive())return t("It is gone","Питомца нет");if(s.stage==Egg)return t("Eggs do not eat","Яйца не едят");
  if(!s.snacks){say(t("No snacks: traffic brings them","Нет вкусняшек: их приносит эфир"));return speech;}
  if(s.food>=950){say(t("I'm full","Я сыт"));return speech;}
  s.snacks--;gain(300,20,2);say(t("Yum","Ням"),Food);save(true);return speech;
}
String Pet::heal(){
  if(!alive())return t("It is gone","Питомца нет");if(s.stage==Egg)return t("Eggs do not get ill","Яйца не болеют");
  if(s.health>=800){say(t("I'm healthy","Я здоров"));return speech;}
  if(millis()-healAt<300000&&healAt){say(t("Medicine works slowly","Лекарство действует не сразу"));return speech;}
  if(!s.snacks){say(t("Medicine costs a snack","Лекарство стоит вкусняшку"));return speech;}
  healAt=millis();s.snacks--;s.health=clampPoints(s.health+300);gain(0,0,2);say(t("Better now","Уже лучше"),Medicine);save(true);return speech;
}
String Pet::newEgg(){
  if(alive())return t("Your pet is alive","Питомец жив");
  Grave graves[Graves];memcpy(graves,s.graves,sizeof graves);uint8_t count=s.graveCount;uint16_t generation=s.generation+1;
  s=State();memcpy(s.graves,graves,sizeof graves);s.graveCount=count;s.generation=generation;s.species=entropy()%Species;
  hatchName();foodRest=joyRest=healthRest=crumbRest=0;say(t("A new egg","Новое яйцо"),Sparkle);save(true);return speech;
}
String Pet::setMortal(bool on){s.mortal=on;if(!on&&alive()&&!s.health)s.health=1;save(true);events++;return on?t("Death: on","Смерть: вкл."):t("Death: off","Смерть: выкл.");}
bool Pet::rename(const String& name){
  String v=name;v.trim();if(!v.length()||v.length()>15||!meshmesh::validUtf8((const uint8_t*)v.c_str(),v.length()))return false;
  strlcpy(s.name,v.c_str(),sizeof s.name);save(true);events++;return true;
}
void Pet::save(bool soon){
  if(soon){if(!saveDue)saveDue=millis()+5000;return;}
  saveDue=0;lastSave=millis();s.check=fnv((const uint8_t*)&s,offsetof(State,check));writeStored(PetFile,PetTemp,&s,sizeof(State));
}
String Pet::stageName(uint8_t stage) const{
  switch(stage){case Egg:return t("Egg","Яйцо");case Baby:return t("Baby","Малыш");case Child:return t("Kid","Детёныш");case Teen:return t("Teen","Подросток");case Adult:return t("Adult","Взрослый");}
  return t("Gone","Умер");
}
String Pet::moodName() const{
  switch(mood()){case Happy:return t("happy","счастлив");case Content:return t("calm","спокоен");case Hungry:return t("hungry","голоден");case Lonely:return t("lonely","скучает");
  case Sleepy:return t("asleep","спит");case Sick:return t("ill","болеет");case Eating:return t("eating","ест");default:return t("gone","умер");}
}
String Pet::causeText(uint8_t cause) const{return cause==Hunger?t("of hunger","от голода"):cause==Loneliness?t("of loneliness","от одиночества"):"";}
String Pet::ageText(uint32_t v) const{
  if(v<3600)return String(v/60)+t(" min"," мин");if(v<86400)return String(v/3600)+t(" h"," ч");
  return String(v/86400)+t(" d"," д")+(v%86400>=3600?" "+String(v%86400/3600)+t(" h"," ч"):String());
}
int Pet::bob(uint32_t now) const{if(!alive()||s.stage==Egg)return 0;return (now/(asleep()?1500:500))%2;}
int Pet::sway(uint32_t now) const{
  if(s.stage!=Egg)return 0;uint32_t left=hatchLeft();uint32_t period=left<120?150:left<300?300:2000;
  unsigned k=(now/period)%4;return k==1?-1:k==3?1:0;
}
void Pet::sprite(uint8_t grid[Size][Size],uint32_t now) const{
  bool glow=now-glowAt<800&&alive();
  if(s.stage==Egg||!alive()){const char* const* art=s.stage==Egg?SpriteEgg:SpriteGrave;bool egg=s.stage==Egg;
    for(unsigned y=0;y<Size;y++)for(unsigned x=0;x<Size;x++){char c=art[y][x];grid[y][x]=c=='o'?InkOutline:c=='b'?(egg?InkShell:InkBody):c=='l'?(egg?InkBody:InkLight):InkNone;}
    if(egg&&hatchLeft()<300){const uint8_t crack[][2]={{5,7},{6,8},{7,7},{8,8},{9,7},{10,8}};for(auto& c:crack)grid[c[1]][c[0]]=InkOutline;}
    return;}
  const Look& f=s.stage<=Child?kind().young:kind().grown;
  for(unsigned y=0;y<Size;y++)for(unsigned x=0;x<Size;x++){uint8_t v=(f.rows[y]>>(2*x))&3;grid[y][x]=v==3?(glow&&y<4?InkGlow:InkLight):v;} // 1 outline, 2 body
  Mood m=mood();
  auto px=[&](int x,int y,uint8_t ink){if(x>=0&&x<int(Size)&&y>=0&&y<int(Size))grid[y][x]=ink;};
  auto skin=[&](int x,int y){return x>=0&&x<int(Size)&&y>=0&&y<int(Size)&&(grid[y][x]==InkBody||grid[y][x]==InkLight);};
  auto eye=[&](int x,uint8_t pattern){ // bits: top-left, top-right, bottom-left, bottom-right
    if(pattern&1)px(x,f.eye,InkEye);if(pattern&2)px(x+1,f.eye,InkEye);if(pattern&4)px(x,f.eye+1,InkEye);if(pattern&8)px(x+1,f.eye+1,InkEye);};
  bool blink=(now%4200)<160,talk=talking(now)&&now-speechAt<2500&&(now/220)%2;
  bool joyful=m==Happy&&(talking(now)||(effect==Hearts&&now-effectAt<3000));
  if(m==Sleepy||blink){eye(f.left,4|8);eye(f.right,4|8);}
  else if(m==Sick){eye(f.left,1|8);eye(f.right,2|4);}
  else if(joyful||m==Eating){eye(f.left,2|4);eye(f.right,1|8);} // ^ ^
  else{eye(f.left,15);eye(f.right,15);}
  int mx=7,my=f.mouth;
  bool open=m==Eating?(now/200)%2:talk||m==Hungry;
  if(open){px(mx,my,InkEye);px(mx+1,my,InkEye);if(skin(mx,my+1))px(mx,my+1,InkEye);if(skin(mx+1,my+1))px(mx+1,my+1,InkEye);}
  else if(m==Happy){if(skin(mx-1,my))px(mx-1,my,InkEye);if(skin(mx+2,my))px(mx+2,my,InkEye);if(skin(mx,my+1))px(mx,my+1,InkEye);if(skin(mx+1,my+1))px(mx+1,my+1,InkEye);}
  else if(m==Lonely||m==Sick){px(mx,my,InkEye);px(mx+1,my,InkEye);if(skin(mx-1,my+1))px(mx-1,my+1,InkEye);if(skin(mx+2,my+1))px(mx+2,my+1,InkEye);}
  else if(m==Sleepy)px(mx,my,InkEye);
  else{px(mx,my,InkEye);px(mx+1,my,InkEye);}
  if((m==Happy||m==Eating)&&skin(3,f.cheek)&&skin(12,f.cheek)){px(3,f.cheek,InkCheek);px(12,f.cheek,InkCheek);}
  if(m==Lonely&&(now/700)%3&&skin(f.left,f.eye+2))px(f.left,f.eye+2,InkTear);
}
String Pet::json() const{
  StaticJsonDocument<1024> d;d["name"]=s.name;d["stage"]=s.stage==Egg?"egg":s.stage==Baby?"baby":s.stage==Child?"kid":s.stage==Teen?"teen":s.stage==Adult?"adult":"dead";
  const char* moods[]={"happy","calm","hungry","lonely","asleep","ill","eating","gone"};d["mood"]=moods[mood()];
  d["level"]=level();d["xp"]=s.xp;d["next_level_xp"]=levelXp(level()+1);d["food"]=s.food;d["joy"]=s.joy;d["health"]=s.health;d["snacks"]=s.snacks;
  d["age_s"]=s.age;d["hatch_left_s"]=hatchLeft();d["species"]=s.species;d["kind"]=kind().name;d["generation"]=s.generation;d["mortal"]=s.mortal;
  if(!alive())d["cause"]=s.cause==Hunger?"hunger":"loneliness";
  d["packets"]=s.packets;d["relays"]=s.relays;d["messages"]=s.messages;d["acks"]=s.acks;d["friends"]=s.friends;d["wins"]=s.wins;d["cuddles"]=s.cuddles;d["walks"]=s.walks;
  if(talking(millis()))d["speech"]=speech;
  JsonArray g=d.createNestedArray("graves");for(unsigned i=0;i<s.graveCount;i++){JsonObject o=g.createNestedObject();o["name"]=s.graves[i].name;o["age_s"]=s.graves[i].age;o["level"]=s.graves[i].level;o["cause"]=s.graves[i].cause==Hunger?"hunger":"loneliness";}
  String out;serializeJson(d,out);return out;
}
// USB: "pet" shows it; "pet cuddle|feed|heal|egg", "pet mortal on|off", "pet name NAME"; "pet skip SECONDS"
// runs its clock forward (up to 14 days) to check growing up and dying without waiting.
String Pet::command(const String& line){
  String arg=line.length()>4?line.substring(4):String();arg.trim();
  auto reply=[&](const String& r){return "OK "+r+" "+json();};
  if(!arg.length())return json();
  if(arg=="cuddle")return reply(cuddle());if(arg=="feed")return reply(feed());if(arg=="heal")return reply(heal());if(arg=="egg")return reply(newEgg());
  if(arg=="mortal on"||arg=="mortal off")return reply(setMortal(arg=="mortal on"));
  if(arg.startsWith("name "))return rename(arg.substring(5))?reply("renamed"):"ERR name: 1-15 UTF-8 bytes";
  if(arg.startsWith("skip ")){long v=arg.substring(5).toInt();if(v<=0||v>14*86400)return "ERR skip 1-1209600 seconds";uint32_t left=v;while(left){uint32_t step=min<uint32_t>(left,600);elapse(step);left-=step;}save(true);return reply("skipped "+String(v)+" s");}
  return "ERR pet [cuddle|feed|heal|egg|mortal on|off|name NAME|skip SECONDS]";
}
}
