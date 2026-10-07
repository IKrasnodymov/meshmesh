// The language of the site's chess page (tools/chess_site.py). The page is web/index.html, which is in
// Russian like the boards' own page; here its text is translated as it is drawn, so the boards' page
// stays as it is. Dictionaries: i18n/chess/<lang>.json, the Russian text as the key (i18n/README.md).
// The language is the site's (?lang=, then "mm-lang" in this browser, then the browser's languages).
(function(){
'use strict';
const LANGS=[['en','English'],['ru','Русский'],['uk','Українська'],['es','Español'],['pt','Português'],['fr','Français'],['de','Deutsch'],['it','Italiano'],['pl','Polski'],['tr','Türkçe'],['zh','中文'],['ja','日本語'],['ko','한국어'],['ar','العربية'],['id','Bahasa Indonesia']];
const CODES=LANGS.map(l=>l[0]);
// Piece letters of the notation, K Q R B N: the Russian page writes Кр Ф Л С К.
const RU_PIECES=['Кр','Ф','Л','С','К'];
const RU_SAN=/^(Кр|[ФЛСК])?([a-h]?[1-8]?x?[a-h][1-8])(?:=(Кр|[ФЛСК]))?([+#]?)$/;
const CYR=/[А-Яа-яЁё]/;

// Compiled per language: exact keys, then patterns with {0}.. (kept as is), {san} (a move) and {r}
// (translated again), the one with more fixed text first.
function compile(dict){
 const exact=new Map(),patterns=[];
 for(const [key,value] of Object.entries(dict)){
  if(key.startsWith('_'))continue;
  if(!/\{(\d|r|san)\}/.test(key)){exact.set(key,value);continue}
  const slots=[];
  const re=key.split(/(\{(?:\d|r|san)\})/).map(part=>{const m=/^\{(\d|r|san)\}$/.exec(part);if(!m)return part.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');slots.push(m[1]);return m[1]==='r'?'(.+)':'(.+?)'}).join('');
  patterns.push({re:new RegExp('^'+re+'$'),slots,value,fixed:key.replace(/\{(?:\d|r|san)\}/g,'').length})}
 patterns.sort((a,b)=>b.fixed-a.fixed); // "не доставлено, повтор через {0} мин" before "{0} мин"
 return {exact,patterns}}

function create(dicts,code){
 let lang='ru',table=null,plurals=null,pieces=null;
 function use(next){
  lang=CODES.includes(next)?next:'en';
  table=lang==='ru'?null:compile(dicts[lang]||dicts.en||{});
  plurals=lang==='ru'?null:new Intl.PluralRules(lang);
  pieces=lang==='ru'?RU_PIECES:String(dicts[lang]&&dicts[lang]._pieces||'K Q R B N').split(' ')}
 // A value is a string or plural forms {one, few, many, other, ...} chosen by the number in {0}.
 function fill(value,args){
  let text=value;
  if(typeof value==='object'){const n=Number(args[0]);text=value[Number.isFinite(n)?plurals.select(n):'other']??value.other}
  return String(text).replace(/\{(\d|r|san)\}/g,(m,k)=>{const i=k==='r'?args.r:k==='san'?args.san:args[+k];return i===undefined?m:i})}
 function san(s){
  const m=RU_SAN.exec(s);if(!m||!CYR.test(s))return null;
  const letter=c=>c?pieces[RU_PIECES.indexOf(c)]:'';
  return letter(m[1])+m[2]+(m[3]?'='+letter(m[3]):'')+m[4]}
 // The translation of a whole text, or null when there is none.
 function core(s){
  if(!CYR.test(s))return null;
  const {exact,patterns}=table;
  if(exact.has(s))return fill(exact.get(s),[]);
  const cap=s[0].toUpperCase()+s.slice(1);if(cap!==s&&exact.has(cap))return fill(exact.get(cap),[]);
  const move=san(s);if(move!==null)return move;
  // Joined pieces, as the page writes them: "a · b", "a — b", "Name: text"; a name never spans a " · ".
  const joined=sep=>{if(!s.includes(sep))return null;const parts=s.split(sep),out=parts.map(x=>core(x));return out.some(x=>x!==null)?out.map((x,i)=>x??parts[i]).join(sep):null};
  const dots=joined(' · ');if(dots!==null)return dots;
  for(const p of patterns){
   const m=p.re.exec(s);if(!m)continue;
   const args=[];
   p.slots.forEach((slot,i)=>{const v=m[i+1];if(slot==='r')args.r=core(v)??v;else if(slot==='san')args.san=san(v)??v;else args[+slot]=v});
   return fill(p.value,args)}
  const dash=joined(' — ');if(dash!==null)return dash;
  const named=/^([^:]{1,40}): (.+)$/.exec(s);
  if(named){const rest=core(named[2]);if(rest!==null)return named[1]+': '+rest}
  return null}
 function t(s){
  if(!table||typeof s!=='string')return s;
  const m=/^(\s*)([\s\S]*?)(\s*)$/.exec(s),out=m[2]?core(m[2]):null;
  return out===null?s:m[1]+out+m[3]}
 use(code);
 return {t,use,get lang(){return lang},san}}

function pick(){
 try{const q=new URLSearchParams(location.search).get('lang');if(CODES.includes(q))return q}catch{}
 try{const s=localStorage.getItem('mm-lang');if(CODES.includes(s))return s}catch{}
 for(const l of (typeof navigator!=='undefined'&&(navigator.languages||[navigator.language]))||[]){const c=String(l||'').toLowerCase().split('-')[0];if(CODES.includes(c))return c;if(c==='be')return 'ru'}
 return 'en'}

const api={LANGS,CODES,create,compile};
if(typeof module!=='undefined'&&module.exports){module.exports=api;return}
const dicts=globalThis.MeshMeshChessDicts||{},tr=create(dicts,pick());
globalThis.MeshMeshChessI18n={...api,t:s=>tr.t(s),get lang(){return tr.lang},set:setLang,select};
if(typeof document==='undefined')return;

// The page as drawn: text and the attributes people read. The Russian original of every node is kept,
// so another language is applied to the original, and Russian gives it back.
const ATTRS=['title','aria-label','placeholder'],textOrig=new WeakMap(),attrOrig=new WeakMap();
const skip=el=>!!el&&!!el.closest&&!!el.closest('script,style,textarea,[data-notr]');
function text(node){
 if(skip(node.parentElement))return;
 const orig=textOrig.has(node)&&node.data===textOrig.get(node).out?textOrig.get(node).ru:node.data;
 const out=tr.t(orig);if(out!==node.data)node.data=out;textOrig.set(node,{ru:orig,out})}
function attrs(el){
 if(skip(el))return;let saved=attrOrig.get(el);
 for(const a of ATTRS){
  if(!el.hasAttribute(a))continue;const cur=el.getAttribute(a),was=saved&&saved[a];
  const orig=was&&cur===was.out?was.ru:cur,out=tr.t(orig);
  if(out!==cur)el.setAttribute(a,out);if(!saved)attrOrig.set(el,saved={});saved[a]={ru:orig,out}}}
function walk(root){
 if(root.nodeType===3){text(root);return}
 if(root.nodeType!==1)return;
 if(skip(root))return;
 attrs(root);
 const w=document.createTreeWalker(root,NodeFilter.SHOW_TEXT|NodeFilter.SHOW_ELEMENT);
 for(let n=w.nextNode();n;n=w.nextNode()){if(n.nodeType===3)text(n);else attrs(n)}}
let busy=false;
const observer=new MutationObserver(list=>{
 if(busy)return;busy=true;
 try{for(const m of list){
  if(m.type==='characterData')text(m.target);
  else if(m.type==='attributes')attrs(m.target);
  else for(const n of m.addedNodes)walk(n)}}
 finally{busy=false;observer.takeRecords()}});
function apply(){
 document.documentElement.lang=tr.lang;
 busy=true;try{walk(document.body)}finally{busy=false;observer.takeRecords()}}
function setLang(code,remember){
 tr.use(code);if(remember)try{localStorage.setItem('mm-lang',tr.lang)}catch{}
 apply();for(const s of document.querySelectorAll('select[data-chesslang]'))s.value=tr.lang}
// The language picker: the page's own text stays out of the translation.
function select(){
 return `<select data-chesslang data-notr aria-label="Language" style="background:var(--card);color:var(--ink);border:1px solid var(--line);border-radius:8px;padding:6px 8px;font:inherit;width:auto;min-width:0">${LANGS.map(([c,n])=>`<option value="${c}"${c===tr.lang?' selected':''}>${n}</option>`).join('')}</select>`}
document.addEventListener('change',e=>{if(e.target.matches&&e.target.matches('select[data-chesslang]'))setLang(e.target.value,true)});
// Background notifications of a move are made by the page with Russian text.
if(typeof Notification!=='undefined'){
 const Native=Notification;
 globalThis.Notification=class extends Native{constructor(title,options){super(tr.t(title),options&&options.body?{...options,body:tr.t(options.body)}:options)}};}
apply();
observer.observe(document.body,{subtree:true,childList:true,characterData:true,attributes:true,attributeFilter:ATTRS});
})();
