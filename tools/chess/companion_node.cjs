// The browser player of web/chess-companion.js, run in Node for tools/chess_companion_check.py: the
// companion is reached through tools/usb_tcp_bridge.py with the same frames as Web Serial.
// node tools/chess/companion_node.cjs PORT — JSON lines on stdin {"id":1,"do":"command","line":"chess ..."},
// "do": command | web | show | peers | self | advert | sync | raw; one JSON answer per line on stdout.
const path=require('path'),net=require('net'),readline=require('readline');
const {Companion,StreamFramer,player}=require(path.join(__dirname,'../../web/chess-companion.js'));
const link=new Companion(),store=new Map(),game=player(link,{get:k=>store.get(k),set:(k,v)=>store.set(k,v)}),others=[];
link.onOther=(from,name,text)=>others.push({from,name,text});
const socket=net.connect(+process.argv[2]||8772,'127.0.0.1');socket.setNoDelay(true);
const framer=new StreamFramer(f=>link.receive(f));
socket.on('data',d=>framer.push(new Uint8Array(d)));socket.on('close',()=>{link.detach();process.exit(2)});
const out=o=>process.stdout.write(JSON.stringify(o)+'\n');
socket.on('connect',async()=>{
 try{await link.attach({write:async d=>{socket.write(Buffer.from(StreamFramer.wrap(d)))},close:()=>socket.end()},'usb');game.load();game.net.linked();link.sync()}
 catch(e){out({ready:false,error:String(e.message||e)});process.exit(1)}
 out({ready:true,self:link.self,device:link.deviceInfo()});
 setInterval(()=>game.net.tick(),1000);setInterval(()=>link.sync(),15000);
 readline.createInterface({input:process.stdin}).on('line',async line=>{
  let q;try{q=JSON.parse(line)}catch{return}
  try{let r;
   if(q.do==='command')r=game.net.command(q.line);
   else if(q.do==='web')r=game.net.web();
   else if(q.do==='show'){const m=game.net.find(parseInt(q.game,16));r=m?game.net.detail(m):null}
   else if(q.do==='peers'){await link.loadContacts();r=link.peers()}
   else if(q.do==='self')r={self:link.self,others,connected:link.connected};
   else if(q.do==='advert')r=String((await link.advert())[0]);
   else if(q.do==='sync'){await link.sync();r='ok'}
   else if(q.do==='raw')r=link.sendText(q.peer,q.text); // typed by hand, as in the MeshCore app
   out({id:q.id,result:r})}catch(e){out({id:q.id,error:String(e.message||e)})}})});
