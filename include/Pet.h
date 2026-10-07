#pragma once
#include "Modules.h"
#include <Arduino.h>
#include "PetSprites.h"
// Mesh pet: a pixel creature that lives on the device and feeds on the radio. Received packets and packets
// relayed for others are its food; messages, delivery ACKs, new nodes and won chess games cheer it up,
// and the owner pets it, feeds it the snacks the traffic earns and treats it when it is ill. Time runs
// while the device is on (a switched-off device puts it to sleep). Hunger or loneliness wears its health
// down; at zero it dies (unless death is switched off), leaves a grave and the owner starts a new egg.
// Fifteen species (PetSprites.h, tools/pet_sprites.py), each with a young and a grown look and its own
// colours; a new egg gets a random one. A pet is optional: a new device has none until the owner starts
// an egg, and the owner can let it go. The screens draw it from a 16x16 grid of inks (sprite()); the
// state is a file in MeshMesh storage.
namespace pet {
enum Stage:uint8_t {Egg,Baby,Child,Teen,Adult,Dead,Empty,StageCount}; // Empty: no pet (the owner did not start one or let it go)
enum Mood:uint8_t {Happy,Content,Hungry,Lonely,Sleepy,Sick,Eating,Gone};
enum Cause:uint8_t {NoCause,Hunger,Loneliness};
enum Effect:uint8_t {NoEffect,Hearts,Food,Sparkle,Medicine};
// Inks of the sprite grid; each screen picks its colours (monochrome: outline, eyes and marks lit).
enum Ink:uint8_t {InkNone,InkOutline,InkBody,InkLight,InkEye,InkCheek,InkTear,InkGlow,InkShell}; // InkLight: the species' accent; InkShell: the egg
constexpr unsigned Size=16,Graves=3,FriendSlots=48;
constexpr uint16_t Full=1000;
struct Grave{char name[16];uint32_t age;uint16_t level;uint8_t cause,species;};
struct State{
  uint32_t version=1;
  char name[16]={};
  uint8_t species=0,stage=Egg,cause=NoCause;bool mortal=true;
  uint16_t food=700,joy=700,health=Full,snacks=2,generation=1;
  uint32_t xp=0,age=0;                   // age: seconds the device was on since the egg was laid
  uint32_t packets=0,relays=0,messages=0,acks=0,friends=0,wins=0,cuddles=0,walks=0;
  uint32_t friendIds[FriendSlots]={};uint8_t friendNext=0;
  Grave graves[Graves]={};uint8_t graveCount=0;
  uint32_t check=0;
};
class Pet {
 public:
  State s;
  String speech;uint32_t speechAt=0;     // the last thing it said, shown for a few seconds
  Effect effect=NoEffect;uint32_t effectAt=0;
  uint32_t glowAt=0;                     // a packet was just received: the antenna lights up
  uint32_t events=0;                     // grows with every change a screen should show
  void begin();
  void tick();
  unsigned level() const;                // from 1
  uint32_t levelXp(unsigned n) const{return 25UL*n*(n-1);} // experience where level n starts
  Mood mood() const;
  bool asleep() const;                   // at night by the local clock
  bool has() const{return s.stage!=Empty;}
  bool alive() const{return s.stage!=Dead&&s.stage!=Empty;}
  uint32_t hatchLeft() const;            // seconds until the egg hatches
  bool needsCare() const;
  const Kind& kind() const{return kinds[s.species%Species];}                // hungry, lonely or ill: the home tile turns yellow
  // Owner actions; each returns what to show (and says it).
  String cuddle();String feed();String heal();String newEgg();String release();     // newEgg: a first pet or the next one after death or release
 String setMortal(bool on);
  bool rename(const String& name);       // 1-15 UTF-8 bytes
  String stageName(uint8_t stage) const;String moodName() const;String causeText(uint8_t cause) const;
  String ageText(uint32_t seconds) const;
  // The look now: inks in a 16x16 grid; the creature bobs and blinks with the time.
  void sprite(uint8_t grid[Size][Size],uint32_t now) const;
  bool talking(uint32_t now) const{return speech.length()&&now-speechAt<6000;}
  int bob(uint32_t now) const;           // 0 or 1: the screens lift it a little to the beat
  int sway(uint32_t now) const;          // -1, 0, 1: an egg rocks before it hatches
  String json() const;                   // with the look now: "sprite" (256 inks) and its colours
  String command(const String& line);    // USB "pet ..."
 private:
  uint32_t lastTick=0,lastSave=0,saveDue=0,chatAt=0,cuddleAt=0,healAt=0;
  uint32_t seenRx=0,seenRelayed=0,seenReceived=0,seenDelivered=0,seenWins=0;bool primed=false,winsPrimed=false;
  uint32_t snackPackets=0,walkSeconds=0;
  uint32_t foodRest=0,joyRest=0,healthRest=0,crumbRest=0; // fractions of a point carried between ticks
  void elapse(uint32_t seconds);
  void gain(int food,int joy,uint32_t xp);
  void say(const String& text,Effect e=NoEffect);
  void chatter();
  void hatchName();
  void die();
  void save(bool soon=false);
  bool known(uint32_t friendId) const;
};
uint16_t clampPoints(int v);
}
extern pet::Pet creature;
