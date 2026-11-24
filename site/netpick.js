// The network of a city for a board (site/index.html, the folded box #netPick, shown on the Russian site only): a profile of the networks catalogue
// (networks.json, a copy of VoxMesh/meshcore-local-networks; "Update" fetches the current one) written to a board over
// the MeshCore companion protocol, by USB (Web Serial) or Bluetooth (Web Bluetooth): MeshMesh 0.9.0+ and the stock
// MeshCore companion alike. Installing firmware never changes the radio: the profile goes only to the board the visitor
// connects here and confirms. The choice stays in this browser (reloads, the next board); the settings a board had
// before are kept per board key for "Return the previous ones". Only frequency, bandwidth, SF and CR change.
const NET_URL='https://raw.githubusercontent.com/VoxMesh/meshcore-local-networks/main/catalog.json';
const el=id=>document.getElementById(id),T=k=>L(k),fill=(k,v)=>Object.entries(v).reduce((s,[a,b])=>s.split('{'+a+'}').join(b),T(k));
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const store=(k,v)=>{try{if(v===undefined)return JSON.parse(localStorage.getItem(k));if(v===null)localStorage.removeItem(k);else localStorage.setItem(k,JSON.stringify(v))}catch{return null}};
const valid=c=>c&&c.schema_version===2&&Array.isArray(c.locations)?c:null;
const radioOf=n=>({frequency:n.radio.frequency_hz/1e6,bandwidth:n.radio.bandwidth_hz/1000,sf:n.radio.sf,cr:n.radio.cr});
const same=(a,b)=>Math.abs(a.frequency-b.frequency)<.0005&&a.bandwidth===b.bandwidth&&a.sf===b.sf&&a.cr===b.cr;
const text=r=>`${Number(r.frequency).toFixed(3)} ${T('net.mhz')} · BW ${r.bandwidth} · SF${r.sf} · CR 4/${r.cr}`;
const bandOf=f=>f<500?'433':f<900?'868':'915';

let cat=null,q='',shown=5,pos=null,posNote='',link=null,ack=false,busy=false,result=null;
// {id, place, name, radio}: the chosen network, as it was when chosen.
let choice=store('mm-net');

async function load(){
 let bundled=null;try{const r=await fetch('networks.json',{cache:'no-cache'});if(r.ok)bundled=valid(await r.json())}catch{}
 cat=[valid(store('mm-networks')),bundled].filter(Boolean).sort((a,b)=>String(b.updated).localeCompare(String(a.updated)))[0]||null;render()}
