#pragma once
#include <stdint.h>
#include <stddef.h>
// Klondike ("Косынка"): rules, scoring, undo and a validated saved form. The screen UI draws it.
namespace solitaire {
// Piles: stock, waste, four foundations, seven tableau columns.
constexpr uint8_t Stock=0,Waste=1,Foundation=2,Tableau=6,PileCount=13,FaceUp=0x80;
// A card is 0..51 (suit*13+rank; suits: spades, hearts, diamonds, clubs; rank 0 = ace) plus FaceUp.
inline uint8_t suit(uint8_t c){return (c&0x3f)/13;}
inline uint8_t rank(uint8_t c){return (c&0x3f)%13;}
inline bool red(uint8_t c){return suit(c)==1||suit(c)==2;}
inline bool up(uint8_t c){return c&FaceUp;}
inline bool isFoundation(uint8_t p){return p>=Foundation&&p<Tableau;}
inline bool isTableau(uint8_t p){return p>=Tableau&&p<PileCount;}
struct Pile{uint8_t n=0,c[24]={};uint8_t top() const{return n?c[n-1]:0;}};
constexpr size_t SavedSize=76,UndoDepth=64;
struct Move{uint8_t from,count,to;};
class Game {
 public:
  Pile pile[PileCount];
  uint8_t draw=1;          // cards turned from the stock at a time: 1 or 3
  int16_t score=0;         // Windows standard scoring, never below zero
  uint16_t moves=0;
  uint32_t seconds=0;      // play time; the UI counts it
  bool counted=false;      // the game has a move and is included in the statistics
  void deal(uint32_t seed,uint8_t cardsPerDraw);
  bool turnStock();                                   // turn cards from the stock or recycle the waste
  bool canMove(uint8_t from,uint8_t count,uint8_t to) const;
  bool move(uint8_t from,uint8_t count,uint8_t to);   // the top count cards of from
  uint8_t movable(uint8_t from) const;                // cards that can be picked up from a pile
  int target(uint8_t from,uint8_t count) const;       // the best destination, or -1
  bool hint(Move& m) const;                           // a useful move; from==Stock with to==Waste means "turn"
  bool collect();                                     // one card to a foundation (lowest rank first)
  bool finishable() const;                            // stock, waste and face-down cards are all gone
  bool won() const;
  unsigned onFoundations() const;
  bool undo();
  unsigned undoCount() const{return undoSize;}
  size_t save(uint8_t* out) const;                    // SavedSize bytes
  bool load(const uint8_t* in,size_t size);           // rejects anything that is not a legal position
 private:
  uint8_t history[UndoDepth][SavedSize];unsigned undoHead=0,undoSize=0;
  void remember();
  void addScore(int delta);
  void flipTops();
};
}
