#pragma once
#include <Arduino.h>
// Rated chess results (docs/chess.md). Both players sign the same record of a finished game with their
// MeshCore node key; a record is kept only with both signatures checked. Ratings (ELO) are replayed from
// the kept records in one order, so devices holding the same records show the same numbers.
namespace rating {
constexpr size_t CoreSize=91,SigSize=64,SigTextSize=88;
constexpr int Start=1500;                    // rating of a player without counted games
constexpr unsigned NewGames=20,NewK=32,K=20; // K-factor: 32 for a player's first 20 counted games, then 20
constexpr unsigned PerDay=3;                 // counted games of one pair per UTC day; the others are kept, not counted
#if defined(MM_NRF52)
constexpr unsigned MaxRecords=40;            // the oldest records are folded into the players' base when full
#else
constexpr unsigned MaxRecords=128;
#endif
constexpr unsigned MaxPlayers=32,History=10,MaxDeltas=8;
enum Result:uint8_t {WhiteWon=1,BlackWon=2,Drawn=3};

struct Record {uint8_t core[CoreSize];uint32_t timeW,timeB;uint8_t sigW[SigSize],sigB[SigSize];};
// The signed part: "MMR1", White's key, Black's key, game number, result, reason, plies, the first
// 16 bytes of SHA-256 over the moves, flags (bit 0: rated).
void makeCore(uint8_t out[CoreSize],const uint8_t white[32],const uint8_t black[32],uint16_t game,uint8_t result,uint8_t reason,const uint16_t* moves,uint16_t plies);
// What one player signs: the core followed by that player's clock (Unix time, 0 when unset).
void signedBytes(uint8_t out[CoreSize+4],const uint8_t core[CoreSize],uint32_t time);
uint32_t when(const Record& r);              // the earlier of the clocks that were set
uint64_t idOf(const uint8_t key[32]);        // the 8-byte node ID, as in contacts
void sigText(const uint8_t sig[SigSize],char out[SigTextSize+1]);   // base64
bool sigParse(const char* text,uint8_t sig[SigSize]);

struct Player {
  uint8_t key[32];char name[25];
  float elo;uint16_t games;                  // counted games; current values after the replay
  uint16_t wins,draws,losses;                // this device's result against them (players[0]: overall)
  float baseElo;uint16_t baseGames,baseWins,baseDraws,baseLosses; // from records folded away
};
// My rating change in one of my recent games.
struct Delta {uint64_t peer;uint16_t game;int16_t before,after;bool counted;};

class Book {
 public:
  Player players[MaxPlayers];unsigned playerCount=0; // players[0] is this device once begin() ran
  unsigned records=0;
  int16_t history[History];unsigned historyCount=0;  // my rating after each of my latest counted games
  Delta deltas[MaxDeltas];unsigned deltaCount=0;
  bool ready=false;
  void begin(const uint8_t selfKey[32],const char* selfName);
  // A record with both signatures checked. False when it is there already or cannot be stored.
  bool add(const Record& r,const char* opponentName);
  int myElo() const{return ready?int(lroundf(players[0].elo)):Start;}
  int elo(uint64_t id) const;                 // Start for players without counted games
  unsigned games(uint64_t id) const;
  void expected(uint64_t id,int& win,int& draw,int& loss) const; // my change for a win, draw, loss against them
  const Delta* delta(uint64_t peer,uint16_t game) const;
  void rename(uint64_t id,const char* name);  // contact names change
  String json() const;
 private:
  const Player* find(uint64_t id) const;
  int index(const uint8_t key[32]);
  void replay(Record* all,unsigned n);
  bool save(const Record* all,unsigned n);
  unsigned load(Record* all);
};
extern Book book;
// One file replaced whole; LittleFS on the boards, memory in the host checks.
bool storeWrite(const uint8_t* data,size_t size);
size_t storeRead(uint8_t* data,size_t cap);
}
