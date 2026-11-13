#pragma once
#include "Modules.h"
#include <Arduino.h>
#include "Checkers.h"
#include "Sea.h"
// Checkers and sea battle with contacts over MeshCore direct messages (docs/games.md): no rating, just a game.
// As in chess (ChessNet.h) every command is an ordinary encrypted, acknowledged text message: "🏁" (checkers)
// or "🚢" (sea battle), a 4-digit game ID and the command, readable in a stock MeshCore app. Channel messages
// are never taken as moves. The games are saved in LittleFS (/meshmesh/duels.0|1).
struct Duel {
  enum Kind:uint8_t {Checkers,Sea};
  enum State:uint8_t {Free,Inviting,Invited,Playing,Over};
  enum Result:uint8_t {Undecided,Won,Lost,Drawn};
  enum Reason:uint8_t {NoReason,Rules,Resigned,Agreed,Declined,Cancelled,Quiet};
  enum Offer:uint8_t {NoOffer,OfferedByMe,OfferedToMe};
  uint64_t peer=0;uint16_t id=0;char name[25]={};
  Kind kind=Checkers;State state=Free;Result result=Undecided;Reason reason=NoReason;Offer drawOffer=NoOffer;
  uint8_t mine=0;             // checkers: my colour; sea: 0 when I shoot first (I invited), 1 when they do
  bool unseen=false;          // news the player has not opened yet
  uint32_t started=0,updated=0; // Unix time; 0 when the clock was not set
  uint32_t changedAt=0;       // millis() of the last change, for ordering
  checkers::Game game;
  // Sea battle: my fleet and the shots at it; what I know of their field; their fleet once the game is over.
  sea::Home home;sea::Target target;sea::Fleet theirs;bool revealed=false,fair=true;
  uint16_t shots=0;           // shots answered so far by both sides: the next one has the number shots+1
  bool seaTurn=false;         // my shot (or my shot waits for its answer: aim)
  int8_t aim=-1,lastOut=-1;   // my shot waiting for the answer; my last answered shot
  int8_t lastIn=-1;uint8_t lastAnswer=0; // their last shot and my answer (sent again when they repeat it)
  // My last move, shot or answer until the other side confirms it (ACK or their next command); the last other command.
  char move[40]={};bool moveOpen=false;uint8_t moveStatus=0;uint32_t moveId=0;
  char out[72]={};uint8_t outStatus=0;uint32_t outId=0;
  uint32_t retryAt=0,triedAt=0,openSince=0;uint8_t retries=0;bool autoStopped=false; // resending (not kept over a restart)
  bool active() const{return state==Inviting||state==Invited||state==Playing;}
  bool myTurn() const;
  bool pending() const;       // something sent from here is not confirmed yet
  bool sending() const;       // a command of this game is on the radio now
  uint8_t link() const;       // delivery shown to the player: failed wins, then in flight
  bool canShuffle() const;    // sea: the fleet can still be placed again (nobody shot at it)
};
#if !MM_DUEL
// Neither game in this image (Modules.h): the calls of the shared code cost nothing, the command says so.
class DuelNet {
 public:
  void begin(){}void tick(){}void flush(){}
  bool receive(uint64_t,const char*,const char*){return false;}bool delivery(uint32_t,uint8_t){return false;}
  unsigned waiting(int=-1) const{return 0;}unsigned count(int=-1) const{return 0;}bool playsWith(uint64_t) const{return false;}
  String command(const String&){return "ERR games are not in this build";}
};
#else
class DuelNet {
 public:
  static constexpr unsigned Max=6;
  Duel games[Max];
  String event;uint32_t events=0;Duel* eventGame=nullptr;
  bool dirty=true;
  void begin();void tick();void flush(){if(saveDue)save();}
  bool receive(uint64_t from,const char* name,const char* text); // a direct message: true when it was a game command
  bool delivery(uint32_t id,uint8_t status);                     // true when the message was one of ours
  Duel* invite(uint64_t peer,Duel::Kind kind,int color=2);       // checkers colour: White, Black or 2 for random
  bool accept(Duel& g);bool decline(Duel& g);bool resign(Duel& g);bool draw(Duel& g); // draw: offer or accept the offer
  bool move(Duel& g,const checkers::Move& m);bool shoot(Duel& g,int cell);bool shuffle(Duel& g);
  bool resend(Duel& g);bool remove(Duel& g);void viewed(Duel& g);
  Duel* find(uint16_t id);
  bool room();
  unsigned waiting(int kind=-1) const;  // games where the player has to act
  unsigned count(int kind=-1) const;
  bool playsWith(uint64_t peer) const;  // a game with this contact keeps it in the contact table
  int32_t retryIn(const Duel& g) const;
  String command(const String& line);   // USB, the web page and the app: duel ...
  String json() const;
  String detail(const Duel& g) const;
  static bool built(Duel::Kind kind){return kind==Duel::Checkers?MM_CHECKERS:MM_SEA;}
 private:
  bool saveDue=false;uint32_t saveAt=0;
  Duel* slot();Duel* find(uint64_t peer,uint16_t id,Duel::Kind kind);
  bool send(Duel& g,const String& text);bool sendMove(Duel& g);
  void retry(Duel& g);void confirmed(Duel& g);void received(Duel& g,bool play);
  void finish(Duel& g,Duel::Result r,Duel::Reason why);
  void news(Duel& g,const String& text);void changed(Duel& g,bool now=true);
  void judge(Duel& g);void seaAnswer(Duel& g,int cell);
  void save();void load();
};
#endif
extern DuelNet duels;
