// MeshMesh for Android: the device's own page (web/index.html) with a connection screen and its
// fetch('/api/…') answered by the app over Wi-Fi, Bluetooth or USB (MainActivity / MeshService).
// Uses the page's globals: auth, timer, route, config, status, conn, request, command, refresh, show…
(()=>{'use strict';
const N=window.MeshNative;if(!N)return;
const H={tab:null,devices:{wifi:[],ble:[],usb:[]},scanning:{},done:{},state:{state:'idle'},prefs:{},sel:'',inApp:false,armed:0,upd:{state:'idle'},manual:false,updNoted:false};
const KIND={wifi:['wifi','Wi-Fi'],ble:['ble','Bluetooth'],usb:['bolt','USB'],tcp:['bolt','USB через компьютер']};

// fetch('/api/…') → the app; everything else (OpenStreetMap for map preparation) → the network.
const pending=new Map();let seq=0;const netFetch=window.fetch.bind(window);
function toBase64(bytes){let s='';for(let i=0;i<bytes.length;i+=32768)s+=String.fromCharCode.apply(null,bytes.subarray(i,i+32768));return btoa(s)}
window.fetch=function(input,init){
 const url=typeof input==='string'?input:input.url,path=url.replace(/^https:\/\/appassets\.androidplatform\.net/,'');
 if(!path.startsWith('/api/'))return netFetch(input,init);
 const method=(init&&init.method)||'GET',body=init&&init.body;
 return new Promise((resolve,reject)=>{const id=++seq;pending.set(id,{resolve,reject});
  N.request(id,method,path,body==null?null:body instanceof Uint8Array?toBase64(body):String(body))})};

window.MeshHost={
 done(id,status,body,b64){const p=pending.get(id);if(!p)return;pending.delete(id);
  if(!status){p.reject(new TypeError(body||'нет связи'));return}
  const data=b64?Uint8Array.from(atob(body),c=>c.charCodeAt(0)):body;
  p.resolve(new Response(data,{status,headers:{'Content-Type':b64?'application/octet-stream':'text/plain; charset=utf-8'}}))},
 devices(kind,list,done){H.devices[kind]=list||[];H.scanning[kind]=!done;if(done)H.done[kind]=true;if(H.tab===kind)renderPanel()},
 state(s){const before=H.state;H.state=s||{state:'idle'};
  if(s.state==='connected')enterApp();
  else if(s.state==='flashing'){if(H.inApp)leaveApp(null);else renderPanel()}
  else if(s.state==='lost'||s.state==='idle'){if(H.inApp)leaveApp(s.message||'Связь с устройством потеряна',s.state==='lost'?'bad':'muted');else renderPanel()}
  else if(s.state==='failed'){renderPanel();notify(s.message||'Не удалось подключиться','bad')}
  else renderPanel();
  void before},
 toast(text,tone){notify(text,tone||'accent')},
 // A scanned QR code or a meshcore:// link: the page asks to join the channel once a board is connected.
 channelLink(text){text=String(text||'').trim();
  if(!/^meshcore:\/\/channel\/add\?/i.test(text)){notify('Это не ссылка на канал MeshCore','warn');return}
  H.link=text;if(H.inApp)openLink();else notify('Подключитесь к устройству, чтобы добавить канал','accent')},
 go(target){if(H.inApp)go(target)},
 // Updates of the app (MainActivity → Updater): a card on the connection screen and on «Подключения».
 update(u){const was=H.upd.state;H.upd=u||{state:'idle'};const st=H.upd.state;
  if(st==='available'&&was!=='available'&&H.inApp&&!H.manual&&!H.updNoted){H.updNoted=true;notify(UPD_NOTE,'accent')}
  if(H.manual&&['none','error','off'].includes(st)){H.manual=false;notify(st==='none'?'Установлена последняя версия приложения':H.upd.message,st==='none'?'ok':st==='off'?'warn':'bad')}
  else if(st==='error')notify(H.upd.message,'bad');
  if(st!=='checking'&&st!=='downloading')H.manual=false;
  if(!H.inApp)renderPanel();else if(route==='connect')renderConnect();else if(route==='home')renderHome();else if(route==='settings')renderSettings()},
 back(){
  if(typeof standaloneMode!=='undefined'&&standaloneMode){standaloneMode=false;$('app').hidden=true;$('login').hidden=false;loginHud();return true}
  if(H.inApp&&!$('back').hidden){$('back').click();return true}
  return false}};

// The connection screen replaces the password form of the login page.
const login=$('login');$('loginForm').hidden=true;
login.querySelector('.brand small').textContent='Ваши люди. Ваша сеть.';
login.querySelector('.brand').insertAdjacentHTML('afterend','<div id="hostPanel"></div>');
const panel=$('hostPanel');
function rssiBars(r){return r==null?'':bars(r>-60?4:r>-70?3:r>-80?2:1,'var(--accent)')}
function row(attrs,icon,cls,name,detail,side,sel){return `<button class="hrow${sel?' sel':''}" ${attrs}><span class="icbox">${ic(icon,cls)}</span><span class="main"><b>${esc(name)}</b><small>${esc(detail)}</small></span><span class="side">${side||''}</span></button>`}
function scanButton(kind,text){const busy=H.scanning[kind];return `<button class="btn save" data-hscan="${kind}"${busy?' disabled':''}>${busy?'Поиск…':text}</button>`}
function renderPanel(){
 const tab=H.tab,s=H.state,flashing=s.state==='flashing',busy=s.state==='connecting'||flashing;
 let h=`<div class="tabs">${['wifi','ble','usb'].map(k=>`<button data-htab="${k}" class="${k===tab?'on':''}">${ic(KIND[k][0])}${KIND[k][1]}</button>`).join('')}</div><div class="card hbody">`;
 if(tab==='wifi'){const list=H.devices.wifi,pass=H.prefs.passwords||{};
  h+=`<div class="hlink">${ic('wifi','info')}<span>Включите точку доступа на устройстве: M9 — «Связь», Heltec — страница Wi-Fi.</span></div>`;
  h+=list.length?list.map(d=>row(`data-hwifi="${esc(d.id)}"`,'wifi','info',d.name,pass[d.id]?'пароль сохранён':'нужен пароль',rssiBars(d.rssi),d.id===H.sel)).join(''):`<div class="hempty">${H.done.wifi?'Точки MM-… не найдены. Можно подключиться и без списка — Android покажет найденные сети.':'Найдите точку MM-… или подключитесь через запрос Android.'}</div>`;
  h+=scanButton('wifi','Искать точки MM-…');
  h+=`<label class="field">Пароль Wi-Fi${H.sel?' для '+esc(H.sel):''}<input id="hPass" type="password" autocomplete="off" value="${esc(pass[H.sel]||'')}"></label><p class="small muted">Пароль показан на устройстве. Если раньше подключались по USB или Bluetooth, приложение уже знает пароль текущей загрузки.</p>`;
  h+=`<button class="btn primary save" data-hgo="wifi"${busy?' disabled':''}>Подключиться</button>`;
  h+=`<details class="more" style="background:var(--bg)"><summary>Другой адрес</summary><label class="field">Адрес устройства<input id="hAddr" value="${esc(H.prefs.address||'192.168.4.1')}" autocomplete="off"></label><p class="small muted">192.168.4.1 — точка доступа MeshMesh. Другой адрес — устройство в сети, к которой телефон уже подключён.</p></details>`}
 else if(tab==='ble'){const list=H.devices.ble;
  h+=`<div class="hlink">${ic('ble','info')}<span>Включите BLE на устройстве. При первом подключении Android спросит PIN — он на экране устройства.</span></div>`;
  h+=list.length?list.map(d=>row(`data-hble="${esc(d.id)}"`,'ble','info',d.name,d.bonded?'сопряжено':'новое устройство',rssiBars(d.rssi))).join(''):`<div class="hempty">${H.scanning.ble?'Ищу устройства MeshMesh…':H.done.ble?'Устройства MeshMesh не найдены: включён ли BLE на плате?':'Нажмите «Искать», чтобы найти устройства рядом.'}</div>`;
  h+=scanButton('ble','Искать устройства')}
 else{const list=H.devices.usb;
  h+=`<div class="hlink">${ic('bolt','acc')}<span>Подключите плату кабелем USB-C через OTG-переходник.</span></div>`;
  h+=list.length?list.map(d=>row(`data-husb="${esc(d.id)}"`,'bolt','acc',d.name,'Нажмите, чтобы подключиться',ic('next','faint'))).join(''):`<div class="hempty">Плата по USB не найдена.</div>`;
  h+=`<button class="btn save" data-hscan="usb">Обновить список</button>`;
  const [host,port]=(H.prefs.bridge||'10.0.2.2:8771').split(':');
  h+=`<details class="more" style="background:var(--bg)"><summary>USB через компьютер</summary><p class="small muted">Плата подключена к компьютеру, на нём запущен <span class="mono">tools/usb_tcp_bridge.py</span>; 10.0.2.2 — компьютер для эмулятора Android.</p><div class="two"><label class="field">Адрес<input id="hHost" value="${esc(host)}"></label><label class="field">Порт<input id="hPort" type="number" value="${esc(port||8771)}"></label></div><button class="btn save" data-hgo="tcp"${busy?' disabled':''}>Подключиться к мосту</button></details>`}
 h+='</div>';
 if(flashing)h=`<div class="card conn"><div class="top"><span class="icbox"><span class="spin"></span></span><div><b>Обновление прошивки</b><small>${esc(s.message||'')}</small></div></div>${s.progress!=null?`<progress value="${s.progress/100}" style="width:100%;margin-top:10px"></progress>`:''}<p class="small muted" style="margin:8px 0 0">Не отключайте кабель и не закрывайте приложение. Ключ, настройки, контакты и история сохранятся.</p></div>`+h;
 else if(busy)h+=`<div class="card hstate"><span class="spin"></span><div style="flex:1">${esc(s.message||'Подключение…')}</div><button class="btn" data-hstop>Отмена</button></div>`;
 else if(s.state==='failed')h+=`<div class="card hstate">${ic('failed','bad')}<div class="bad" style="flex:1">${esc(s.message||'Не удалось подключиться')}</div>${s.flash?'<button class="btn primary" data-hflash="retry">Повторить</button>':''}</div>`;
 const last=H.prefs.last;
 if(last&&last.kind&&!busy)h+=`<h3>Последнее устройство</h3><div class="card">${row('data-hlast','radio','acc',last.label||'MeshMesh',KIND[last.kind]?.[1]+' · подключиться снова','',false)}</div>`;
 panel.innerHTML=updateCard(false)+h}

function connect(spec){N.connect(JSON.stringify(spec))}
// A found app update, once connected: a dot on «Связь» (home) and «Подключения» (settings), where its card is,
// and one notice per run of the app, on entering the device or when the check finishes later.
const UPD_NOTE='Вышла новая версия приложения: «Связь» → «Обновить»';
function updatePending(){return ['available','downloading','ready'].includes(H.upd.state)}
function markConnect(list){if(!updatePending())return;const t=$(list).querySelector('[data-go="connect"]');if(t)t.insertAdjacentHTML('beforeend','<span class="hdot"></span>')}
const pageRenderHome=window.renderHome,pageRenderSettings=window.renderSettings;
window.renderHome=function(){pageRenderHome();markConnect('tiles')};
window.renderSettings=function(){pageRenderSettings();markConnect('settingsList')};
function updateCard(always){const u=H.upd,st=u.state,mb=u.size?` · ${(u.size/1048576).toFixed(1)} МБ`:'';
 if(!always&&!['available','downloading','ready'].includes(st))return'';
 const text=st==='available'?`Доступна версия ${esc(u.name)}${mb}`:st==='downloading'?`Загрузка ${esc(u.name)}: ${u.progress|0}%`:st==='ready'?'Загружено: подтвердите установку в окне Android':st==='checking'?'Проверка…':`Версия ${esc(N.version())}`;
 const btn=st==='available'?'<button class="btn primary" data-hupd="install">Обновить</button>':st==='ready'?'<button class="btn primary" data-hupd="install">Установить</button>':st==='downloading'||st==='checking'?'':'<button class="btn soft" data-hupd="check">Проверить обновления</button>';
 return `<div class="card conn"><div class="top"><span class="icbox">${ic('down',st==='available'||st==='ready'?'acc':'info')}</span><div><b>Приложение MeshMesh</b><small>${text}</small></div></div>${st==='downloading'?`<progress value="${(u.progress|0)/100}" style="width:100%"></progress>`:''}${btn?`<div class="extra"><div class="btns">${btn}</div></div>`:''}</div>`}
// Firmware of the site over the USB link (MeshService.flashFirmware → flash/FirmwareUpdate): ESP32 boards
// by their ROM loader, nRF52 (GAT562, T114) by its serial DFU bootloader.
let fwSite=null;
function loadFwSite(){if(fwSite)return;fwSite={};netFetch('https://ikrasnodymov.github.io/meshmesh/firmware/boards.json',{cache:'no-store'}).then(r=>r.json()).then(j=>{fwSite=j}).catch(()=>{fwSite={error:true}}).finally(()=>{if(H.inApp&&route==='connect')renderConnect()})}
function newerVersion(a,b){const x=String(a).split('.').map(Number),y=String(b).split('.').map(Number);for(let i=0;i<Math.max(x.length,y.length);i++){if((x[i]||0)!==(y[i]||0))return (x[i]||0)>(y[i]||0)}return false}
function firmwareCard(){const s=H.state;if(!H.inApp||!['usb','tcp'].includes(s.kind))return'';loadFwSite();
 const env=status.board==='heltec_v4'&&status.psram>3e6?'heltec_v4_r8':status.board,b=fwSite.boards?.find(x=>x.env===env),cur=String(status.firmware||'').replace(/^MeshMesh\s*/,''),esp=b&&(b.install!=='uf2'||!!b.dfu),newer=esp&&newerVersion(fwSite.version,cur);
 const text=fwSite.error?'Сайт недоступен: для обновления нужен интернет':!fwSite.boards?'Проверка версии на сайте…':!b?'На сайте нет прошивки для этой платы':!esp?`На сайте ${fwSite.version}: эта плата обновляется с сайта или компьютера`:`Установлена ${esc(cur)} · на сайте ${esc(fwSite.version)}`;
 return `<div class="card conn"><div class="top"><span class="icbox">${ic('bolt',newer?'acc':'info')}</span><div><b>Прошивка платы</b><small>${text}</small></div></div>${esp?`<div class="extra"><div class="btns"><button class="btn ${newer?'primary':'soft'}" data-hflash="go">${newer?'Обновить прошивку':'Переустановить'}</button></div><small class="muted">По USB с сайта, ${esc(b.name)}; данные платы сохраняются</small></div>`:''}</div>`}
document.addEventListener('click',e=>{const t=e.target.closest('[data-hflash]');if(!t)return;
 if(t.dataset.hflash==='retry'){N.flashFirmware(true);return}
 if(Date.now()-H.armed>6000){H.armed=Date.now();notify('Нажмите ещё раз: плата перезапустится в загрузчик, запись займёт 1–3 минуты'+(status.wifi_radio===false?' (Android может спросить доступ к USB загрузчика — разрешите)':'')+'. Ключ, настройки, контакты и история сохранятся','warn');return}
 H.armed=0;N.flashFirmware(false)});
document.addEventListener('click',e=>{const t=e.target.closest('[data-hupd]');if(!t)return;
 if(t.dataset.hupd==='check'){H.manual=true;N.checkUpdate()}else N.installUpdate()});
// The connection screen's header: the page's hud() would keep the last page's title and BACK.
function loginHud(){hud();$('title').textContent='MeshMesh';$('title').className='home';$('back').hidden=true;$('icons').innerHTML=''}
panel.addEventListener('click',e=>{const t=e.target.closest('[data-htab],[data-hscan],[data-hwifi],[data-hble],[data-husb],[data-hgo],[data-hstop],[data-hlast]');if(!t)return;const d=t.dataset;
 if(d.htab){H.tab=d.htab;renderPanel();if(!H.done[d.htab]&&!H.scanning[d.htab])N.scan(d.htab);return}
 if(d.hscan){H.scanning[d.hscan]=d.hscan!=='usb';renderPanel();N.scan(d.hscan);return}
 if(d.hwifi!==undefined){H.sel=H.sel===d.hwifi?'':d.hwifi;renderPanel();return}
 if(d.hble){N.stopScan();H.scanning.ble=false;connect({kind:'ble',id:d.hble});return}
 if(d.husb){connect({kind:'usb',id:d.husb});return}
 if(d.hstop!==undefined){N.disconnect();return}
 if(d.hlast!==undefined){const l=H.prefs.last;if(l.kind==='wifi'){H.tab='wifi';H.sel=l.id&&l.id.startsWith('MM-')?l.id:'';renderPanel();return}
  if(l.kind==='tcp'){const [host,port]=String(l.id).split(':');connect({kind:'tcp',host,port:+port});return}connect({kind:l.kind,id:l.id});return}
 if(d.hgo==='wifi'){const password=$('hPass').value,address=($('hAddr')?.value||'192.168.4.1').trim();if(!password){notify('Введите пароль Wi-Fi с экрана устройства','warn');return}
  connect({kind:'wifi',ssid:address==='192.168.4.1'?H.sel:'',password,address});return}
 if(d.hgo==='tcp'){connect({kind:'tcp',host:$('hHost').value.trim(),port:+$('hPort').value||8771});return}});

function openLink(){const l=H.link;H.link='';if(l&&window.openChannelLink)openChannelLink(l)}

// Connected: the same steps as the page's own login.
async function enterApp(){
 if(H.inApp)return;H.inApp=true;auth='MeshMesh app';
 try{config=await request('/api/config')}catch(e){H.inApp=false;auth='';N.disconnect();notify('Устройство не ответило: '+e.message,'bad');return}
 $('login').hidden=true;$('app').hidden=false;await refresh();clearInterval(timer);timer=setInterval(refresh,H.state.kind==='ble'?4000:3000);route='';show();
 const note=updatePending()&&!H.updNoted;if(note)H.updNoted=true;
 notify('Подключено: '+(H.state.label||'')+(note?'. '+UPD_NOTE:''),note?'accent':'ok');openLink()}
function leaveApp(message,tone){
 H.inApp=false;auth='';clearInterval(timer);if(radarTimer){clearInterval(radarTimer);radarTimer=null;radarData=null}
 for(const [,p] of pending)p.reject(new TypeError('нет связи'));pending.clear();
 H.prefs=JSON.parse(N.prefs());$('app').hidden=true;$('login').hidden=false;loginHud();renderPanel();if(message)notify(message,tone)}

// Connections page: the app's link first, with disconnect and, from Bluetooth, the faster Wi-Fi.
const pageRenderConnect=window.renderConnect;
window.renderConnect=function(){pageRenderConnect();const s=H.state;if(!H.inApp||!KIND[s.kind])return;const c=conn||{};
 const toWifi=s.kind==='ble'&&status.wifi&&c.ssid&&c.password?`<button class="btn soft" data-hconn="wifi">Перейти на Wi-Fi</button>`:'';
 $('connList').insertAdjacentHTML('afterbegin',`<div class="card conn"><div class="top"><span class="icbox">${ic(KIND[s.kind][0],'acc')}</span><div><b>Приложение · ${KIND[s.kind][1]}</b><small>${esc(s.label||'')}</small></div></div><div class="extra"><div class="btns">${toWifi}<button class="btn danger" data-hconn="off">Отключиться</button></div>${toWifi?'<small class="muted">Wi-Fi быстрее Bluetooth для карт и радара</small>':''}</div></div>`+firmwareCard()+updateCard(true))};
document.addEventListener('click',e=>{const t=e.target.closest('[data-hconn]');if(!t)return;
 if(t.dataset.hconn==='off'){N.disconnect();return}
 const c=conn||{};N.disconnect();setTimeout(()=>connect({kind:'wifi',ssid:c.ssid,password:c.password,address:'192.168.4.1'}),300)});

// Switches: over USB or BLE the access point is not this page's link; BLE off ends a BLE link.
const pageToggle=window.toggle;
window.toggle=async function(which){const k=H.state.kind;
 if(which==='wifi'&&k!=='wifi'){try{const r=await command('wifi');notify(r.includes('on')?'Точка доступа включена':'Точка доступа выключена');await refresh();await loadConnections()}catch(e){notify(e.message,'bad')}return}
 if(which==='wifi'){if(Date.now()-wifiArmed>5000){wifiArmed=Date.now();notify('Нажмите ещё раз: приложение потеряет связь по Wi-Fi. Включить точку снова можно на устройстве или по USB/Bluetooth','warn');return}
  try{await command('wifi');N.disconnect();notify('Точка доступа выключается','warn')}catch(e){notify(e.message,'bad')}return}
 if(which==='ble'&&k==='ble'&&status.ble){if(Date.now()-H.armed>5000){H.armed=Date.now();notify('Нажмите ещё раз: Bluetooth устройства выключится, приложение потеряет связь','warn');return}
  try{await command('ble')}catch{}N.disconnect();return}
 return pageToggle(which)};

// Files: the page's downloads (map packages, settings) go to an Android save dialog.
window.download=function(data,name,type){const bytes=typeof data==='string'?new TextEncoder().encode(data):data instanceof Uint8Array?data:new Uint8Array(data);N.saveFile(name,type||'application/octet-stream',toBase64(bytes))};

// Map upload over Bluetooth: about 2 KB/s, so a large package asks once more.
const pageUpload=window.uploadMap;
window.uploadMap=function(){const f=$('mapFile').files[0];
 if(f&&H.state.kind==='ble'&&f.size>300000&&Date.now()-H.armed>8000){H.armed=Date.now();notify(`По Bluetooth ${(f.size/1048576).toFixed(1)} МБ займут около ${Math.ceil(f.size/2048/60)} мин; по USB или Wi-Fi намного быстрее. Нажмите ещё раз, чтобы начать`,'warn');return}
 return pageUpload()};
$('uploadMap').onclick=window.uploadMap;
$('saveBuilder').hidden=true; // the app itself prepares maps: no need to save the page
$('uploader').querySelector('p').textContent='Выберите файл .mmmap. Он передаётся по текущему подключению; интернет для этого не нужен.';

H.upd=JSON.parse(N.updateInfo());
H.prefs=JSON.parse(N.prefs());const last=H.prefs.last;
H.tab=last&&KIND[last.kind]&&last.kind!=='tcp'?last.kind:'usb';
loginHud();renderPanel();N.scan('usb');if(H.tab!=='usb'&&H.tab==='ble'&&last)H.done.ble=false;
const s=JSON.parse(N.state());if(s.state!=='idle')MeshHost.state(s);
for(const v of document.querySelectorAll('.version'))v.textContent=N.version();
})();
