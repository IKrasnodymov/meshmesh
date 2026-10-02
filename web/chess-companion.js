// MeshMesh Chess through a stock MeshCore companion (docs/chess.md, «Со штатной MeshCore companion»).
// The site's chess page is web/index.html with this file (tools/chess_site.py). The rules port
// src/Chess.cpp and the games src/ChessNet.cpp: the same text commands, ACKs and resending, so a game
// with an M9, a Heltec or a GAT562 on MeshMesh goes as between two boards. The radio is the
// companion's: USB (Web Serial) or Bluetooth (Web Bluetooth), frames of the MeshCore companion
// protocol (upstream/meshcore-stock/examples/companion_radio/MyMesh.cpp). The games are kept in this
// browser, separately for every companion key.
(function(){
'use strict';

// ---- Rules (src/Chess.cpp) ----
// Squares 0..63: a1=0 .. h8=63. A piece is +type for White, -type for Black.
// A move: from | to<<6 | promotion<<12; 0 is never a legal move.
const Pawn=1,Knight=2,Bishop=3,Rook=4,Queen=5,King=6,White=0,Black=1,MaxPlies=512;
const Ongoing=0,Checkmate=1,Stalemate=2,FiftyMoves=3,Repetition=4,DeadPosition=5,TooLong=6;
const KNIGHT=[[1,2],[2,1],[2,-1],[1,-2],[-1,-2],[-2,-1],[-2,1],[-1,2]],KING=[[1,0],[1,1],[0,1],[-1,1],[-1,0],[-1,-1],[0,-1],[1,-1]];
const DIAG=[[1,1],[1,-1],[-1,1],[-1,-1]],LINES=[[1,0],[-1,0],[0,1],[0,-1]],LETTERS=' PNBRQK';
const START='rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';
const onBoard=(f,r)=>f>=0&&f<8&&r>=0&&r<8,owns=(p,c)=>c===White?p>0:p<0,sign=c=>c===White?1:-1;
const makeMove=(from,to,promo=0)=>from|to<<6|promo<<12,moveFrom=m=>m&63,moveTo=m=>m>>6&63,movePromotion=m=>m>>12&7;
const square=t=>(t.charCodeAt(0)-97)+(t.charCodeAt(1)-49)*8,squareName=s=>'abcdefgh'[s&7]+((s>>3)+1);
const uci=m=>squareName(moveFrom(m))+squareName(moveTo(m))+(movePromotion(m)?'  nbrq'[movePromotion(m)]:'');

class Position{
 constructor(){this.board=new Int8Array(64);this.side=White;this.castling=0;this.enPassant=-1;this.halfmove=0;this.fullmove=1}
 clone(){const p=new Position();p.board.set(this.board);p.side=this.side;p.castling=this.castling;p.enPassant=this.enPassant;p.halfmove=this.halfmove;p.fullmove=this.fullmove;return p}
 start(){this.fromFen(START);return this}
 fromFen(fen){
  const p=new Position(),[place,side,castle,ep,half,full]=String(fen).split(' '),rows=String(place).split('/');
  if(rows.length!==8||castle===undefined||ep===undefined)return false;
  for(let i=0;i<8;i++){let f=0;for(const c of rows[i]){if(c>='1'&&c<='8'){f+=+c;if(f>8)return false;continue}const t=LETTERS.indexOf(c.toUpperCase());if(t<1||f>7)return false;p.board[(7-i)*8+f++]=c>='a'?-t:t}if(f!==8)return false}
  if(side!=='w'&&side!=='b')return false;p.side=side==='w'?White:Black;
  if(castle!=='-')for(const c of castle){const k='KQkq'.indexOf(c);if(k<0)return false;p.castling|=1<<k}
  if(ep!=='-'){if(!/^[a-h][36]$/.test(ep))return false;p.enPassant=square(ep)}
  if(half!==undefined){const h=+half,m=full===undefined?1:+full;if(!Number.isInteger(h)||h<0||h>999||!Number.isInteger(m)||m<1||m>9999)return false;p.halfmove=h;p.fullmove=m}
  if(p.king(White)<0||p.king(Black)<0)return false;
  Object.assign(this,p);return true}
 fen(){
  let s='';for(let r=7;r>=0;r--){let gap=0;for(let f=0;f<8;f++){const p=this.board[r*8+f];if(!p){gap++;continue}if(gap)s+=gap;gap=0;const l=LETTERS[Math.abs(p)];s+=p>0?l:l.toLowerCase()}if(gap)s+=gap;if(r)s+='/'}
  let c='';for(let i=0;i<4;i++)if(this.castling>>i&1)c+='KQkq'[i];
  return `${s} ${this.side===White?'w':'b'} ${c||'-'} ${this.enPassant<0?'-':squareName(this.enPassant)} ${this.halfmove} ${this.fullmove}`}
 king(color){const k=King*sign(color);for(let s=0;s<64;s++)if(this.board[s]===k)return s;return -1}
 attacked(sq,by){
  if(sq<0)return false;const f=sq&7,r=sq>>3,s=sign(by),b=this.board;
  for(const df of[-1,1])if(onBoard(f+df,r-s)&&b[(r-s)*8+f+df]===s*Pawn)return true;
  for(const[dx,dy]of KNIGHT)if(onBoard(f+dx,r+dy)&&b[(r+dy)*8+f+dx]===s*Knight)return true;
  for(const[dx,dy]of KING)if(onBoard(f+dx,r+dy)&&b[(r+dy)*8+f+dx]===s*King)return true;
  for(let k=0;k<2;k++)for(const[dx,dy]of k?LINES:DIAG){let x=f+dx,y=r+dy;while(onBoard(x,y)){const p=b[y*8+x];if(p){if(p===s*Queen||p===s*(k?Rook:Bishop))return true;break}x+=dx;y+=dy}}
  return false}
 inCheck(){return this.attacked(this.king(this.side),this.side^1)}
 pseudo(){
  const out=[],side=this.side,s=sign(side),b=this.board;
  const pawnTo=(from,to)=>{if((to>>3)===0||(to>>3)===7)for(const p of[Queen,Rook,Bishop,Knight])out.push(makeMove(from,to,p));else out.push(makeMove(from,to))};
  for(let sq=0;sq<64;sq++){
   const p=b[sq];if(!owns(p,side))continue;const type=p*s,f=sq&7,r=sq>>3;
   if(type===Pawn){
    const ahead=r+s;
    if(onBoard(f,ahead)&&!b[ahead*8+f]){pawnTo(sq,ahead*8+f);if(r===(side===White?1:6)&&!b[(r+2*s)*8+f])out.push(makeMove(sq,(r+2*s)*8+f))}
    for(const df of[-1,1]){if(!onBoard(f+df,ahead))continue;const t=ahead*8+f+df;if((b[t]&&!owns(b[t],side))||t===this.enPassant)pawnTo(sq,t)}
    continue}
   if(type===Knight||type===King){
    for(const[dx,dy]of type===Knight?KNIGHT:KING){const x=f+dx,y=r+dy;if(onBoard(x,y)&&!owns(b[y*8+x],side))out.push(makeMove(sq,y*8+x))}
    if(type===King&&sq===(side===White?4:60)&&!this.attacked(sq,side^1)){
     const base=sq-4,shortRight=side===White?1:4,longRight=side===White?2:8;
     if((this.castling&shortRight)&&b[base+7]===s*Rook&&!b[base+5]&&!b[base+6]&&!this.attacked(base+5,side^1)&&!this.attacked(base+6,side^1))out.push(makeMove(sq,base+6));
     if((this.castling&longRight)&&b[base]===s*Rook&&!b[base+1]&&!b[base+2]&&!b[base+3]&&!this.attacked(base+3,side^1)&&!this.attacked(base+2,side^1))out.push(makeMove(sq,base+2))}
    continue}
   for(let k=0;k<2;k++){
    if((k===0&&type===Rook)||(k===1&&type===Bishop))continue;
    for(const[dx,dy]of k?LINES:DIAG){let x=f+dx,y=r+dy;while(onBoard(x,y)){const t=b[y*8+x];if(owns(t,side))break;out.push(makeMove(sq,y*8+x));if(t)break;x+=dx;y+=dy}}}
  }
  return out}
 apply(m){ // no checks: only for moves from legal()
  const from=moveFrom(m),to=moveTo(m),promo=movePromotion(m),b=this.board,p=b[from],type=Math.abs(p);let capture=b[to]!==0;
  if(type===Pawn&&to===this.enPassant&&!capture){b[to-8*sign(this.side)]=0;capture=true}
  if(type===King&&Math.abs(to-from)===2){const rookFrom=to>from?to+1:to-2,rookTo=to>from?to-1:to+1;b[rookTo]=b[rookFrom];b[rookFrom]=0}
  b[to]=promo?promo*sign(this.side):p;b[from]=0;
  this.enPassant=type===Pawn&&Math.abs(to-from)===16?(from+to)/2:-1;
  for(const sq of[from,to]){if(sq===4)this.castling&=~3;if(sq===60)this.castling&=~12;if(sq===0)this.castling&=~2;if(sq===7)this.castling&=~1;if(sq===56)this.castling&=~8;if(sq===63)this.castling&=~4}
  this.halfmove=type===Pawn||capture?0:this.halfmove+1;if(this.side===Black)this.fullmove++;this.side^=1}
 legal(){return this.pseudo().filter(m=>{const n=this.clone();n.apply(m);return !n.attacked(n.king(this.side),n.side)})}
 isLegal(m){return this.legal().includes(m)}
 enPassantTarget(){
  if(this.enPassant<0)return -1;const f=this.enPassant&7,r=(this.enPassant>>3)-sign(this.side);
  for(const df of[-1,1])if(onBoard(f+df,r)&&this.board[r*8+f+df]===Pawn*sign(this.side))return this.enPassant;
  return -1}
 samePlacement(o){if(this.side!==o.side||this.castling!==o.castling||this.enPassantTarget()!==o.enPassantTarget())return false;for(let i=0;i<64;i++)if(this.board[i]!==o.board[i])return false;return true}
 insufficientMaterial(){
  let minors=0,bishops=0,colors=0;
  for(let s=0;s<64;s++){const t=Math.abs(this.board[s]);if(t===Pawn||t===Rook||t===Queen)return false;if(t===Knight)minors++;if(t===Bishop){minors++;bishops++;colors|=1<<(((s&7)+(s>>3))&1)}}
  return minors<=1||(bishops===minors&&colors!==3)} // lone minor piece, or bishops on one colour only
 parseUci(text){ // "e2e4", "e7e8q"; 0 unless legal
  if(!/^[a-h][1-8][a-h][1-8][nbrqNBRQ]?$/.test(text))return 0;
  const from=square(text),to=square(text.slice(2));let promo=0;
  if(text.length===5)promo=Knight+'nbrq'.indexOf(text[4].toLowerCase());
  else if(Math.abs(this.board[from])===Pawn&&((to>>3)===0||(to>>3)===7))promo=Queen;
  const m=makeMove(from,to,promo);return this.isLegal(m)?m:0}
 san(m){ // before the move is played: "Nbd7", "exd6", "O-O", "e8=Q+", "Qh5#"
  const from=moveFrom(m),to=moveTo(m),type=Math.abs(this.board[from]),capture=!!this.board[to]||(type===Pawn&&(from&7)!==(to&7));let s='';
  if(type===King&&Math.abs(to-from)===2)s=to>from?'O-O':'O-O-O';
  else{
   if(type!==Pawn){
    s+=LETTERS[type];let other=false,sameFile=false,sameRank=false;
    for(const x of this.legal()){const f=moveFrom(x);if(f===from||moveTo(x)!==to||Math.abs(this.board[f])!==type)continue;other=true;sameFile||=(f&7)===(from&7);sameRank||=(f>>3)===(from>>3)}
    if(other)s+=!sameFile?'abcdefgh'[from&7]:!sameRank?String((from>>3)+1):squareName(from);
   }else if(capture)s+='abcdefgh'[from&7];
   if(capture)s+='x';
   s+=squareName(to);if(movePromotion(m))s+='='+LETTERS[movePromotion(m)]}
  const next=this.clone();next.apply(m);
  if(next.inCheck())s+=next.legal().length?'+':'#';
  return s}
}
// A game from the starting position: the move list is the saved form and is replayed on load.
class Game{
 constructor(){this.reset()}
 reset(){this.moves=[];this.pos=new Position().start();this.outcome=Ongoing}
 get plies(){return this.moves.length}
 play(m){if(this.outcome!==Ongoing||this.moves.length>=MaxPlies||!this.pos.isLegal(m))return false;this.pos.apply(m);this.moves.push(m);this.outcome=this.judge();return true}
 load(list){this.reset();if(list.length>MaxPlies)return false;for(const m of list)if(!this.play(m)){this.reset();return false}return true}
 at(ply){const p=new Position().start();for(let i=0;i<ply&&i<this.moves.length;i++)p.apply(this.moves[i]);return p}
 judge(){
  const pos=this.pos,plies=this.moves.length;
  if(!pos.legal().length)return pos.inCheck()?Checkmate:Stalemate; // mate stands even on the 50th move
  if(pos.insufficientMaterial())return DeadPosition;
  if(pos.halfmove>=100)return FiftyMoves;
  if(pos.halfmove>=8){ // only positions since the last capture or pawn move can repeat
   const p=new Position().start(),first=pos.halfmove>plies?0:plies-pos.halfmove;let seen=1;
   for(let k=0;k<plies;k++){if(k>=first&&p.samePlacement(pos)&&++seen>=3)return Repetition;p.apply(this.moves[k])}}
  return plies>=MaxPlies?TooLong:Ongoing}
}

// ---- Games and commands (src/ChessNet.cpp) ----
// "♟3F2A 5 g1f3 3. Nf3": a tag, the game number, then the command; the rest is for a human reader.
const Tag='♟',Queued=1,Sent=2,Delivered=3,Failed=4,MaxMatches=12;
// Automatic resending: after 2, 5 and 10 minutes, then every 15; at once when the other player is
// heard again (at most every 2 min); stops after 24 h without confirmation ("Повторить отправку" resends).
const RetryDelays=[120000,300000,600000,900000],RetryWindow=86400000,HeardGap=120000;
const retryDelay=n=>RetryDelays[Math.min(n,3)],inFlight=s=>s===Queued||s===Sent;
const hex4=n=>n.toString(16).toUpperCase().padStart(4,'0');

class Match{
 constructor(){
  Object.assign(this,{peer:'',id:0,name:'',mine:White,state:'free',result:'',reason:'',drawOffer:'',unseen:false,started:0,updated:0,changedAt:0,game:new Game(),
   out:'',outStatus:0,outId:0,moveOpen:false,moveStatus:0,moveId:0,retryAt:0,triedAt:0,openSince:0,retries:0,autoStopped:false})}
 active(){return this.state==='inviting'||this.state==='invited'||this.state==='playing'}
 myTurn(){return this.state==='playing'&&this.game.pos.side===this.mine}
 won(){return (this.result==='white'&&this.mine===White)||(this.result==='black'&&this.mine===Black)}
 lost(){return (this.result==='white'&&this.mine===Black)||(this.result==='black'&&this.mine===White)}
 pending(){return this.moveOpen||(!!this.out&&this.outStatus!==Delivered)}
 sending(){return (this.moveOpen&&inFlight(this.moveStatus))||(!!this.out&&inFlight(this.outStatus))}
 link(){if((this.moveOpen&&this.moveStatus===Failed)||(this.out&&this.outStatus===Failed))return Failed;if(this.moveOpen)return this.moveStatus;return this.out?this.outStatus:0}
}

// env: send(peer,text) -> message id or 0, contact(peer) -> {name,type,heardAt}|null, save(json),
// now() in ms, unix() in s, random(n).
class ChessNet{
 constructor(env){this.env=env;this.matches=[];this.event='';this.events=0;this.onChange=null}
 note(text){this.event=text;this.events++}
 news(m,text){this.note(text);m.unseen=true}
 changed(m){m.changedAt=this.env.now();m.updated=this.env.unix();this.save()}
 tick(){
  const now=this.env.now();
  for(const m of this.matches){
   if(!m.pending()||m.sending()||m.autoStopped)continue;
   if(!m.openSince)m.openSince=now;
   if(now-m.openSince>RetryWindow){m.autoStopped=true;m.retryAt=0;this.note(m.name+': нет подтверждения 24 ч');continue}
   const p=this.env.contact(m.peer),heard=p&&p.heardAt>m.triedAt&&now-m.triedAt>=HeardGap; // they are on air again
   if(!m.retryAt&&!heard){m.retryAt=now+retryDelay(m.retries);continue}
   if(heard||now>=m.retryAt)this.retry(m)}}
 // The companion is back: what was not delivered goes again soon, as after a restart of a board.
 linked(){const now=this.env.now();for(const m of this.matches)if(m.pending()&&!m.sending()&&!m.autoStopped)m.retryAt=Math.min(m.retryAt||Infinity,now+3000)}
 retryIn(m){if(!m.pending()||m.sending()||m.autoStopped||!m.retryAt)return -1;const left=m.retryAt-this.env.now();return left>0?Math.ceil(left/1000):0}
 // One command per attempt: the open move first, then the last other command.
 retry(m){
  m.retries=Math.min(m.retries+1,255);m.retryAt=0;
  if(m.moveOpen&&!inFlight(m.moveStatus)&&m.moveStatus!==Delivered)this.sendMove(m);
  else if(m.out&&m.outStatus===Failed)this.send(m,m.out)}
 confirmed(m){if(m.pending())return;m.retries=0;m.retryAt=0;m.openSince=0;m.autoStopped=false}
 find(id,peer){return this.matches.find(m=>m.id===id&&(peer===undefined||m.peer===peer))}
 waiting(){return this.matches.filter(m=>m.state==='invited'||m.myTurn()||m.unseen).length}
 // A free place, or the finished game that changed longest ago.
 slot(){
  if(this.matches.length<MaxMatches){const m=new Match();this.matches.push(m);return m}
  let oldest=null;for(const m of this.matches)if(m.state==='over'&&!m.unseen&&(!oldest||m.changedAt<oldest.changedAt))oldest=m;
  if(oldest)this.matches.splice(this.matches.indexOf(oldest),1,new Match());
  return oldest?this.matches.find(m=>m.state==='free'):null}
 send(m,text){m.out=text;m.outId=this.env.send(m.peer,text);m.outStatus=m.outId?Queued:Failed;m.triedAt=this.env.now();return m.outId}
 moveText(m){const g=m.game,before=g.at(g.plies-1),mv=g.moves[g.plies-1];return `${Tag}${hex4(m.id)} ${g.plies} ${uci(mv)} ${before.fullmove}.${before.side===White?'':'..'} ${before.san(mv)}`}
 // The move is also the last command when it was sent last; a resend does not overwrite a later
 // command (a draw offer or resignation) that is still waiting for its own confirmation.
 sendMove(m){
  const last=!m.out||m.outId===m.moveId||m.outStatus===Delivered,text=this.moveText(m);
  if(last){this.send(m,text);m.moveId=m.outId;m.moveStatus=m.outStatus;return m.moveId}
  m.moveId=this.env.send(m.peer,text);m.moveStatus=m.moveId?Queued:Failed;m.triedAt=this.env.now();return m.moveId}
 delivery(id,status){
  for(const m of this.matches){
   let hit=false;
   if(m.moveOpen&&m.moveId===id){m.moveStatus=status;m.moveOpen=status!==Delivered;hit=true}
   if(m.outId===id&&m.outStatus!==Delivered){m.outStatus=status;hit=true}
   if(!hit)continue;
   if(status===Delivered){this.confirmed(m);if(m.pending())m.retryAt=this.env.now()} // the next open command goes at once
   if(status===Failed&&!m.retries)this.note(m.name+': не подтвердил, повторю автоматически');
   this.changed(m);return}}
 // Their move answers my last move, and "yes" my challenge: those arrived even when the ACK was lost.
 received(m,move){
  if(move){m.moveOpen=false;m.moveStatus=Delivered}
  const at=m.out.indexOf(' ');
  if(at>=0&&m.outStatus!==Delivered){const verb=m.out.slice(at+1);if(move?(/^[1-9]/.test(verb)||verb.startsWith('yes')):(verb.startsWith('new ')||verb.startsWith('draw?')))m.outStatus=Delivered}
  this.confirmed(m)}
 finish(m,result,reason){m.state='over';m.result=result;m.reason=reason;m.drawOffer=''}
 judge(m){
  const g=m.game;if(g.outcome===Ongoing)return;
  this.finish(m,g.outcome===Checkmate?(g.pos.side===White?'black':'white'):'draw',['','mate','stalemate','fifty','repetition','material','too_long'][g.outcome])}
 // Commands from the other player. Only this contact's own games are touched; anything out of turn,
 // illegal or repeated is ignored (a repeat arrives when an ACK was lost and the sender tried again).
 receive(from,name,text){
  if(!text.startsWith(Tag))return false;
  const c=text.slice(1);if(!/^[0-9A-Fa-f]{4} /.test(c))return false;const id=parseInt(c.slice(0,4),16);if(!id)return false;
  const words=c.slice(5).trim().split(/\s+/),a=(words[0]||'').slice(0,11),b=(words[1]||'').slice(0,7);if(!a)return false;
  let m=this.find(id,from);
  if(a==='new'){
   if(b[0]!=='w'&&b[0]!=='b')return false;if(m)return true;
   m=this.slot();
   if(!m){this.env.send(from,`${Tag}${c.slice(0,4)} no`);this.note(name+': вызов отклонён, нет свободной доски');return true}
   Object.assign(m,{peer:from,id,name,mine:b[0]==='w'?Black:White,state:'invited',started:this.env.unix()});m.game.reset();
   this.news(m,name+' вызывает на партию в шахматы');this.changed(m);return true}
  if(!m){this.note('Шахматы: ход в неизвестной партии');return true}
  m.name=name;const g=m.game,theirs=m.mine^1;
  if(a[0]>='1'&&a[0]<='9'){
   const ply=parseInt(a,10);
   if(m.state==='inviting'&&m.mine===Black&&ply===1)m.state='playing'; // their first move accepts my challenge
   if(m.state!=='playing')return true;
   if(ply&&ply===g.plies&&((ply-1)&1)===theirs){const was=uci(g.moves[ply-1]);if(was===b||(b.length===4&&was.startsWith(b)))return true} // repeat
   const mv=ply===g.plies+1&&g.pos.side===theirs?g.pos.parseUci(b):0;
   if(!mv){this.note(`${m.name}: ход не принят (${b})`);return true}
   const san=g.pos.san(mv);g.play(mv);this.received(m,true);if(m.drawOffer==='mine')m.drawOffer='';
   this.judge(m);
   this.news(m,`${m.name}: ${san}`+(m.state==='over'?(m.won()?', вы победили':m.lost()?', вы проиграли':', ничья'):g.pos.inCheck()?', шах, ваш ход':', ваш ход'));this.changed(m);return true}
  if(a==='yes'){if(m.state!=='inviting')return true;m.state='playing';this.received(m,false);this.news(m,m.name+' принял вызов: играем');this.changed(m);return true}
  if(a==='no'){
   if(m.state==='inviting'){this.finish(m,'','declined');this.news(m,m.name+' отказался от партии')}
   else if(m.state==='invited'||(m.state==='playing'&&!g.plies)){this.finish(m,'','cancelled');this.news(m,m.name+' отменил партию')}
   else return true;
   this.changed(m);return true}
  if(a==='draw?'){if(m.state!=='playing'||m.drawOffer==='theirs')return true;if(m.drawOffer==='mine'){this.finish(m,'draw','agreed');this.news(m,'Ничья по соглашению')}else{m.drawOffer='theirs';this.news(m,m.name+' предлагает ничью')}this.changed(m);return true}
  if(a==='draw'){if(m.state!=='playing'||m.drawOffer!=='mine')return true;this.received(m,false);this.finish(m,'draw','agreed');this.news(m,m.name+' согласился на ничью');this.changed(m);return true}
  if(a==='resign'){if(m.state!=='playing')return true;this.finish(m,m.mine===White?'white':'black','resigned');this.news(m,m.name+' сдался: вы победили');this.changed(m);return true}
  return true} // a newer command this version does not know
 // The player's actions. Each one changes the game here first and then sends one command.
 invite(peer,color){
  const p=this.env.contact(peer);if(!p||p.type!==1){this.note('Шахматы: выберите чат-контакт');return null}
  const m=this.slot();if(!m){this.note('Шахматы: все доски заняты, завершите партию');return null}
  let id;do id=1+this.env.random(0xffff);while(this.matches.some(x=>x!==m&&x.id===id));
  Object.assign(m,{peer,id,name:p.name,mine:color===2?this.env.random(2):color&1,state:'inviting',started:this.env.unix()});m.game.reset();
  this.send(m,`${Tag}${hex4(id)} new ${m.mine===White?'w':'b'} · шахматы MeshMesh: вы играете ${m.mine===White?'чёрными':'белыми'}`);
  this.changed(m);return m}
 accept(m){if(m.state!=='invited')return false;m.state='playing';m.unseen=false;this.send(m,`${Tag}${hex4(m.id)} yes`);this.changed(m);return true}
 decline(m){
  if(m.state!=='invited'&&m.state!=='inviting'&&!(m.state==='playing'&&!m.game.plies))return false;
  this.finish(m,'',m.state==='invited'?'declined':'cancelled');m.unseen=false;this.send(m,`${Tag}${hex4(m.id)} no`);this.changed(m);return true}
 move(m,mv){
  if(!m.myTurn()||!m.game.play(mv))return false;
  if(m.drawOffer==='theirs')m.drawOffer='';this.judge(m);
  Object.assign(m,{moveOpen:true,retries:0,retryAt:0,openSince:0,autoStopped:false,moveId:m.outId});this.sendMove(m);this.changed(m);return true} // a new move is the last command
 offerDraw(m){if(m.state!=='playing'||m.drawOffer)return false;m.drawOffer='mine';this.send(m,`${Tag}${hex4(m.id)} draw?`);this.changed(m);return true}
 acceptDraw(m){if(m.state!=='playing'||m.drawOffer!=='theirs')return false;this.finish(m,'draw','agreed');this.send(m,`${Tag}${hex4(m.id)} draw`);this.changed(m);return true}
 resign(m){if(m.state!=='playing')return false;this.finish(m,m.mine===White?'black':'white','resigned');this.send(m,`${Tag}${hex4(m.id)} resign`);this.changed(m);return true}
 resend(m){if(m.link()!==Failed||m.sending())return false;m.autoStopped=false;m.openSince=this.env.now();this.retry(m);this.changed(m);return m.link()!==Failed}
 remove(m){if(m.active())return false;this.matches.splice(this.matches.indexOf(m),1);this.save();return true}
 viewed(m){if(m.unseen){m.unseen=false;this.changed(m)}}

 // The page's JSON: the same fields as /api/chess and /api/chess?id= of the boards.
 json(){
  return this.matches.filter(m=>m.state!=='free').map(m=>{
   const g=m.game,o={id:hex4(m.id),peer:m.peer,name:m.name,state:m.state,color:m.mine===White?'white':'black',plies:g.plies,fen:g.pos.fen()};
   if(g.plies){o.last=uci(g.moves[g.plies-1]);o.last_san=g.at(g.plies-1).san(g.moves[g.plies-1])}
   return Object.assign(o,{my_turn:m.myTurn(),result:m.result,reason:m.reason,draw_offer:m.drawOffer,unseen:m.unseen,out_status:m.link(),updated:m.updated,check:g.pos.inCheck(),
    move_open:m.moveOpen,retry_in:this.retryIn(m),retries:m.retries,auto_stopped:m.autoStopped})})}
 web(){return {events:this.events,event:this.event,waiting:this.waiting(),games:this.json()}}
 detail(m){
  const p=new Position().start(),san=[];for(const mv of m.game.moves){san.push(p.san(mv));p.apply(mv)}
  return {id:hex4(m.id),san,legal:m.myTurn()?m.game.pos.legal().map(uci):[],check:m.game.pos.inCheck()}}
 command(line){ // the board's USB/HTTP command "chess ...", as the page sends it
  const words=line.trim().split(/\s+/),verb=words[1]||'';
  if(verb==='invite'){
   const peer=String(words[2]||'').toUpperCase(),color=words[3]||'r';if(!/^[0-9A-F]{12}$/.test(peer))return 'ERR chess invite NODE_ID w|b|r';
   const m=this.invite(peer,color==='w'?White:color==='b'?Black:2);if(!m)return 'ERR '+this.event;return `OK game ${hex4(m.id)} ${m.mine===White?'white':'black'}`}
  const gid=words[2]||'',m=/^[0-9A-Fa-f]{4}$/.test(gid)?this.find(parseInt(gid,16)):null;
  if(!m)return 'ERR chess invite|accept|decline|move|draw|resign|resend|remove|show|seen GAME_ID ...';
  let done=false;
  if(verb==='accept')done=this.accept(m);
  else if(verb==='decline')done=this.decline(m);
  else if(verb==='move'){const mv=m.myTurn()?m.game.pos.parseUci(words[3]||''):0;done=!!mv&&this.move(m,mv)}
  else if(verb==='draw')done=m.drawOffer==='theirs'?this.acceptDraw(m):this.offerDraw(m);
  else if(verb==='resign')done=this.resign(m);
  else if(verb==='resend')done=this.resend(m);
  else if(verb==='remove')done=this.remove(m);
  else if(verb==='show')return this.detail(m);
  else if(verb==='seen'){this.viewed(m);return 'OK seen'}
  else return 'ERR unknown chess command';
  return done?'OK '+verb:'ERR '+verb+' not possible now'}

 // Saved per companion: the move lists are replayed, a damaged game is dropped rather than shown wrong.
 save(){
  const games=this.matches.filter(m=>m.state!=='free').map(m=>({peer:m.peer,id:m.id,name:m.name,mine:m.mine,state:m.state,result:m.result,reason:m.reason,drawOffer:m.drawOffer,unseen:m.unseen,
   started:m.started,updated:m.updated,out:m.out,outStatus:m.outStatus,moveOpen:m.moveOpen,moves:m.game.moves.map(uci)}));
  this.env.save(JSON.stringify({v:1,games}));if(this.onChange)this.onChange()}
 load(text){
  this.matches=[];let data;try{data=JSON.parse(text)}catch{return 0}
  let dropped=0;const now=this.env.now();
  for(const s of (data&&data.v===1&&Array.isArray(data.games)?data.games:[]).slice(0,MaxMatches)){
   const m=new Match(),moves=[],p=new Position().start();let ok=Array.isArray(s.moves)&&['inviting','invited','playing','over'].includes(s.state)&&/^[0-9A-F]{12}$/.test(s.peer)&&s.id>0&&s.id<=0xffff;
   for(const t of ok?s.moves:[]){const mv=p.parseUci(String(t));if(!mv){ok=false;break}moves.push(mv);p.apply(mv)}
   if(!ok||!m.game.load(moves)){dropped++;continue}
   Object.assign(m,{peer:s.peer,id:s.id,name:String(s.name||s.peer),mine:s.mine&1,state:s.state,result:s.result||'',reason:s.reason||'',drawOffer:s.drawOffer||'',unseen:!!s.unseen,
    started:s.started|0,updated:s.updated|0,out:String(s.out||''),outStatus:s.outStatus|0,moveOpen:!!s.moveOpen&&moves.length>0});
   if(m.out&&m.outStatus!==Delivered)m.outStatus=Failed; // unconfirmed before the page closed: not delivered
   if(m.moveOpen)m.moveStatus=Failed;
   if(m.pending()){m.openSince=now;m.retryAt=now+60000} // resend a minute after the start
   m.changedAt=now-(1000-this.matches.length);this.matches.push(m)}
  if(dropped)this.note('Шахматы: повреждённая партия удалена');
  return dropped}
}

// ---- MeshCore companion protocol (MyMesh.cpp) ----
const CMD={appStart:1,sendText:2,getContacts:4,getTime:5,setTime:6,advert:7,syncNext:10,resetPath:13,deviceQuery:22};
const RESP={ok:0,err:1,contactsStart:2,contact:3,contactsEnd:4,selfInfo:5,sent:6,msg:7,channelMsg:8,time:9,noMore:10,deviceInfo:13,msgV3:16,channelMsgV3:17};
const PUSH={advert:0x80,path:0x81,ack:0x82,waiting:0x83,newAdvert:0x8A};
const MaxFrame=176,AppVersion=3;
const utf8=new TextEncoder(),text8=new TextDecoder();
const toHex=b=>Array.from(b,x=>x.toString(16).padStart(2,'0')).join('').toUpperCase();
const u32=(b,i)=>(b[i]|b[i+1]<<8|b[i+2]<<16|b[i+3]<<24)>>>0;
const le32=n=>[n&255,n>>>8&255,n>>>16&255,n>>>24&255];
const cstr=b=>{const z=b.indexOf(0);return text8.decode(z<0?b:b.subarray(0,z))};
const sleep=ms=>new Promise(r=>setTimeout(r,ms));

// A byte stream (USB serial or TCP in the checks): frames "<" + length LE16 + data to the companion,
// ">" + length + data back. Bytes before ">" (a boot log) are skipped.
class StreamFramer{
 constructor(onFrame){this.onFrame=onFrame;this.buf=new Uint8Array(0)}
 static wrap(data){const f=new Uint8Array(data.length+3);f[0]=0x3C;f[1]=data.length&255;f[2]=data.length>>8;f.set(data,3);return f}
 push(chunk){
  const b=new Uint8Array(this.buf.length+chunk.length);b.set(this.buf);b.set(chunk,this.buf.length);let i=0;
  while(i<b.length){
   if(b[i]!==0x3E){i++;continue}
   if(b.length-i<3)break;const n=b[i+1]|b[i+2]<<8;
   if(!n||n>MaxFrame){i++;continue}
   if(b.length-i<3+n)break;
   this.onFrame(b.slice(i+3,i+3+n));i+=3+n}
  this.buf=b.slice(i)}
}

// The companion link: one command at a time, each answered by the next frame below 0x80; pushes
// (0x80 and up) come at any time. Text messages are sent with the official app's attempts: up to
// three, the third by flood after the path is reset.
class Companion{
 constructor(){
  this.transport=null;this.kind='';this.queue=Promise.resolve();this.pending=null;this.contactsIn=null;
  this.self=null;this.device=null;this.contacts=new Map();this.heard=new Map();this.sends=new Map();this.nextId=1;
  this.clock={unix:0,at:0};this.syncing=false;this.resync=false;this.connected=false;
  this.onMessage=null;this.onDelivery=null;this.onContacts=null;this.onState=null;this.onOther=null}
 // transport: {write(Uint8Array) -> Promise, close()}; frames come to receive().
 async attach(transport,kind){
  this.transport=transport;this.kind=kind;this.connected=true;
  try{
   this.device=await this.command([CMD.deviceQuery,AppVersion],[RESP.deviceInfo]);
   const app=utf8.encode('MeshMesh Chess');this.self=this.parseSelf(await this.command([CMD.appStart,AppVersion,0,0,0,0,0,0,...app],[RESP.selfInfo]));
   await this.command([CMD.setTime,...le32(Math.floor(Date.now()/1000))],[RESP.ok,RESP.err]).catch(()=>{}); // as the MeshCore app: only forward
   await this.loadContacts();
  }catch(e){this.detach();throw e}
  if(this.onState)this.onState()}
 detach(){
  if(!this.connected)return;this.connected=false;
  if(this.pending){this.pending.reject(Error('Нет связи с companion'));this.pending=null}
  for(const s of this.sends.values())if(inFlight(s.status)){clearTimeout(s.timer);this.status(s,Failed)}
  try{this.transport&&this.transport.close()}catch{}
  this.transport=null;if(this.onState)this.onState()}
 parseSelf(f){return {key:toHex(f.subarray(4,36)),txPower:f[2],freq:u32(f,48)/1000,bw:u32(f,52)/1000,sf:f[56],cr:f[57],name:text8.decode(f.subarray(58))}}
 deviceInfo(){const f=this.device;return f?{model:cstr(f.subarray(20,60)),version:cstr(f.subarray(60,80))}:null}
 command(data,expect,timeout=6000){
  const run=()=>new Promise((resolve,reject)=>{
   if(!this.connected){reject(Error('Нет связи с companion'));return}
   const timer=setTimeout(()=>{if(this.pending&&this.pending.timer===timer){this.pending=null;reject(Error('Companion не ответил'))}},timeout);
   this.pending={expect,resolve:f=>{clearTimeout(timer);resolve(f)},reject:e=>{clearTimeout(timer);reject(e)},timer};
   this.transport.write(new Uint8Array(data)).catch(e=>{if(this.pending&&this.pending.timer===timer){this.pending=null;clearTimeout(timer);reject(e)}})});
  const p=this.queue.then(run,run);this.queue=p.catch(()=>{});return p}
 receive(f){
  if(!f.length)return;const code=f[0];
  if(code>=0x80){this.push(f);return}
  if(this.contactsIn&&(code===RESP.contact||code===RESP.contactsStart)){if(code===RESP.contact)this.contactsIn.push(f);return}
  const p=this.pending;if(!p)return;
  if(p.expect.includes(code)||code===RESP.err){this.pending=null;if(code===RESP.err&&!p.expect.includes(RESP.err))p.reject(Error('Ошибка companion '+(f[1]??'')));else p.resolve(f)}}
 push(f){
  const code=f[0];
  if(code===PUSH.ack){const ack=u32(f,1);for(const s of this.sends.values())if(s.acks.includes(ack)){clearTimeout(s.timer);if(s.status!==Delivered)this.status(s,Delivered)}return}
  if(code===PUSH.waiting){this.sync();return}
  if(code===PUSH.advert||code===PUSH.path||code===PUSH.newAdvert){if(f.length>=33)this.heard.set(toHex(f.subarray(1,7)),Date.now());this.contactsSoon();return}}
 contactsSoon(){clearTimeout(this.contactsTimer);this.contactsTimer=setTimeout(()=>this.loadContacts().catch(()=>{}),1500)}
 async loadContacts(){
  const list=[];this.contactsIn=list;
  try{await this.command([CMD.getContacts],[RESP.contactsEnd],15000)}finally{this.contactsIn=null}
  try{const t=await this.command([CMD.getTime],[RESP.time]);this.clock={unix:u32(t,1),at:Date.now()}}catch{}
  const map=new Map();
  for(const f of list){if(f.length<148)continue;const key=toHex(f.subarray(1,33));
   map.set(key.slice(0,12),{id:key.slice(0,12),key,type:f[33],pathLength:f[35],name:cstr(f.subarray(100,132))||key.slice(0,8),lastmod:u32(f,144)})}
  this.contacts=map;if(this.onContacts)this.onContacts()}
 contact(id){const c=this.contacts.get(id);return c?{...c,heardAt:this.heard.get(id)||0}:null}
 // The page's peers: as /api/nodes, with the age from the companion's own clock.
 peers(){
  const now=this.clock.unix?this.clock.unix+(Date.now()-this.clock.at)/1000:0;
  return [...this.contacts.values()].map(c=>{const heardAt=this.heard.get(c.id),age=heardAt?(Date.now()-heardAt)/1000:now&&c.lastmod?Math.max(0,now-c.lastmod):0;
   return {id:c.id,name:c.name,type:c.type,heard:!!heardAt||(!!now&&c.lastmod>1700000000&&c.lastmod<=now+60),age_seconds:age,path_length:c.pathLength}})}
 async sync(){
  if(this.syncing){this.resync=true;return}this.syncing=true;
  try{do{this.resync=false;
   for(let n=0;n<64&&this.connected;n++){
    const f=await this.command([CMD.syncNext],[RESP.noMore,RESP.msg,RESP.msgV3,RESP.channelMsg,RESP.channelMsgV3]);
    if(f[0]===RESP.noMore)break;this.message(f)}}while(this.resync&&this.connected)}
  catch{}finally{this.syncing=false}}
 message(f){
  if(f[0]===RESP.msg||f[0]===RESP.msgV3){
   const i=f[0]===RESP.msgV3?4:1,from=toHex(f.subarray(i,i+6)),type=f[i+7],text=text8.decode(f.subarray(i+12+(type===2?4:0)));
   if(type===1)return; // command data for a server
   this.heard.set(from,Date.now());const c=this.contacts.get(from);
   if(this.onMessage)this.onMessage(from,c?c.name:from,text)}
  else if(this.onOther){const i=f[0]===RESP.channelMsgV3?4:1;this.onOther('',`Канал ${f[i]}`,text8.decode(f.subarray(i+7)))}}
 // Returns the message id; the status goes to onDelivery(id, Queued/Sent/Delivered/Failed).
 sendText(peer,text){
  if(!this.connected||!this.contacts.has(peer))return 0;
  const s={id:this.nextId++,peer,text,ts:Math.floor(Date.now()/1000),attempt:0,acks:[],status:Queued,direct:false,timer:0};
  this.sends.set(s.id,s);if(this.sends.size>64)this.sends.delete(this.sends.keys().next().value); // late ACKs of recent messages still count
  this.attempt(s);return s.id}
 async attempt(s){
  const c=this.contacts.get(s.peer);
  try{
   if(s.attempt===2&&s.direct&&c)await this.command([CMD.resetPath,...hexBytes(c.key)],[RESP.ok,RESP.err]); // the third goes by flood
   const r=await this.command([CMD.sendText,0,s.attempt,...le32(s.ts),...hexBytes(s.peer),...utf8.encode(s.text)],[RESP.sent]);
   s.direct=r[1]===0;s.acks.push(u32(r,2));if(s.status!==Delivered)this.status(s,Sent);
   s.timer=setTimeout(()=>this.next(s),this.waitFor(u32(r,6)));
  }catch{if(this.connected)s.timer=setTimeout(()=>this.next(s),3000);else if(s.status!==Delivered)this.status(s,Failed)}}
 waitFor(estimate){return Math.max(8000,estimate*1.25+2000)} // the companion's estimate for the ACK, with a margin
 next(s){if(s.status===Delivered||s.status===Failed)return;if(++s.attempt<3&&this.connected)this.attempt(s);else this.status(s,Failed)}
 status(s,v){s.status=v;if(this.onDelivery)this.onDelivery(s.id,v)}
 advert(){return this.command([CMD.advert,1],[RESP.ok])} // flood: the other player learns this node
}
const hexBytes=h=>h.match(/../g).map(x=>parseInt(x,16));

// Transports of the browser.
async function serialTransport(link,port){
 await port.open({baudRate:115200});
 // ESP32-S3 USB Serial/JTAG and nRF52 TinyUSB: DTR on, RTS off (no reset), as tools/device.py.
 try{await port.setSignals({dataTerminalReady:true,requestToSend:false})}catch{}
 const framer=new StreamFramer(f=>link.receive(f)),writer=port.writable.getWriter(),reader=port.readable.getReader();let open=true;
 (async()=>{try{for(;;){const {value,done}=await reader.read();if(done)break;if(value)framer.push(value)}}catch{}open=false;link.detach()})();
 return {write:async d=>{if(!open)throw Error('Порт закрыт');await writer.write(StreamFramer.wrap(d))},
  close:async()=>{open=false;try{await reader.cancel()}catch{}try{reader.releaseLock();writer.releaseLock()}catch{}try{await port.close()}catch{}}}}
const NUS='6e400001-b5a3-f393-e0a9-e50e24dcca9e',NUS_RX='6e400002-b5a3-f393-e0a9-e50e24dcca9e',NUS_TX='6e400003-b5a3-f393-e0a9-e50e24dcca9e';
async function bleTransport(link,device){
 const server=await device.gatt.connect(),service=await server.getPrimaryService(NUS),rx=await service.getCharacteristic(NUS_RX),tx=await service.getCharacteristic(NUS_TX);
 tx.addEventListener('characteristicvaluechanged',e=>{const v=e.target.value;link.receive(new Uint8Array(v.buffer,v.byteOffset,v.byteLength).slice())});
 await tx.startNotifications();
 const gone=()=>{device.removeEventListener('gattserverdisconnected',gone);link.detach()};device.addEventListener('gattserverdisconnected',gone);
 return {write:d=>rx.writeValueWithResponse?rx.writeValueWithResponse(d):rx.writeValue(d),close:()=>{device.removeEventListener('gattserverdisconnected',gone);try{device.gatt.disconnect()}catch{}}}}

// Games of one companion, tied to its link and kept in storage under its key.
function player(link,storage){
 const key=()=>'mm-chess-'+(link.self?link.self.key.slice(0,12):'none');
 const net=new ChessNet({send:(peer,text)=>link.sendText(peer,text),contact:id=>link.contact(id),save:j=>storage.set(key(),j),
  now:()=>Date.now(),unix:()=>Math.floor(Date.now()/1000),random:n=>Math.floor(Math.random()*n)});
 link.onMessage=(from,name,text)=>{if(!net.receive(from,name,text)&&link.onOther)link.onOther(from,name,text)};
 link.onDelivery=(id,status)=>net.delivery(id,status);
 // Another companion brings its own games; the same one keeps those in memory, which are newer.
 let loaded='';
 return {net,load(){const k=key();if(k!==loaded){loaded=k;net.load(storage.get(k)||'')}}}}

const api={Position,Game,ChessNet,Companion,StreamFramer,player,uci,Delivered,Failed,Sent,Queued};
if(typeof module!=='undefined'&&module.exports){module.exports=api;return}
globalThis.MeshMeshChess=api;

// ---- The page (web/index.html): only the chess pages, the radio is the companion ----
if(typeof document==='undefined'||!document.getElementById('p-chess'))return;
const storage={get:k=>{try{return localStorage.getItem(k)}catch{return null}},set:(k,v)=>{try{localStorage.setItem(k,v)}catch{}}};
const link=new Companion(),game=player(link,storage),net=game.net,others=[];
let lastPort=null,lastDevice=null,busy=false,drawTimer=0;
const hasSerial=!!navigator.serial,hasBle=!!navigator.bluetooth;
document.title='MeshMesh Chess';
// The boards' page names the tab MeshMesh (with the number of games waiting); here it is MeshMesh Chess.
const boardRefresh=refreshChess;refreshChess=async(...a)=>{await boardRefresh(...a);document.title=document.title.replace(/MeshMesh$/,'MeshMesh Chess')};
const style=document.createElement('style');
style.textContent='.cmp{display:flex;flex-wrap:wrap;gap:6px 10px;align-items:center;padding:12px;margin-bottom:10px}.cmp .who{flex:1 1 220px;min-width:0}.cmp .acts{display:flex;gap:6px;margin-left:auto}.cmp .who b{display:block}.cmp .who small{color:var(--dim);display:block}.cmp .btn{min-height:34px;padding:6px 10px}.cmpwhy{margin:6px 4px 0}.others .row{min-height:44px;cursor:default}.cmpbtns{display:grid;gap:8px}';
document.head.appendChild(style);
// The start screen: connect instead of the Wi-Fi password.
const login=$('login');for(const el of login.querySelectorAll('form,details'))el.hidden=true;
login.querySelector('.brand small').textContent='Шахматы через MeshCore companion';
const box=document.createElement('div');box.className='card pad';
box.innerHTML=`<p class="small muted">Подключите устройство со штатной прошивкой MeshCore Companion (USB или Bluetooth). Ходы уходят обычными личными сообщениями MeshCore; соперник играет на MeshMesh (M9, Heltec, GAT562) или здесь же.</p>
<div class="cmpbtns"><button class="btn primary" id="cmpUsb"${hasSerial?'':' disabled'}>${ic('bolt')}USB (Web Serial)</button><button class="btn soft" id="cmpBle"${hasBle?'':' disabled'}>${ic('ble')}Bluetooth</button></div>
<p class="small muted cmpwhy" id="cmpWhy">${hasSerial||hasBle?'Пока страница подключена, штатное приложение MeshCore к этому устройству не подключайте: сообщения заберёт кто-то один.':'Этот браузер не умеет Web Serial и Web Bluetooth. Откройте страницу в Chrome или Edge на компьютере либо в Chrome на Android.'}</p>`;
login.insertBefore(box,login.querySelector('.foot'));
// The link card above the games, and messages that were not chess (they leave the companion's queue here).
const card=document.createElement('div');card.className='card cmp';const other=document.createElement('div');
$('p-chess').insertBefore(card,$('p-chess').firstChild);$('p-chess').appendChild(other);
const hint=$('p-chess').querySelector('p.faint');if(hint)hint.textContent='Ходы идут личными сообщениями MeshCore с подтверждением доставки и проходят через ретрансляторы. Соперник играет на MeshMesh (M9, Heltec, GAT562) или на такой же странице. Партии хранятся в этом браузере отдельно для каждого companion; ходы соперника, пришедшие без страницы, ждут в очереди companion (до 16 сообщений).';

function draw(){clearTimeout(drawTimer);drawTimer=setTimeout(()=>{if(auth)refreshChess(false)},150)}
function peersNow(){peers=link.peers();fetchedAt=Date.now()}
function renderCard(){
 const s=link.self,on=link.connected,d=link.deviceInfo();
 card.innerHTML=`${ic(link.kind==='ble'?'ble':'bolt',on?'ok':'bad')}<span class="who"><b>${esc(s?s.name:'Companion')}</b><small>${on?`${link.kind==='ble'?'Bluetooth':'USB'} · ${s.freq.toFixed(3)} МГц · SF${s.sf} · BW ${s.bw} · CR 4/${s.cr}${d&&d.version?' · '+esc(d.version):''}`:'нет связи — ходы ждут подключения'}</small></span>`
  +`<span class="acts">${on?`<button class="btn" data-cmp="advert" title="Объявить себя по сети">Объявить</button><button class="btn" data-cmp="off">Отключить</button>`:`<button class="btn primary" data-cmp="again">Подключить</button>`}</span>`;
 other.innerHTML=others.length?`<h3>Другие сообщения</h3><div class="list others">${others.map(m=>`<div class="row">${avatar(m.from||'0',m.name,m.from?1:0)}<span class="main"><b>${esc(m.name)}</b><small>${esc(m.text)}</small></span><span class="side"><span>${timeText(m.at)}</span></span></div>`).join('')}</div><p class="small muted cmpwhy">Эти сообщения companion отдал странице; в приложении MeshCore их уже не будет.</p>`:''}
card.addEventListener('click',async e=>{const b=e.target.closest('[data-cmp]');if(!b)return;const a=b.dataset.cmp;
 if(a==='advert'){try{await link.advert();notify('Объявление отправлено по сети','ok')}catch(err){notify(err.message,'bad')}}
 else if(a==='off'){lastPort=null;lastDevice=null;link.detach()}
 else if(a==='again')connect(link.kind||'usb',true)});
link.onState=()=>{renderCard();if(link.connected)peersNow();else notify('Связь с companion потеряна','warn');hud();draw()};
link.onContacts=()=>{peersNow();renderCard();draw()};
link.onOther=(from,name,text)=>{others.unshift({from,name,text,at:Math.floor(Date.now()/1000)});others.length=Math.min(others.length,20);notify(`${name}: ${text}`,'info');renderCard()};
net.onChange=draw;

// The page asks for /api/chess and sends "chess ..." commands: answered here, as a board would.
companion={
 async request(path,data){
  if(path==='/api/chess')return net.web();
  if(path.startsWith('/api/chess?id=')){const m=net.find(parseInt(path.slice(14),16));if(!m)throw Error('Нет такой партии');return net.detail(m)}
  if(path==='/api/command'&&data&&/^chess( |$)/.test(data.command))return net.command(data.command);
  throw Error('Недоступно через companion')},
 icons(){return `<span class="${link.connected?'ok':'bad'}" title="${link.connected?'Companion подключён':'Нет связи'}">${ic(link.kind==='ble'?'ble':'bolt')}</span><span class="clock" id="clock">${clockText()}</span>`}};

async function connect(kind,again){
 if(busy)return;busy=true;
 try{
  if(link.connected)link.detach();
  let transport;
  if(kind==='ble'){
   const device=again&&lastDevice?lastDevice:await navigator.bluetooth.requestDevice({filters:[{services:[NUS]},{namePrefix:'MeshCore'}],optionalServices:[NUS]});
   lastDevice=device;notify('Подключаюсь по Bluetooth…','muted');transport=await bleTransport(link,device)}
  else{
   const port=again&&lastPort?lastPort:await navigator.serial.requestPort();lastPort=port;notify('Подключаюсь по USB…','muted');transport=await serialTransport(link,port)}
  await link.attach(transport,kind);
  game.load();net.linked();link.sync();config={name:link.self.name,utc_offset:-new Date().getTimezoneOffset()};clockBase={unix:Math.floor(Date.now()/1000),at:Date.now()};
  auth='companion';$('login').hidden=true;$('app').hidden=false;
  for(const v of document.querySelectorAll('.version'))v.textContent='Chess · '+(link.deviceInfo()?.version||'MeshCore');
  peersNow();renderCard();await refreshChess(true);route='';if(!/^#(chess|board)/.test(location.hash))location.hash='#chess';show();
  notify(`Подключено: ${link.self.name}`,'ok')}
 catch(e){link.detach();if(e&&e.name!=='NotFoundError')notify('Не удалось подключиться: '+(e.message||e),'bad')}
 finally{busy=false}}
$('cmpUsb').onclick=()=>connect('usb',false);$('cmpBle').onclick=()=>connect('ble',false);
// A port allowed earlier is opened again without asking.
if(hasSerial)navigator.serial.getPorts().then(ports=>{if(ports.length===1&&!link.connected){lastPort=ports[0];connect('usb',true)}}).catch(()=>{});
setInterval(()=>{if(auth)net.tick()},1000);
setInterval(()=>{if(link.connected)link.sync()},30000); // a missed "message waiting" push
setInterval(()=>{if(auth){peersNow();refreshChess(false)}},3000);
})();
