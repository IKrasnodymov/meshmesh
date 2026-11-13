#pragma once
#include <stdint.h>
#include <stddef.h>
// Russian draughts (русские шашки): 8x8, White starts from a1-h2-..., men move forward and capture in all four
// directions, kings fly, capturing is compulsory (any capture sequence, not the longest), a man that reaches the
// last row during a capture continues it as a king, captured pieces leave the board after the move (the Turkish
// strike: a piece is never jumped twice). Plain C++ (no Arduino), shared by the screens, the network games and the
// host checks.
namespace checkers {
constexpr uint8_t White=0,Black=1;
// The 32 dark squares: s = row*4 + column/2, row 0 is White's first row; a1 (s 0) is dark.
// A piece is +Man/+King for White, -Man/-King for Black.
constexpr int8_t Empty=0,Man=1,King=2;
inline int colOf(int s){return ((s&3)<<1)|((s>>2)&1);}
inline int rowOf(int s){return s>>2;}
inline int squareAt(int col,int row){return col<0||col>7||row<0||row>7||((col^row)&1)?-1:row*4+(col>>1);}
constexpr unsigned MaxPath=13,MaxMoves=64;
// A move: the squares the piece stands on, from the start to the end; capture: at least one piece taken.
struct Move{uint8_t n=0;bool capture=false;uint8_t path[MaxPath]={};uint8_t from() const{return path[0];}uint8_t to() const{return path[n-1];}};
bool sameMove(const Move& a,const Move& b);
// "c3-d4", "c3:e5:c7"; at most 3*MaxPath bytes.
size_t moveText(const Move& m,char* out,size_t cap);
void squareName(int s,char out[3]);
int parseSquare(const char* text); // "c3" -> square, -1 when it is not a dark square
struct Position {
  int8_t board[32]={};
  uint8_t side=White;
  void start();
  int8_t own(int s) const{int8_t p=board[s];return side==White?p:-p;} // >0: a piece of the side to move
  unsigned legal(Move* out) const;       // MaxMoves entries; captures only when there is one
  void apply(const Move& m);             // no checks: only for moves from legal()
  bool parse(const char* text,Move& m) const; // a legal move written as moveText()
  unsigned count(uint8_t color) const;
 private:
  void captures(int s,bool king,int8_t* b,Move& m,Move* out,unsigned& n) const;
};
// A game: the position and what the automatic draw counts; the saved form is the position itself.
// The game ends when the side to move has no move (it loses) or after 15 moves of each side with kings only
// and without a capture (a draw, as in the Russian rules).
enum Outcome:uint8_t {Ongoing,NoMoves,QuietDraw};
constexpr uint8_t QuietPlies=30;
struct Game {
  Position pos;uint16_t plies=0;uint8_t quiet=0;Outcome outcome=Ongoing;Move last;
  void reset(){pos.start();plies=0;quiet=0;outcome=Ongoing;last=Move();}
  bool play(const Move& m); // a legal move while the game is on
  void judge();
};
}
