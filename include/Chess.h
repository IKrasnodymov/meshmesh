#pragma once
#include <stdint.h>
#include <stddef.h>
// Chess rules: legal moves, check, castling, en passant, promotion and the automatic endings.
// Plain C++ (no Arduino), shared by the M9 screen, the network games and the host checks.
namespace chess {
// Squares 0..63: a1=0, b1=1 .. h8=63. A piece is +type for White, -type for Black.
enum Piece:int8_t {Empty=0,Pawn=1,Knight=2,Bishop=3,Rook=4,Queen=5,King=6};
constexpr uint8_t White=0,Black=1;
inline int fileOf(int s){return s&7;}
inline int rankOf(int s){return s>>3;}
// A move: from | to<<6 | promotion piece<<12 (Empty when there is none). 0 is never a legal move.
typedef uint16_t Move;
inline Move makeMove(int from,int to,int promotion=Empty){return Move(from|to<<6|promotion<<12);}
inline int moveFrom(Move m){return m&63;}
inline int moveTo(Move m){return m>>6&63;}
inline int movePromotion(Move m){return m>>12&7;}
constexpr unsigned MaxMoves=256;   // legal moves in one position (the known maximum is 218)
enum Castle:uint8_t {WhiteShort=1,WhiteLong=2,BlackShort=4,BlackLong=8};
struct Position {
  int8_t board[64]={};
  uint8_t side=White,castling=0;
  int8_t enPassant=-1;          // the square a pawn skipped on the last move, or -1
  uint16_t halfmove=0,fullmove=1;
  void start();
  bool fromFen(const char* fen);
  size_t fen(char* out,size_t cap) const;   // at most 90 bytes
  int king(uint8_t color) const;
  bool attacked(int square,uint8_t by) const;
  bool inCheck() const{return attacked(king(side),side^1);}
  unsigned legal(Move* out) const;          // MaxMoves entries
  bool isLegal(Move m) const;
  void apply(Move m);                       // no checks: only for moves from legal()
  bool play(Move m){if(!isLegal(m))return false;apply(m);return true;}
  bool samePlacement(const Position& o) const; // repetition: pieces, side, castling, en passant
  bool insufficientMaterial() const;
  bool parseUci(const char* text,Move& m) const; // "e2e4", "e7e8q"; only legal moves
  // Standard notation before the move is played: "Nbd7", "exd6", "O-O", "e8=Q+", "Qh5#".
  size_t san(Move m,char* out,size_t cap) const;
 private:
  unsigned pseudo(Move* out) const;
  int enPassantTarget() const;               // enPassant only when a pawn can take there
};
void uci(Move m,char out[6]);
constexpr unsigned MaxPlies=512;
// Draws are automatic: threefold repetition, 50 moves without a capture or pawn move, no mating
// material, and MaxPlies (TooLong) because the saved move list has a fixed size.
enum Outcome:uint8_t {Ongoing,Checkmate,Stalemate,FiftyMoves,Repetition,DeadPosition,TooLong};
// A game from the starting position: the move list is the saved form and is replayed on load.
class Game {
 public:
  Move moves[MaxPlies]={};
  uint16_t plies=0;
  Position pos;
  Outcome outcome=Ongoing;
  void reset(){plies=0;pos.start();outcome=Ongoing;}
  bool play(Move m);                           // a legal move while the game is on
  bool load(const Move* list,unsigned count);  // rejects any illegal move
  Position at(unsigned ply) const;             // the position after ply moves
  int material(uint8_t color) const;           // pawn = 1 .. queen = 9
 private:
  Outcome judge() const;
};
}
