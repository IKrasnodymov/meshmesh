// Check of the chess page's languages (web/chess-i18n.js, i18n/chess/<lang>.json): the texts the real
// functions of web/index.html give for every state of a game, the news of a move and the companion's
// texts must come out translated in every language, with the move notation of that language.
// Run: node tools/chess/i18n_check.cjs [lang ...]
const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const ROOT=path.resolve(__dirname,'../..'),I=require(path.join(ROOT,'web/chess-i18n.js'));
const langs=process.argv.slice(2).length?process.argv.slice(2):I.CODES.filter(c=>c!=='ru');
const dicts={};for(const l of langs)dicts[l]=JSON.parse(fs.readFileSync(path.join(ROOT,`i18n/chess/${l}.json`),'utf8'));

// The page's own functions, as tools/check_web_chat.cjs runs them.
const html=fs.readFileSync(path.join(ROOT,'web/index.html'),'utf8'),source=html.split('<script>')[1].split('</script>')[0];
function element(){return{value:'',dataset:{},classList:{toggle(){}},getContext(){return{}}}}
const ids=new Map(),page={document:{documentElement:{outerHTML:html},getElementById(id){if(!ids.has(id))ids.set(id,element());return ids.get(id)},querySelectorAll(){return[]},addEventListener(){},createElement:element},TextDecoder,TextEncoder,Uint8Array,Uint8ClampedArray,DataView,Map,Set,JSON,Math,Number,Date,setTimeout,clearTimeout,URL:{},Blob,fetch(){throw Error('No network expected')}};
vm.createContext(page);vm.runInContext(source,page);
const fn=name=>vm.runInContext(name,page);
const chessState=fn('chessState'),deliveryText=fn('deliveryText'),chessNews=fn('chessNews'),pathText=fn('pathText'),ago=fn('ago'),ruSan=fn('ruSan');

const texts=new Set(),add=s=>{if(s)texts.add(s)};
const game=(o)=>({id:'1A2B',peer:'AABBCCDDEEFF',name:'Bob',color:'white',plies:4,state:'playing',my_turn:true,check:false,result:'',reason:'',draw_offer:'',out_status:3,retry_in:-1,retries:0,auto_stopped:false,last_san:'Nf3',...o});
const states=[];
for(const color of ['white','black']){
 states.push(game({color,state:'inviting'}),game({color,state:'invited'}),game({color,draw_offer:'theirs'}),game({color,check:true}),game({color,my_turn:false}),
  game({color,state:'over',reason:'declined'}),game({color,state:'over',reason:'cancelled'}));
 for(const [result,reason] of [['white','mate'],['black','mate'],['white','resigned'],['black','resigned'],['draw','stalemate'],['draw','repetition'],['draw','fifty'],['draw','material'],['draw','too_long'],['draw','agreed']])
  states.push(game({color,state:'over',result,reason}))}
for(const g of states){add(chessState(g));add(chessState(g,true))}
for(const out_status of [1,2,3])add(deliveryText(game({out_status})));
for(const retry_in of [-1,0,30,61,600])add(deliveryText(game({out_status:4,retry_in})));
add(deliveryText(game({out_status:4,auto_stopped:true})));
// News of the list: a move (with every ending), a challenge, an answer, a draw offer, a lost ACK; several at once.
const before=states.map((g,i)=>game({...g,id:'G'+i,state:g.state==='over'?'playing':g.state,plies:3,draw_offer:'',out_status:3}));
const after=states.map((g,i)=>({...g,id:'G'+i,plies:4,last_san:['Nf3','exd6','O-O','e8=Q+','Qh5#','Kxe2','Bb5+','Rad1'][i%8]}));
for(let i=0;i<states.length;i++)add(chessNews([before[i]],[after[i]]).text);
add(chessNews([],[game({state:'invited',name:'Анна'})]).text);
add(chessNews([game({state:'inviting'})],[game({state:'playing',plies:0})]).text);
add(chessNews([game({state:'inviting',my_turn:false})],[game({state:'playing',plies:0,my_turn:false})]).text);
add(chessNews([game({})],[game({draw_offer:'theirs'})]).text);
add(chessNews([game({out_status:2})],[game({out_status:4})]).text);
add(chessNews([game({}),game({id:'2'})],[game({plies:5,last_san:'Bc4'}),game({id:'2',draw_offer:'theirs'})]).text);
for(const n of [0,1,2,3,5,11,21,22,25,255])add(pathText({path_length:n}));
add(ago(30));add('соперник слышен сейчас');for(const s of [90,4000,90000]){add(ago(s));add(ago(s)+' назад');add('соперник слышен '+ago(s)+' назад')} // under a minute: "сейчас"
// Texts of the page and of the companion that are written out as they are.
for(const k of Object.keys(JSON.parse(fs.readFileSync(path.join(ROOT,'i18n/chess/en.json'),'utf8'))))if(!/[{_]/.test(k[0])&&!k.includes('{'))add(k);
add('Шахматы · Bob');add('USB · 868.731 МГц · SF8 · BW 62.5 · CR 4/6 · v1.12.0');add('Не выполнено: Шахматы: выберите чат-контакт');add('Не удалось подключиться: Companion не ответил');
add('Ваш ход · '+ruSan('Nf3'));add('Победа: мат · '+ruSan('Qxf7#'));

const CYR=/[А-Яа-яЁё]/;let checks=0;
for(const lang of langs){
 const t=I.create(dicts,lang).t,pieces=dicts[lang]._pieces.split(' ');
 // Every translated piece marked ⟦…⟧: what is left outside the marks was not translated (Ukrainian
 // shares words with Russian, so the text alone does not tell).
 const mark=v=>typeof v==='object'?Object.fromEntries(Object.entries(v).map(([k,x])=>[k,'⟦'+x+'⟧'])):'⟦'+v+'⟧';
 const marked=I.create({[lang]:Object.fromEntries(Object.entries(dicts[lang]).map(([k,v])=>[k,k==='_pieces'?v:mark(v)]))},lang).t;
 for(const s of texts){
  const out=t(s),left=marked(s).replace(/⟦[^⟧]*⟧/g,'').replace(/Анна/g,'').replace(/(Кр|[ФЛСКТ])?[a-h]?[1-8]?x?[a-h][1-8](=(Кр|[ФЛСКТ]))?/g,''); // a move: checked below
  assert(!CYR.test(left),`${lang}: not translated: ${s} -> ${marked(s)}`);
  assert(!/\{(\d|r|san)\}/.test(out),`${lang}: placeholder left: ${s} -> ${out}`);checks++}
 // Notation: the pieces of the language, castling and pawns as they are.
 const moves=[['Кf3',pieces[4]+'f3'],['Крxe2',pieces[0]+'xe2'],['e8=Ф+','e8='+pieces[1]+'+'],['Лad1',pieces[2]+'ad1'],['Сb5+',pieces[3]+'b5+'],['O-O','O-O'],['exd6','exd6']];
 for(const [ru,want] of moves){assert.strictEqual(t(ru),want,`${lang}: ${ru}`);checks++}
 assert(t('Bob: '+ruSan('Nf3')+' — ваш ход').startsWith('Bob: '+pieces[4]+'f3 — '),`${lang}: a move in the news`);checks++
 // Names and other people's text that only look like Russian stay as they are.
 assert.strictEqual(t('Анна'),'Анна');assert.strictEqual(t('Привет, как дела?'),'Привет, как дела?');checks+=2}
console.log(`PASS chess page languages: ${langs.join(' ')}, ${texts.size} texts each, ${checks} checks`);
