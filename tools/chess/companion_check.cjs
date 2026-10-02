// Host check of web/chess-companion.js: the browser rules against src/Chess.cpp (perft, notation,
// endings and random games replayed by both), two players exchanging the real command texts with lost
// ACKs and repeats, and the MeshCore companion protocol against a simulated companion.
// Run: node tools/chess/companion_check.cjs
const assert=require('assert'),{execFileSync}=require('child_process'),fs=require('fs'),os=require('os'),path=require('path');
const ROOT=path.resolve(__dirname,'../..'),C=require(path.join(ROOT,'web/chess-companion.js'));
const {Position,Game,ChessNet,Companion,StreamFramer,player,uci,Delivered,Failed,Sent}=C;
let checks=0;const ok=(cond,what)=>{assert(cond,what);checks++};

// ---- Rules ----
const perft=(p,d)=>{if(!d)return 1;const list=p.legal();if(d===1)return list.length;let n=0;for(const m of list){const q=p.clone();q.apply(m);n+=perft(q,d-1)}return n};
const fen=f=>{const p=new Position();ok(p.fromFen(f),'fen '+f);return p};
for(const [f,d,n] of [ // https://www.chessprogramming.org/Perft_Results
 ['rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',4,197281],
 ['r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1',3,97862],
 ['8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1',5,674624],
 ['r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1',4,422333],
 ['rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8',3,62379],
 ['r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10',3,89890]]){
 const p=fen(f);ok(perft(p,d)===n,`perft ${f}`);ok(p.fen()===f,'fen round trip '+f)}
for(const [f,m,s] of [['2k5/8/8/8/8/8/8/R3K2R w KQ - 0 1','e1g1','O-O'],['2k5/8/8/8/8/8/8/R3K2R w KQ - 0 1','e1c1','O-O-O'],['2k5/8/8/8/8/8/4K3/R6R w - - 0 1','a1d1','Rad1'],
 ['7k/8/8/8/8/5N2/8/1N2K3 w - - 0 1','b1d2','Nbd2'],['7k/8/8/1N6/8/8/8/1N2K3 w - - 0 1','b1c3','N1c3'],['8/7k/8/8/Q7/8/8/Q5QK w - - 0 1','a1d4','Qa1d4'],['8/7k/8/8/Q7/8/8/Q3Q2K w - - 0 1','a1d4','Q1d4'],
 ['7k/8/8/3pP3/8/8/8/4K3 w - d6 0 1','e5d6','exd6'],['7k/P7/8/8/8/8/8/4K3 w - - 0 1','a7a8q','a8=Q+'],['7k/P7/8/8/8/8/8/4K3 w - - 0 1','a7a8n','a8=N'],['6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1','a1a8','Ra8#']]){
 const p=fen(f),mv=p.parseUci(m);ok(mv&&p.san(mv)===s,`san ${m}: ${mv&&p.san(mv)}, expected ${s}`)}
{const p=fen('7k/P7/8/8/8/8/8/4K3 w - - 0 1');ok(uci(p.parseUci('a7a8'))==='a7a8q','default promotion');ok(!p.parseUci('a7a8k')&&!p.parseUci('e1e3')&&!p.parseUci('e1'),'bad moves')}
ok(!fen('4k3/8/8/8/8/8/5r2/R3K2R w KQ - 0 1').parseUci('e1g1'),'castle through check');ok(fen('4k3/8/8/8/8/8/5r2/R3K2R w KQ - 0 1').parseUci('e1c1'),'long castle');
const play=s=>{const g=new Game();for(const w of s.split(' '))ok(g.play(g.pos.parseUci(w)),'play '+w);return g};
ok(play('f2f3 e7e5 g2g4 d8h4').outcome===1,"fool's mate");
ok(play('g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8').outcome===4,'threefold repetition');
ok(play('g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1').outcome===0,'two repetitions');
ok(play('e2e3 a7a5 d1h5 a8a6 h5a5 h7h5 h2h4 a6h6 a5c7 f7f6 c7d7 e8f7 d7b7 d8d3 b7b8 d3h7 b8c8 f7g6 c8e6').outcome===2,'stalemate');
ok(fen('8/8/2b5/4k3/8/8/8/4K2B w - - 0 1').insufficientMaterial()&&!fen('8/8/3b4/4k3/8/8/8/4K2B w - - 0 1').insufficientMaterial()&&!fen('8/8/3n4/4k3/8/8/8/4K2N w - - 0 1').insufficientMaterial(),'material');
{const g=new Game();ok(!g.load([12|28<<6,12|28<<6])&&g.plies===0,'illegal replay')}