async function update(){
 try{const r=await fetch(NET_URL,{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const c=valid(await r.json());if(!c)throw Error('format');store('mm-networks',c);cat=c;say(fill('net.updated',{date:c.updated}),'ok')}
 catch(e){say(T('net.updateFailed')+' '+e.message,'warn')}render()}
function all(){const out=[];for(const l of cat?.locations||[])for(const n of l.networks||[])out.push({l,n});return out}
const find=id=>all().find(x=>x.n.id===id);
function distance(l){if(!pos||typeof l.lat!=='number')return null;const r=Math.PI/180,a=Math.sin((l.lat-pos.lat)*r/2)**2+Math.cos(pos.lat*r)*Math.cos(l.lat*r)*Math.sin((l.lon-pos.lon)*r/2)**2;return 12742*Math.asin(Math.sqrt(a))}
// Places matching the search (nearest first once the position is known), with the networks of the larger territory
// around them while searching or located; one row per network.
function rows(){const locs=cat?.locations||[],s=q.trim().toLowerCase(),by=new Map(locs.map(l=>[l.id,l])),wide=!!(s||pos);
 const list=locs.filter(l=>!s||`${l.name} ${l.region||''}`.toLowerCase().includes(s)).map(l=>({l,d:distance(l),nets:[...(l.networks||[]).map(n=>({n,l})),...(wide?by.get(l.parent_id)?.networks||[]:[]).map(n=>({n,l:by.get(l.parent_id)}))]}));
 if(pos)list.sort((a,b)=>(a.d??1e9)-(b.d??1e9));return list.flatMap(x=>x.nets.map(y=>({...y,place:x.l,d:x.d})))}
function locate(){if(!navigator.geolocation){posNote=T('net.noPosition');render();return}posNote=T('net.locating');render();
 navigator.geolocation.getCurrentPosition(p=>{pos={lat:p.coords.latitude,lon:p.coords.longitude};posNote='';shown=5;render()},()=>{posNote=T('net.noPosition');render()},{maximumAge:600000,timeout:15000})}
function say(t,cls=''){const s=el('netStatus');s.hidden=!t;s.className='note '+cls;s.textContent=t}

// The companion link: frames "<" + LE16 length + data to the board, ">" + length + data back (USB), or the Nordic UART
// service (Bluetooth); one command at a time, pushes (0x80 and up) ignored.
const NUS='6e400001-b5a3-f393-e0a9-e50e24dcca9e',NUS_RX='6e400002-b5a3-f393-e0a9-e50e24dcca9e',NUS_TX='6e400003-b5a3-f393-e0a9-e50e24dcca9e';
const u32=(b,i)=>(b[i]|b[i+1]<<8|b[i+2]<<16|b[i+3]<<24)>>>0,le32=n=>[n&255,n>>>8&255,n>>>16&255,n>>>24&255];
const cstr=b=>{const z=b.indexOf(0);return new TextDecoder().decode(z<0?b:b.subarray(0,z))};
class Link{
 constructor(){this.pending=null;this.buf=new Uint8Array(0);this.close=()=>{}}
 frame(f){if(!f.length||f[0]>=0x80)return;const p=this.pending;if(p){this.pending=null;p(f)}}
 stream(chunk){const b=new Uint8Array(this.buf.length+chunk.length);b.set(this.buf);b.set(chunk,this.buf.length);let i=0;
  while(i<b.length){if(b[i]!==0x3E){i++;continue}if(b.length-i<3)break;const n=b[i+1]|b[i+2]<<8;if(!n||n>176){i++;continue}if(b.length-i<3+n)break;this.frame(b.slice(i+3,i+3+n));i+=3+n}
  this.buf=b.slice(i)}
 async usb(){const port=await navigator.serial.requestPort();await port.open({baudRate:115200});
  try{await port.setSignals({dataTerminalReady:true,requestToSend:false})}catch{} // no reset, as tools/device.py
  const reader=port.readable.getReader(),writer=port.writable.getWriter();
  (async()=>{try{for(;;){const {value,done}=await reader.read();if(done)break;if(value)this.stream(value)}}catch{}})();
  // A CR first: a UART that woke the board loses the bytes that woke it.
  this.write=async d=>{const f=new Uint8Array(d.length+3);f[0]=0x3C;f[1]=d.length&255;f[2]=d.length>>8;f.set(d,3);await writer.write(f)};await writer.write(new Uint8Array([13]));
  this.close=async()=>{try{await reader.cancel()}catch{}try{reader.releaseLock();writer.releaseLock()}catch{}try{await port.close()}catch{}}}
 async ble(){const device=await navigator.bluetooth.requestDevice({filters:[{services:[NUS]},{namePrefix:'MeshCore'}],optionalServices:[NUS]});
  const server=await device.gatt.connect(),service=await server.getPrimaryService(NUS),rx=await service.getCharacteristic(NUS_RX),tx=await service.getCharacteristic(NUS_TX);
  tx.addEventListener('characteristicvaluechanged',e=>{const v=e.target.value;this.frame(new Uint8Array(v.buffer,v.byteOffset,v.byteLength).slice())});await tx.startNotifications();
  this.write=d=>rx.writeValueWithResponse?rx.writeValueWithResponse(new Uint8Array(d)):rx.writeValue(new Uint8Array(d));this.close=()=>{try{device.gatt.disconnect()}catch{}}}
 command(data,timeout=4000){return new Promise((resolve,reject)=>{const t=setTimeout(()=>{this.pending=null;reject(Error(T('net.noAnswer')))},timeout);
  this.pending=f=>{clearTimeout(t);resolve(f)};this.write(new Uint8Array(data)).catch(e=>{clearTimeout(t);this.pending=null;reject(e)})})}
 // Device query (13), then app start (5: self info with the radio); a board still booting gets three tries.
 async hello(){let info=null;for(let i=0;i<3&&!info;i++)info=await this.command([22,3]).catch(e=>{if(i===2)throw e;return null});
  if(info[0]!==13)throw Error(T('net.noAnswer'));this.info=info;await this.self()}
 async self(){const f=await this.command([1,3,0,0,0,0,0,0,...new TextEncoder().encode('MeshMesh site')]);if(f[0]!==5)throw Error(T(f[0]===1?'net.badState':'net.noAnswer'));
  this.key=Array.from(f.subarray(4,36),x=>x.toString(16).padStart(2,'0')).join('');this.radio={frequency:u32(f,48)/1000,bandwidth:u32(f,52)/1000,sf:f[56],cr:f[57]};this.name=new TextDecoder().decode(f.subarray(58))}
 get model(){return cstr(this.info.subarray(20,60))}
 get version(){return cstr(this.info.subarray(60,80))}
 get meshmesh(){return this.version.startsWith('MeshMesh')}
 // Set radio params (11): kHz-thousandths; the client repeat flag (firmware 9+) is sent back as it was.
 async setRadio(r){const d=[11,...le32(Math.round(r.frequency*1000)),...le32(Math.round(r.bandwidth*1000)),r.sf,r.cr];if(this.info[1]>=9&&this.info.length>80)d.push(this.info[80]);
  const f=await this.command(d,8000);if(f[0]!==0)throw Error(T('net.rejected')+(f[0]===1?' ('+f[1]+')':''));await this.self();if(!same(this.radio,r))throw Error(T('net.notApplied'))}
}

// Why this profile cannot go to the connected board, or ''.
function blocked(r){if(link&&link.meshmesh&&!((r.frequency>=863&&r.frequency<=870)||(r.frequency>=902&&r.frequency<=928)))return T('net.outOfRange');
 if(link&&link.meshmesh&&![62.5,125,250,500].includes(r.bandwidth))return T('net.badBandwidth');return ''}
async function connect(kind){if(busy)return;busy=true;result=null;ack=false;render();
 try{await disconnect();link=new Link();say(T('net.connecting'));if(kind==='ble')await link.ble();else await link.usb();await link.hello();say('')}
 catch(e){const gone=e.name==='NotFoundError';await disconnect();say(gone?'':T('net.connectFailed')+' '+e.message,gone?'':'warn')}
 busy=false;render()}
async function disconnect(){if(link){const l=link;link=null;await l.close()}}
async function write(r,previous){if(!link||busy)return;busy=true;render();const before=link.radio;
 try{await link.setRadio(r);if(!previous)store('mm-net-prev-'+link.key.slice(0,16),before);else store('mm-net-prev-'+link.key.slice(0,16),null);
  result=previous?T('net.restored'):fill('net.written',{name:link.name});await disconnect();say(result+' '+T('net.next'),'ok')}
 catch(e){say(T('net.writeFailed')+' '+e.message,'warn')}
 busy=false;render()}

// Only a changed catalogue entry of the chosen network is shown here; the choice itself is the marked row and the summary.
function chosenBox(){if(!choice)return '';
 const now=find(choice.id),nr=now&&radioOf(now.n);
 return nr&&!same(nr,choice.radio)?`<div class="note warn">${fill('net.changed',{radio:esc(text(nr))})} <button class="linkbtn" type="button" data-net-do="take">${T('net.takeNew')}</button></div>`:''}
function boardBox(){
 if(!link)return `<div class="row">${'serial' in navigator?`<button class="btn primary" type="button" data-net-do="usb"${busy?' disabled':''}>${T('net.usb')}</button>`:''}${'bluetooth' in navigator?`<button class="btn" type="button" data-net-do="ble"${busy?' disabled':''}>${T('net.ble')}</button>`:''}</div>`+
  ('serial' in navigator||'bluetooth' in navigator?`<p class="small" style="color:var(--faint)">${T('net.portNote')}</p>`:`<div class="note warn">${T('web.unsupported')}</div>`);
 const r=choice&&choice.radio,why=r&&blocked(r),prev=store('mm-net-prev-'+link.key.slice(0,16)),cross=r&&bandOf(r.frequency)!==bandOf(link.radio.frequency),done=r&&same(r,link.radio);
 return `<div class="note"><b>${esc(link.name)}</b> · ${esc(link.model)} · ${esc(link.version)}<br>${T('net.now')} ${esc(text(link.radio))}${r&&!done?`<br>${T('net.will')} <b>${esc(text(r))}</b>`:''}<br><small>${T('net.keeps')}</small></div>`+
  (why?`<div class="note warn">${esc(why)}</div>`:'')+(r&&!why&&!done&&cross?`<label class="small" style="display:flex;gap:8px;align-items:center;margin-top:10px"><input type="checkbox" data-net-ack${ack?' checked':''}>${fill('net.band',{band:bandOf(r.frequency)})}</label>`:'')+
  `<div class="row" style="margin-top:10px">${r&&!why&&!done?`<button class="btn primary" type="button" data-net-do="write"${busy||cross&&!ack?' disabled':''}>${T('net.write')}</button>`:''}${done?`<span class="tag ok" style="margin:0">${T('net.already')}</span>`:''}`+
  `${prev&&!same(prev,link.radio)?`<button class="btn" type="button" data-net-do="previous"${busy?' disabled':''}>${fill('net.previous',{radio:esc(text(prev))})}</button>`:''}<button class="btn" type="button" data-net-do="disconnect">${T('net.disconnect')}</button></div>`}
// One row per network: place, network, radio, distance; the community link beside it.
function listBox(){
 if(!cat)return `<p class="small">${T('net.noCatalogue')}</p>`;
 const list=rows(),part=list.slice(0,shown);
 return part.map(({n,l,place,d})=>{const url=/^https?:\/\//.test(n.community_url||'')?n.community_url:'',km=d!=null?` · ${d<10?d.toFixed(1):Math.round(d)} ${T('net.km')}`:'';
  return `<div class="netrow${choice&&choice.id===n.id?' on':''}"><button type="button" data-net="${esc(n.id)}" title="${esc(n.note||n.id)}"><b>${esc(place.name)}</b><span>${esc(l===place?n.name:l.name+': '+n.name)}</span><small>${esc(text(radioOf(n)))}${km}</small></button>`+
   (url?`<a href="${esc(url)}" target="_blank" rel="noopener" title="${T('net.community')}">↗</a>`:'')+'</div>'}).join('')+
  (list.length>shown?`<button class="linkbtn small" type="button" data-net-do="more" style="margin-top:10px">${fill('net.more',{n:list.length-shown})}</button>`:'')+(list.length?'':`<p class="small">${T('net.nothing')}</p>`)}
function render(){const box=el('netPick');if(!box)return;
 el('netSum').textContent=choice?`${choice.place} · ${choice.name}`:'';
 el('netChosen').innerHTML=chosenBox();el('netBoard').innerHTML=boardBox();el('netList').innerHTML=listBox();el('netPos').textContent=posNote;el('netPos').hidden=!posNote;
 el('netCat').innerHTML=cat?fill('net.catalogue',{date:esc(cat.updated),n:cat.locations.length}):''}

el('netPick').addEventListener('click',e=>{const b=e.target.closest('[data-net],[data-net-do]');if(!b||b.disabled)return;
 if(b.dataset.net){const x=find(b.dataset.net);if(!x)return;choice=choice&&choice.id===x.n.id?null:{id:x.n.id,place:x.l.name,name:x.n.name,radio:radioOf(x.n)};store('mm-net',choice);ack=false;render();return}
 const a=b.dataset.netDo;
 if(a==='take'){const x=find(choice.id);if(x){choice={...choice,radio:radioOf(x.n)};store('mm-net',choice);render()}}
 else if(a==='usb'||a==='ble')connect(a);
 else if(a==='disconnect')disconnect().then(render);
 else if(a==='write')write(choice.radio,false);
 else if(a==='previous')write(store('mm-net-prev-'+link.key.slice(0,16)),true);
 else if(a==='more'){shown+=20;render()}
 else if(a==='locate')locate();
 else if(a==='update')update()});
el('netPick').addEventListener('change',e=>{if(e.target.matches('[data-net-ack]')){ack=e.target.checked;render()}});
el('netQ').addEventListener('input',e=>{q=e.target.value;shown=5;el('netList').innerHTML=listBox()});
// The site's language changes the texts.
new MutationObserver(render).observe(document.documentElement,{attributes:true,attributeFilter:['lang']});
// A link to the section opens it.
if(location.hash==='#netPick')el('netPick').open=true;
load();
