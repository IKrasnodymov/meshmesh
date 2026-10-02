// Replays games for tools/chess/companion_check.cjs: one game of UCI moves per input line; per ply
// "SAN FEN legal-moves outcome", so the browser rules (web/chess-companion.js) are compared with these.
#include "Chess.h"
#include <stdio.h>
#include <string.h>
using namespace chess;
int main(){
  static char line[8192];
  while(fgets(line,sizeof line,stdin)){
    Game g;g.reset();char* save=nullptr;
    for(char* w=strtok_r(line," \n",&save);w;w=strtok_r(nullptr," \n",&save)){
      Move m;if(!g.pos.parseUci(w,m)){printf("illegal %s\n",w);break;}
      char san[12],fen[96];g.pos.san(m,san,sizeof san);if(!g.play(m)){printf("refused %s\n",w);break;}
      Move list[MaxMoves];g.pos.fen(fen,sizeof fen);printf("%s %s %u %d\n",san,fen,g.pos.legal(list),g.outcome);
    }
    printf("end\n");
  }
  return 0;
}