// Random games, replayed by src/Chess.cpp: notation, position, number of legal moves and outcome per ply.
let seed=7;const rnd=n=>{seed=(seed*1103515245+12345)&0x7fffffff;return seed%n};
const games=[];
for(let i=0;i<300;i++){const g=new Game(),lines=[];
 while(!g.outcome&&g.plies<(i%10===0?512:220)){const list=g.pos.legal(),caps=list.filter(m=>g.pos.board[m>>6&63]||m>>12),m=caps.length&&rnd(3)?caps[rnd(caps.length)]:list[rnd(list.length)],san=g.pos.san(m);g.play(m);lines.push(`${san} ${g.pos.fen()} ${g.pos.legal().length} ${g.outcome}`)}
 games.push({moves:g.moves.map(uci).join(' '),lines})}
const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'mmchess-'));
try{
 execFileSync('c++',['-std=gnu++17','-O2','-I',path.join(ROOT,'include'),path.join(ROOT,'src/Chess.cpp'),path.join(ROOT,'tools/chess/replay.cpp'),'-o',path.join(tmp,'replay')]);
 const out=execFileSync(path.join(tmp,'replay'),{input:games.map(g=>g.moves).join('\n')+'\n',maxBuffer:1<<28}).toString().split('end\n');
 let plies=0,ends=new Set();
 games.forEach((g,i)=>{const c=out[i].trim().split('\n');assert.deepStrictEqual(c,g.lines,`game ${i} differs from src/Chess.cpp`);plies+=c.length;ends.add(g.lines.at(-1).split(' ').at(-1))});
 ok(ends.size>=4,'random games reach several endings: '+[...ends]);console.log(`PASS rules: perft, notation, endings, ${games.length} random games (${plies} plies) as src/Chess.cpp`)}
finally{fs.rmSync(tmp,{recursive:true,force:true})}

// ---- Two players over a simulated radio: the real command texts ----
let clock=1e6;const air=[];
function side(me,other,name){
 let next=1;const store={};
 const net=new ChessNet({send:(peer,text)=>{const id=next++;air.push({from:me,to:peer,text,id,net:()=>net});return id},contact:id=>id===other?{name,type:1,heardAt:0}:null,
  save:j=>store.json=j,now:()=>clock,unix:()=>1.8e9,random:n=>n===2?0:(4242+next*977)%n});
 net.store=store;return net}
const A=side('AAAAAAAAAAAA','BBBBBBBBBBBB','Bob'),B=side('BBBBBBBBBBBB','AAAAAAAAAAAA','Ann'),nets={AAAAAAAAAAAA:A,BBBBBBBBBBBB:B};
// Deliver everything on air; drop: texts whose ACK is lost (delivered, but the sender hears nothing).
function deliver(lostAck=[]){while(air.length){const m=air.shift(),to=nets[m.to];to.receive(m.from,m.from==='AAAAAAAAAAAA'?'Ann':'Bob',m.text);m.net().delivery(m.id,lostAck.some(t=>m.text.includes(t))?Failed:Delivered)}}
const g=n=>n.json()[0];
ok(A.command('chess invite BBBBBBBBBBBB w').startsWith('OK game '),'invite');
const id=g(A).id;ok(air[0].text.startsWith(`♟${id} new w`),'invite text '+air[0].text);deliver();
ok(g(B).state==='invited'&&g(B).color==='black','challenge arrives');ok(B.command('chess accept '+id)==='OK accept','accept');deliver();
ok(g(A).state==='playing'&&g(A).my_turn,'game on');
const moves='e2e4 e7e5 f1c4 b8c6 d1h5 g8f6 h5f7'.split(' ');
moves.forEach((m,i)=>{const [me,them]=i%2?[B,A]:[A,B];ok(me.command(`chess move ${id} ${m}`)==='OK move','move '+m);
 if(i===1)ok(air[0].text===`♟${id} 2 e7e5 1... e5`,'black move text '+air[0].text);if(i===2)ok(air[0].text===`♟${id} 3 f1c4 2. Bc4`,'white move text '+air[0].text);
 if(i===3){const t=air[0].text;deliver(['b8c6']);ok(g(B).out_status===Failed&&g(B).move_open,'lost ACK: not delivered');them.receive('BBBBBBBBBBBB','Bob',t);ok(g(A).plies===4,'repeat ignored')}
 else deliver();
 ok(g(A).fen===g(B).fen,'same position after '+m)});
