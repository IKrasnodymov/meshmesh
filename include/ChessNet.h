#pragma once
#include <Arduino.h>
#include "Chess.h"
// Chess with contacts over MeshCore direct messages (docs/chess.md). Every command is an ordinary
// encrypted, acknowledged text message that starts with "♟" and a 4-digit game ID, so a stock
// MeshCore app shows it as readable text. Games are replayed from their move lists and saved in
// LittleFS; channel messages are never taken as moves.
struct ChessMatch {
  enum State:uint8_t {Free,Inviting,Invited,Playing,Over};
  enum Result:uint8_t {Undecided,WhiteWon,BlackWon,Drawn};
  enum Reason:uint8_t {NoReason,Mate,Resigned,Stalemate,Repetition,FiftyMoves,DeadPosition,TooLong,Agreed,Declined,Cancelled};
  enum Offer:uint8_t {NoOffer,OfferedByMe,OfferedToMe};
  uint64_t peer=0;uint16_t id=0;char name[25]={};
  uint8_t mine=chess::White;              // this device's colour
  State state=Free;Result result=Undecided;Reason reason=NoReason;Offer drawOffer=NoOffer;
  bool unseen=false;                      // news the player has not opened yet
  uint32_t started=0,updated=0;           // Unix time; 0 when the clock was not set
  uint32_t changedAt=0;                   // millis() of the last change, for ordering
  chess::Game game;
  // The last command this device sent and its delivery (ChatMessage::Status).
  char out[80]={};uint8_t outStatus=0;uint32_t outId=0;
  // My last move until the other side confirms it (ACK or their reply). It is resent from the game
  // itself, so a draw offer or resignation sent later does not replace it.
  bool moveOpen=false;uint8_t moveStatus=0;uint32_t moveId=0;
  // Automatic resending (not saved): next attempt, last attempt, start of the unconfirmed period.
  uint32_t retryAt=0,triedAt=0,openSince=0;uint8_t retries=0;bool autoStopped=false;
  // Rated games (ChessRating.h): the challenge said "r"; at the end both players sign the same result.
  enum Sign:uint8_t {SignNone,SignDue,SignSent,SignStored,SignBad};
  bool rated=false;uint8_t peerKey[32]={};
  uint8_t sign=SignNone;bool theirSigned=false;
  uint32_t myTime=0,theirTime=0;uint8_t mySig[64]={},theirSig[64]={};
  bool sigOpen=false;uint8_t sigStatus=0;uint32_t sigId=0; // my signature until its ACK
  uint16_t tour=0;uint8_t round=0;bool tourReported=false; // a tournament game (ChessTour.h)
  bool active() const{return state==Inviting||state==Invited||state==Playing;}
  bool settled() const;                   // finished, its rating signature and tournament report done
  bool myTurn() const{return state==Playing&&game.pos.side==mine;}
  bool won() const{return (result==WhiteWon&&mine==chess::White)||(result==BlackWon&&mine==chess::Black);}
  bool lost() const{return (result==WhiteWon&&mine==chess::Black)||(result==BlackWon&&mine==chess::White);}
  bool pending() const;                   // something sent from here is not confirmed yet
  bool sending() const;                   // a command of this game is on the radio now
  uint8_t link() const;                   // delivery shown to the player: failed wins, then in flight
};
class ChessNet {
 public:
  static constexpr unsigned MaxMatches=6;
  ChessMatch matches[MaxMatches];
  String event;uint32_t events=0;         // the latest news for the screen; events counts them
  ChessMatch* eventMatch=nullptr;
  bool dirty=true;
  void begin();void tick();
  // From MeshRadio: a direct message from a known contact. True when it was a chess command.
  bool receive(uint64_t from,const char* name,const char* text);
  void delivery(uint32_t id,uint8_t status);
  ChessMatch* invite(uint64_t peer,int color,bool rated=true); // color: White, Black or 2 for random
  // A tournament pairing: the game starts at once (no challenge); the opponent becomes a contact if needed.
  ChessMatch* tourGame(uint64_t peer,const uint8_t key[32],const char* name,uint16_t id,int color,uint16_t tour,uint8_t round);
  bool accept(ChessMatch& m);bool decline(ChessMatch& m);
  bool move(ChessMatch& m,chess::Move move);
  bool offerDraw(ChessMatch& m);bool acceptDraw(ChessMatch& m);bool resign(ChessMatch& m);
  bool resend(ChessMatch& m);bool remove(ChessMatch& m);
  ChessMatch* find(uint16_t id);
  bool room();                            // a new game fits (a free board or a settled finished game)
  void viewed(ChessMatch& m);            // the player opened the game
  unsigned waiting() const;               // games where the player has to act
  unsigned count() const;
  int32_t retryIn(const ChessMatch& m) const; // seconds to the next automatic resend, or -1
  String command(const String& line);     // USB: chess ...
  String json() const;
  String detail(const ChessMatch& m) const;  // one game for the web page: notation and legal moves
  String web() const;                       // the list for the web page, with the latest news
 private:
  bool saveDue=false;uint32_t saveAt=0;
  ChessMatch* slot();
  ChessMatch* find(uint64_t peer,uint16_t id);
  bool send(ChessMatch& m,const String& text);
  bool sendMove(ChessMatch& m);
  String moveText(const ChessMatch& m) const;
  void retry(ChessMatch& m);
  bool sendSig(ChessMatch& m);void signResult(ChessMatch& m);void storeResult(ChessMatch& m);bool core(const ChessMatch& m,uint8_t* out) const;
  void confirmed(ChessMatch& m);
  void received(ChessMatch& m,bool move);
  void finish(ChessMatch& m,ChessMatch::Result result,ChessMatch::Reason reason);
  void judge(ChessMatch& m);
  void news(ChessMatch& m,const String& text);
  void changed(ChessMatch& m,bool now=true);
  void save();void load();
};
extern ChessNet chessNet;
// Storage for the saved games; LittleFS on the boards, memory in the host preview.
// Notation in the interface language: K Q R B N, or Кр Ф Л С К in Russian.
String chessLocalSan(const char* san);
bool chessStoreWrite(const uint8_t* data,size_t size);
size_t chessStoreRead(uint8_t* data,size_t cap);
