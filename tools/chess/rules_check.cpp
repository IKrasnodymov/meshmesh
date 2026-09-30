// Host check of src/Chess.cpp: move generation against published perft counts, notation and the
// automatic endings. Build and run: tools/chess/rules_check.sh
#include "Chess.h"
#include <stdio.h>
#include <string.h>
using namespace chess;
static int failures=0;
#define CHECK(cond,...) do{if(!(cond)){failures++;printf("FAIL %s:%d ",__FILE__,__LINE__);printf(__VA_ARGS__);printf("\n");}}while(0)
static uint64_t perft(const Position& p,int depth){if(!depth)return 1;Move list[MaxMoves];unsigned n=p.legal(list);if(depth==1)return n;uint64_t sum=0;for(unsigned i=0;i<n;i++){Position q=p;q.apply(list[i]);sum+=perft(q,depth-1);}return sum;}
static Game play(const char* moves){Game g;g.reset();char word[8];const char* c=moves;while(sscanf(c,"%7s",word)==1){Move m;if(!g.pos.parseUci(word,m)||!g.play(m)){printf("  illegal %s\n",word);failures++;break;}c=strstr(c,word)+strlen(word);}return g;}
static void sanOf(const char* fen,const char* move,const char* expected){Position p;CHECK(p.fromFen(fen),"fen %s",fen);Move m;CHECK(p.parseUci(move,m),"move %s",move);char s[12];p.san(m,s,sizeof s);CHECK(!strcmp(s,expected),"san %s: %s, expected %s",move,s,expected);}
int main(){
  // https://www.chessprogramming.org/Perft_Results
  struct {const char* fen;int depth;uint64_t nodes;} cases[]={
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",4,197281},
    {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",3,97862},
    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",5,674624},
    {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",4,422333},
    {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",3,62379},
    {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",3,89890}};
  for(auto& c:cases){Position p;CHECK(p.fromFen(c.fen),"fen");uint64_t n=perft(p,c.depth);CHECK(n==c.nodes,"perft %s depth %d: %llu, expected %llu",c.fen,c.depth,(unsigned long long)n,(unsigned long long)c.nodes);}
  // FEN round trip.
  for(auto& c:cases){Position p;p.fromFen(c.fen);char out[96];p.fen(out,sizeof out);CHECK(!strcmp(out,c.fen),"fen round trip %s -> %s",c.fen,out);}
  // Notation: disambiguation, captures, en passant, castling, promotion, check and mate.
  sanOf("2k5/8/8/8/8/8/8/R3K2R w KQ - 0 1","e1g1","O-O");
  sanOf("2k5/8/8/8/8/8/8/R3K2R w KQ - 0 1","e1c1","O-O-O");
  sanOf("2k5/8/8/8/8/8/4K3/R6R w - - 0 1","a1d1","Rad1");
  sanOf("7k/8/8/8/8/5N2/8/1N2K3 w - - 0 1","b1d2","Nbd2");
  sanOf("7k/8/8/1N6/8/8/8/1N2K3 w - - 0 1","b1c3","N1c3");
  sanOf("8/7k/8/8/Q7/8/8/Q5QK w - - 0 1","a1d4","Qa1d4");
  sanOf("8/7k/8/8/Q7/8/8/Q3Q2K w - - 0 1","a1d4","Q1d4");
  sanOf("7k/8/8/3pP3/8/8/8/4K3 w - d6 0 1","e5d6","exd6");
  sanOf("7k/P7/8/8/8/8/8/4K3 w - - 0 1","a7a8q","a8=Q+");
  sanOf("7k/P7/8/8/8/8/8/4K3 w - - 0 1","a7a8n","a8=N");
  sanOf("6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1","a1a8","Ra8#");
  // A UCI move to the last rank without a letter becomes a queen.
  {Position p;p.fromFen("7k/P7/8/8/8/8/8/4K3 w - - 0 1");Move m;CHECK(p.parseUci("a7a8",m)&&movePromotion(m)==Queen,"default promotion");CHECK(!p.parseUci("a7a8k",m),"king promotion");CHECK(!p.parseUci("e1e3",m),"illegal king move");CHECK(!p.parseUci("e1",m),"short text");}
  // Castling through check and out of check is refused.
  {Position p;p.fromFen("4k3/8/8/8/8/8/5r2/R3K2R w KQ - 0 1");Move m;CHECK(!p.parseUci("e1g1",m),"castle through f1 attack");CHECK(p.parseUci("e1c1",m),"long castle allowed");}
  {Position p;p.fromFen("4k3/8/8/8/8/8/4r3/R3K2R w KQ - 0 1");Move m;CHECK(!p.parseUci("e1g1",m)&&!p.parseUci("e1c1",m),"castle out of check");}
  // Endings.
  CHECK(play("f2f3 e7e5 g2g4 d8h4").outcome==Checkmate,"fool's mate");
  CHECK(play("e2e4 e7e5 d1h5 b8c6 f1c4 g8f6 h5f7").outcome==Checkmate,"scholar's mate");
  CHECK(play("g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8").outcome==Repetition,"threefold repetition");
  CHECK(play("g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1").outcome==Ongoing,"two repetitions only");
  // Sam Loyd's ten-move stalemate.
  CHECK(play("e2e3 a7a5 d1h5 a8a6 h5a5 h7h5 h2h4 a6h6 a5c7 f7f6 c7d7 e8f7 d7b7 d8d3 b7b8 d3h7 b8c8 f7g6 c8e6").outcome==Stalemate,"stalemate");
  {Game g;g.reset();CHECK(g.pos.fromFen("8/8/8/4k3/8/8/8/4K2B w - - 0 1")&&g.pos.insufficientMaterial(),"K+B vs K");}
  {Position p;p.fromFen("8/8/2b5/4k3/8/8/8/4K2B w - - 0 1");CHECK(p.insufficientMaterial(),"bishops on one colour");p.fromFen("8/8/3b4/4k3/8/8/8/4K2B w - - 0 1");CHECK(!p.insufficientMaterial(),"bishops on both colours");p.fromFen("8/8/3n4/4k3/8/8/8/4K2N w - - 0 1");CHECK(!p.insufficientMaterial(),"two knights");}
  // Replay rejects an illegal list; moves after the end are refused.
  {Game g=play("f2f3 e7e5 g2g4 d8h4");Move m=makeMove(8,16);CHECK(!g.play(m),"move after mate");Game h;Move bad[]={makeMove(12,28),makeMove(12,28)};CHECK(!h.load(bad,2)&&h.plies==0,"illegal replay");Game k;CHECK(k.load(g.moves,g.plies)&&k.outcome==Checkmate,"replay of a finished game");}
  // En passant only counts for repetition when the capture is possible.
  {Position a,b;a.fromFen("4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1");b.fromFen("4k3/8/8/8/4P3/8/8/4K3 b - - 0 1");CHECK(a.samePlacement(b),"idle en passant square");a.fromFen("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1");b.fromFen("4k3/8/8/8/3pP3/8/8/4K3 b - - 0 1");CHECK(!a.samePlacement(b),"real en passant");}
  printf(failures?"%d failures\n":"OK chess rules\n",failures);return failures?1:0;
}