ok(g(B).out_status===Delivered&&!g(B).move_open,'their next move confirms the move whose ACK was lost');
ok(g(A).state==='over'&&g(A).result==='white'&&g(A).reason==='mate'&&g(B).result==='white','mate on both sides');
// Out of turn, illegal and unknown games change nothing.
const before=g(B).fen;B.receive('AAAAAAAAAAAA','Ann',`♟${id} 9 a2a4`);B.receive('AAAAAAAAAAAA','Ann','♟0001 1 e2e4');ok(g(B).fen===before,'ignored commands');
ok(!B.receive('AAAAAAAAAAAA','Ann','hello'),'plain text is not chess');
// Rematch with a draw offer, automatic resending and saving.
ok(B.command(`chess remove ${id}`)==='OK remove'&&B.json().length===0,'remove a finished game');
ok(B.command('chess invite AAAAAAAAAAAA w').startsWith('OK game'),'rematch');const id2=g(B).id;deliver();A.command('chess accept '+id2);deliver();
const two=A.json().find(x=>x.id===id2);ok(two.state==='playing'&&!two.my_turn&&g(B).my_turn,'white moves first');
B.command(`chess move ${id2} d2d4`);air.length=0;B.delivery(B.matches[0].moveId,Failed);ok(g(B).out_status===Failed,'move not delivered');
B.tick();ok(g(B).retry_in===120,'retry in 2 min');clock+=121000;B.tick();ok(air.length===1&&air[0].text.includes(' 1 d2d4 1. d4'),'resent automatically');deliver();
ok(A.json().find(x=>x.id===id2).plies===1,'resent move arrives');
A.command(`chess draw ${id2}`);deliver();ok(g(B).draw_offer==='theirs','draw offered');B.command(`chess draw ${id2}`);deliver();
ok(A.json().find(x=>x.id===id2).reason==='agreed'&&g(B).result==='draw','draw agreed');
const C2=side('BBBBBBBBBBBB','AAAAAAAAAAAA','Ann');C2.load(B.store.json);
ok(JSON.stringify(C2.json().map(x=>[x.id,x.fen,x.state,x.result])).length>10&&C2.json()[0].fen===g(B).fen&&C2.json()[0].reason==='agreed','saved and replayed');
ok(C2.load('{"v":1,"games":[{"peer":"AAAAAAAAAAAA","id":5,"state":"playing","moves":["e2e4","e2e4"]}]}')===1&&!C2.json().length,'damaged game dropped');
console.log('PASS games: challenge, moves, lost ACK, repeat, mate, rematch, resending, draw, saving');

// ---- The companion protocol, against a simulated MeshCore companion ----
function fakeCompanion(){
 const key=Buffer.alloc(32,0x11),peerKey=Buffer.concat([Buffer.from('BBBBBBBBBBBB','hex'),Buffer.alloc(26,0x22)]),sent=[],queue=[];let ackNo=100,link,ackDrop=0,parse=new Uint8Array(0);
 const frame=d=>{const b=Buffer.from(d),out=Buffer.concat([Buffer.from([0x3E,b.length&255,b.length>>8]),b]);
  // in random pieces, after some boot noise, as a serial port gives them
  let at=0;while(at<out.length){const n=1+Math.floor(Math.random()*7);const piece=out.subarray(at,at+n);at+=n;setImmediate(()=>framer.push(new Uint8Array(piece)))}};
 const le=n=>[n&255,n>>8&255,n>>16&255,n>>>24&255];
 const contact=(k,type,name)=>{const f=Buffer.alloc(148);f[0]=3;k.copy(f,1);f[33]=type;f[35]=1;f.write(name,100);f.writeUInt32LE(1.8e9-30,144);return f};
 const handle=c=>{
  const code=c[0];
  if(code===22){const f=Buffer.alloc(100);f[0]=13;f[1]=9;f.write('Heltec V4',20);f.write('v1.17.1',60);frame(f)}
  else if(code===1){const f=Buffer.alloc(58+6);f[0]=5;key.copy(f,4);f.writeUInt32LE(868731,48);f.writeUInt32LE(62500,52);f[56]=8;f[57]=6;f.write('Stock',58);frame(f.subarray(0,63))}
  else if(code===6)frame([0]);
  else if(code===4){frame([2,...le(2)]);frame(contact(peerKey,1,'Bob'));frame(contact(Buffer.alloc(32,0x33),2,'Repeater'));frame([4,...le(0)])}
  else if(code===5)frame([9,...le(1.8e9)]);
  else if(code===13)frame([0]);
  else if(code===10)frame(queue.length?queue.shift():[10]);
  else if(code===2){const attempt=c[2],to=Buffer.from(c.subarray(7,13)).toString('hex').toUpperCase(),text=Buffer.from(c.subarray(13)).toString();
   const ack=ackNo++;sent.push({attempt,to,text,ack,flood:sent.resetBefore===true});sent.resetBefore=false;frame([6,0,...le(ack),...le(100)]);
   if(ackDrop>0)ackDrop--;else setTimeout(()=>frame([0x82,...le(ack),...le(900)]),5)}
  if(code===13)sent.resetBefore=true};
 const framer=new StreamFramer(f=>link.receive(f));
 return {key,sent,
  transport(l){link=l;setImmediate(()=>framer.push(new Uint8Array(Buffer.from('ESP-ROM boot\r\n>x'))));
   return {write:async raw=>{const d=StreamFramer.wrap(raw),b=new Uint8Array(parse.length+d.length);b.set(parse);b.set(d,parse.length);let i=0;
    while(b.length-i>=3){ok(b[i]===0x3C,'frame to companion starts with <');const n=b[i+1]|b[i+2]<<8;if(b.length-i<3+n)break;handle(b.subarray(i+3,i+3+n));i+=3+n}parse=b.slice(i)},close(){}}},
  incoming(text,v3=true){const t=Buffer.from(text),f=v3?[16,40,0,0,...Buffer.from('BBBBBBBBBBBB','hex'),0xFF,0,...le(1.8e9),...t]:[7,...Buffer.from('BBBBBBBBBBBB','hex'),0xFF,0,...le(1.8e9),...t];queue.push(f);frame([0x83])},
  dropAcks(n){ackDrop=n}}}
