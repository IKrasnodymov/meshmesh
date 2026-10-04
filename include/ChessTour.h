#pragma once
#include <Arduino.h>
struct ChessMatch;
// Swiss chess tournaments over MeshCore direct messages (docs/chess.md, «Турнир»). The organiser's device
// keeps the whole tournament: invitations, pairings, results and standings. It pairs automatically:
// round 1 by lot (seeded by the tournament number), later rounds by score, without repeated games and
// with alternating colours. Each player gets their pairing with the opponent's key, plays an ordinary
// rated game (ChessNet), reports the result and receives the standings after every round.
namespace tour {
constexpr unsigned MaxPlayers=10,MaxRounds=9,MaxTours=3,MaxPairs=MaxPlayers/2,MaxOut=40;
constexpr uint8_t NoPlayer=0xFF;
enum State:uint8_t {Free,Inviting,Invited,Declined,Running,Over,Cancelled};
enum PlayerState:uint8_t {Asked,Joined,Refused};
enum Result:uint8_t {Pending=0,WhiteWon=1,BlackWon=2,Drawn=3,Bye=4,Disputed=5};
struct Player {uint64_t id;uint8_t key[32];bool hasKey;char name[25];uint8_t state;};
// One game of a round; black is NoPlayer for a bye. The organiser keeps each side's report.
struct Pair {uint8_t white,black;uint16_t game;uint8_t result,reportW,reportB;};
struct Tour {
  uint16_t id=0;uint8_t state=Free;char name[33]={};uint64_t organizer=0;uint8_t rounds=5,hours=24;
  Player players[MaxPlayers]={};uint8_t playerCount=0;   // organiser: everyone asked; player: the list it was sent
  uint8_t round=0;                                       // the round being played, 0 before the start
  Pair pairs[MaxRounds][MaxPairs]={};uint8_t pairCount[MaxRounds]={}; // organiser: every game; player: its own
  uint8_t standRound=0,standCount=0;uint64_t standId[MaxPlayers]={};uint8_t standHalf[MaxPlayers]={},standBuch[MaxPlayers]={}; // player: from the organiser
  bool unseen=false;uint32_t changedAt=0;
  uint8_t due=0;                                         // work left for tick(): start, round check
  bool organising() const;
  bool active() const{return state==Inviting||state==Invited||state==Running;}
  int index(uint64_t id) const;
  const char* nameOf(uint64_t id) const;
};
struct Row {uint64_t id;const char* name;uint8_t half,buch;bool me;};

class Net {
 public:
  Tour tours[MaxTours];
  String event;uint32_t events=0;Tour* eventTour=nullptr;bool dirty=true;
  void begin();void tick();
  // From MeshRadio: a direct message from a known contact. True when it was a tournament command.
  bool receive(uint64_t from,const char* name,const char* text);
  bool delivery(uint32_t id,uint8_t status);  // true when the message was one of ours
  void gameOver(ChessMatch& m);               // from ChessNet: a tournament game ended here
  Tour* create(const String& name,uint8_t rounds,uint8_t hours,const uint64_t* ids,unsigned n);
  bool accept(Tour& t);bool decline(Tour& t);bool start(Tour& t);bool cancel(Tour& t);bool remove(Tour& t);
  bool setResult(Tour& t,uint16_t game,uint8_t result); // the organiser settles a disputed game
  void viewed(Tour& t);
  Tour* find(uint16_t id);
  unsigned waiting() const;                   // invitations to answer and news not opened
  unsigned standings(const Tour& t,Row* out) const;
  const Pair* myPair(const Tour& t,uint8_t round) const;
  uint8_t myColor(const Tour& t,const Pair& p) const; // 0 White, 1 Black, 2 bye
  uint64_t opponent(const Tour& t,const Pair& p) const;
  String command(const String& line);         // USB: tour ...
  String json() const;
 private:
  struct Out {uint64_t peer;char text[152];uint32_t id,retryAt,since;uint8_t status,retries;bool used;};
  Out outbox[MaxOut]={};
  bool saveDue=false;uint32_t saveAt=0,boardAt=0;
  Tour* slot();
  void send(uint64_t peer,const String& text);void transmit(Out& o);
  void news(Tour& t,const String& text);void changed(Tour& t,bool now=true);
  void pairRound(Tour& t);void announceRound(Tour& t);void sendPlayers(Tour& t);void sendStandings(Tour& t,const char* verb="st");bool readStandings(Tour& t,const char* args);
  void record(Tour& t,uint16_t game,uint64_t from,uint8_t result);void roundDone(Tour& t);
  void localGame(Tour& t,const Pair& p);void reset(Tour& t);
  void save();void load();
};
extern Net net;
// Storage: one file replaced whole; LittleFS on the boards, memory in the host preview.
bool storeWrite(const uint8_t* data,size_t size);
size_t storeRead(uint8_t* data,size_t cap);
}
