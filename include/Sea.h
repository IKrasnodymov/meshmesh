#pragma once
#include <stdint.h>
#include <stddef.h>
// Sea battle (морской бой): a 10x10 field, columns a..j, rows 1..10; ten ships of 4, 3, 3, 2, 2, 2, 1, 1, 1, 1
// cells that do not touch each other, not even at a corner. A hit or a sunk ship lets the shooter fire again.
// Plain C++ (no Arduino), shared by the screens, the network games and the host checks.
namespace sea {
constexpr int Size=10,Cells=100,Ships=10,ShipCells=20;
constexpr uint8_t Lengths[Ships]={4,3,3,2,2,2,1,1,1,1};
inline int cellAt(int col,int row){return col<0||col>=Size||row<0||row>=Size?-1:row*Size+col;}
void cellName(int c,char out[4]);   // "a1" .. "j10"
int parseCell(const char* text,const char** end=nullptr); // -1 when it is not a cell
// A fleet: the first cell (top-left) of each ship and whether it lies down the rows.
struct Fleet {
  uint8_t at[Ships]={};uint16_t down=0;
  int cell(int ship,int k) const{int c=at[ship];return down>>ship&1?c+k*Size:c+k;}
  int shipAt(int cell) const;       // -1: water
  bool valid() const;               // inside the field, not touching
  void random(uint32_t seed);
  size_t text(char* out,size_t cap) const; // "a1h c3v e5h ... j10": a cell and h (across) or v (down), one-cell ships without
  bool parse(const char* text);
};
// What the shooter knows about a cell of the other field.
enum Mark:uint8_t {Unknown=0,Miss=1,Hit=2,Sunk=3};
enum Answer:uint8_t {NoAnswer=0,AnswerMiss=1,AnswerHit=2,AnswerSunk=3,AnswerWin=4};
const char* answerWord(uint8_t a);  // "miss", "hit", "sunk", "win"
int answerOf(const char* word);
// The defender's side: its fleet and the cells shot at. answer() records a shot and tells the result.
struct Home {
  Fleet fleet;uint8_t shot[13]={};
  bool isShot(int c) const{return shot[c>>3]>>(c&7)&1;}
  uint8_t answer(int c);
  bool sunk(int ship) const;
  unsigned shipsLeft() const;
};
// The shooter's side: the marks; a sunk ship marks its cells and the water around it (it cannot hold a ship).
struct Target {
  uint8_t mark[Cells]={};
  void record(int c,uint8_t answer);
  bool around(int c) const;         // water next to a sunk ship: known without a shot
  bool open(int c) const{return c>=0&&c<Cells&&mark[c]==Unknown&&!around(c);}
  unsigned sunkShips() const;
};
// The defender's answers must agree with the fleet shown at the end: false when a shot got a wrong answer.
bool honest(const Fleet& f,const Target& t);
}