const wait=ms=>new Promise(r=>setTimeout(r,ms));
async function until(cond,what,ms=3000){const end=Date.now()+ms;while(!cond()){if(Date.now()>end)throw Error('timeout: '+what);await wait(5)}ok(true,what)}
(async()=>{
 const fake=fakeCompanion(),link=new Companion(),store=new Map(),game=player(link,{get:k=>store.get(k),set:(k,v)=>store.set(k,v)}),net=game.net,others=[];
 link.waitFor=()=>40;link.onOther=(from,name,text)=>others.push(text);
 await link.attach(fake.transport(link),'usb');game.load();link.sync();
 ok(link.self.name==='Stock'&&link.self.freq===868.731&&link.self.bw===62.5&&link.self.sf===8&&link.self.cr===6,'self info');
 ok(link.deviceInfo().version==='v1.17.1'&&link.deviceInfo().model==='Heltec V4','device info');
 const peers=link.peers();ok(peers.length===2&&peers[0].id==='BBBBBBBBBBBB'&&peers[0].name==='Bob'&&peers[0].type===1&&peers[0].heard&&peers[0].age_seconds<120&&peers[0].path_length===1,'contacts '+JSON.stringify(peers[0]));
 fake.incoming('♟1A2B new w · шахматы MeshMesh: вы играете чёрными');
 await until(()=>net.json().length===1&&net.json()[0].state==='invited','a challenge from the radio');
 fake.incoming('Привет',false);await until(()=>others[0]==='Привет','other messages are kept for the page (old frame format)');
 ok(net.command('chess accept 1A2B')==='OK accept','accept through the page command');
 await until(()=>fake.sent.length===1&&net.json()[0].out_status===Delivered,'ACK from the companion delivers');
 ok(fake.sent[0].text==='♟1A2B yes'&&fake.sent[0].to==='BBBBBBBBBBBB'&&fake.sent[0].attempt===0,'text and recipient '+JSON.stringify(fake.sent[0]));
 fake.incoming('♟1A2B 1 e2e4 1. e4');await until(()=>net.json()[0].plies===1&&net.json()[0].my_turn,'their move');
 fake.dropAcks(2);ok(net.command('chess move 1A2B e7e5')==='OK move','my move');
 await until(()=>net.json()[0].out_status===Delivered,'third attempt delivers');
 ok(fake.sent.slice(1).map(s=>s.attempt).join()==='0,1,2'&&fake.sent[3].flood&&!fake.sent[2].flood,'attempts 0,1,2; the path is reset before the third');
 ok(new Set(fake.sent.slice(1).map(s=>s.text)).size===1&&fake.sent[1].text==='♟1A2B 2 e7e5 1... e5','the same text every attempt');
 fake.incoming('♟1A2B 3 d1h5 2. Qh5');await until(()=>net.json()[0].plies===3,'next move');
 fake.dropAcks(3);net.command('chess move 1A2B b8c6');
 await until(()=>net.json()[0].out_status===Failed,'no ACK after three attempts: not delivered');
 ok(net.json()[0].move_open,'the move stays open for resending');
 ok(JSON.parse(store.get('mm-chess-111111111111')).games[0].moves.length===4,'saved under the companion key');
 link.detach();ok(!link.connected&&link.sendText('BBBBBBBBBBBB','x')===0,'no link: nothing is sent');
 console.log(`PASS companion protocol: framing with noise, self and device info, contacts, messages, ACK, three attempts with flood, failure`);
 console.log(`OK ${checks} checks`);process.exit(0)
})().catch(e=>{console.error('FAIL',e);process.exit(1)});
