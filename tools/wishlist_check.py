#!/usr/bin/env python3
"""Check the 0.19 wishlist over real LoRa between V4 and T114, restoring user settings.

Creates a temporary encrypted channel and performs normal restarts; requires verified
backups and the intended firmware already installed. Reports and recovery data stay
in ignored local directories with mode 0600. USB key events do not test physical buttons.
"""
import sys,json,time,os,argparse
from pathlib import Path
from contextlib import ExitStack
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
from device import connect,command
from radio_check import PROFILE_FIELDS,delivery
def read(d,name):return json.loads(command(d,name,timeout=25))
def apply(d,v):
 for _ in range(20):
  r=command(d,'set '+json.dumps(v,ensure_ascii=False),timeout=25)
  if r.startswith('OK'):return
  if r!='ERR radio busy; retry':raise RuntimeError(r)
  time.sleep(.5)
 raise TimeoutError('radio busy applying settings')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--v4',default='/dev/cu.usbmodem101');parser.add_argument('--t114',default='/dev/cu.usbmodem1101')
parser.add_argument('--output',type=Path,default=Path('artifacts/wishlist/hardware.json'))
parser.add_argument('--after-history',action='store_true',help='Continue UI/reboot checks from five passed RF/history checks on the same images')
args=parser.parse_args()
def private(p,value):
 p.parent.mkdir(parents=True,exist_ok=True);p.touch(mode=0o600);p.chmod(0o600);p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
def summary(s):return {k:s.get(k) for k in ['board','version','revision','build_sha256','boot','rx','tx','radio','radio_error','diagnostic_rx','heap','notification_dropped']}
def do(d,kind,**v):
 r=command(d,kind+' do '+json.dumps(v,ensure_ascii=False),timeout=25);assert r.startswith('OK'),kind+' operation failed: '+r;return r
report={'passed':False,'physical_buttons':False,'checks':[],'full_finish_run':False}
previous=None
if args.after_history:
 previous=json.loads(args.output.read_text());assert previous['settings_restored'] and len(previous['checks'])==5
 report['checks']=previous['checks'];report['continued_after_history']=True;report['earlier_checks_before']=previous['before']
with ExitStack() as stack:
 dev=[stack.enter_context(connect(p)) for p in [args.v4,args.t114]]
 before=[read(d,'status') for d in dev];cfg=[read(d,'config') for d in dev];quick=[read(d,'quick') for d in dev];people=[read(d,'people') for d in dev];dice=[read(d,'dice') for d in dev];keys=[read(d,'key') for d in dev]
 report['before']=[summary(s) for s in before];private(Path('backups/wishlist-runtime-recovery.json'),{'config':cfg,'quick':quick,'people':people,'dice_modes':[x['mode'] for x in dice],'keys':keys})
 assert all(s['version']=='0.19.0' and s['radio'] and not s['radio_error'] for s in before)
 for s,board in zip(before,['heltec-v4','heltec-t114']):
  package=json.loads(Path(f'artifacts/meshmesh-{board}-0.19.0/manifest.json').read_text())
  expected=package['app_elf_sha256'] if board=='heltec-v4' else package['firmware.bin']['sha256']
  assert s['build_sha256'].lower()==expected.lower(),'installed image differs from the intended package'
 report['images_match_packages']=True
 if previous:assert [s['build_sha256'] for s in before]==[s['build_sha256'] for s in previous['before']],'continuation requires identical images'

 assert all(abs(cfg[0][k]-cfg[1][k])<.0001 for k in PROFILE_FIELDS)
 added=[];channel=None;ntp_saved=None
 try:
  for d in dev:
   command(d,'clock '+json.dumps({'unix':int(time.time()),'source':'phone'}));command(d,'hello')
  time.sleep(3)
  if not args.after_history:
   for sender,receiver,a,b in [(dev[0],dev[1],before[0],before[1]),(dev[1],dev[0],before[1],before[0])]:
    delivery(sender,receiver,a['node'],b['node'],'Проверка 0.19.0 '+str(time.time_ns()))
   print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('direct LoRa both directions, ACK and one received copy')
   for d in dev:do(d,'people',action='settings',ble=True,wifi=False,personal_only=False,window=30,rssi=-100)
   time.sleep(12)
   counts=[read(d,'people') for d in dev];assert all(x['ble_state']=='listening' and x['ble_devices']>0 for x in counts),'BLE observations missing'
   report['ble_counts']=[x['ble_devices'] for x in counts];print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('real BLE observations on both boards with BLE advertising enabled')
   for d,q in zip(dev,quick):
    do(d,'quick',action='set',index=0,text='Принято\nНа месте');assert read(d,'quick')['presets'][0]=='Принято\nНа месте'
    assert command(d,'quick do '+json.dumps({'action':'target','to':'-1'})).startswith('ERR')
    assert command(d,'quick do '+json.dumps({'action':'send','index':0,'to':'1122334455667788'})).startswith('ERR')
   print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('multiline UTF-8 templates; missing/invalid target fails')
  private_key=os.urandom(16).hex();result=do(dev[0],'channel',action='add',name='Wishlist check',key=private_key);channel=result.split()[-1];added.append((dev[0],channel))
  chan=next(c for c in read(dev[0],'channels')['channels'] if c['id']==channel)
  do(dev[1],'channel',action='add',name='Wishlist check',key=private_key);added.append((dev[1],channel))
  for d,cap in zip(dev,[16,8]):
   do(d,'channel',action='policy',channel=channel,priority=0,device_limit=cap,app_limit=8,led=2,wake=0,popup=0)
   do(d,'quick',action='target',to=channel);do(d,'quick',action='gps',enabled=False)
  if not args.after_history:
   def receive_quick(text):
    text+=' #'+str(time.time_ns());initial_ui=read(dev[1],'ui');assert initial_ui['page']=='home';before_unread=initial_ui['unread']
    do(dev[0],'quick',action='set',index=0,text=text);do(dev[0],'quick',action='send',index=0)
    end=time.monotonic()+30
    while time.monotonic()<end:
     observed=read(dev[1],'ui')
     if observed['unread']<=before_unread:time.sleep(.2);continue
     incoming=[m for m in read(dev[1],'messages') if not m['outgoing'] and m['destination']==channel and m['text']==text]
     if incoming:
      assert len(incoming)==1;return observed
     time.sleep(.2)
    raise TimeoutError('temporary-channel RF receipt not observed')
   # Screen wake and pop-ups are independent, including per-channel inheritance.
   apply(dev[1],{'apps':'-dice -quick -people -wardrive','dim_after':10,'notify_wake':0,'notify_popup':0,'sound':False})
   command(dev[1],'uikey 134');command(dev[1],'uikey 134');do(dev[1],'channel',action='policy',channel=channel,wake=3,popup=3)
   time.sleep(12);assert read(dev[1],'ui')['screen_off']
   u=receive_quick('Notification off');assert u['screen_off'] and not u['popup']
   apply(dev[1],{'notify_wake':2,'notify_popup':2});time.sleep(12)
   u=receive_quick('Ordinary message');assert u['screen_off']
   u=receive_quick('@['+cfg[1]['name']+'] message');report['mention_ui']={k:u[k] for k in ['page','screen_off','popup','unread']};assert not u['screen_off'] and u['popup']
   # A large history reply may outlast the popup: Enter would then open Messages.
   command(dev[1],'uikey 134');command(dev[1],'uikey 134')
   do(dev[1],'channel',action='policy',channel=channel,wake=0,popup=0)
   time.sleep(12);u=receive_quick('@['+cfg[1]['name']+'] override');assert u['screen_off']
   print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('real incoming RF: off, ordinary/mention filtering, independent wake and popup, channel override')
   for i in range(18):
    text='Wishlist '+str(i)+' '+str(int(time.time()));do(dev[0],'quick',action='set',index=0,text=text);do(dev[0],'quick',action='send',index=0)
    end=time.monotonic()+15
    while time.monotonic()<end:
     a=[m for m in read(dev[0],'messages') if m['text']==text];b=[m for m in read(dev[1],'messages') if m['text']==text]
     if a and a[-1]['status']==2 and len(b)==1:break
     time.sleep(.5)
    else:raise AssertionError('private channel reception timed out')
    time.sleep(1)
    if (i+1)%6==0:print('Channel sends confirmed',i+1,'of 18',flush=True)
   retained=[[m for m in read(d,'messages') if m['destination']==channel] for d in dev];assert len(retained[0])==16 and len(retained[1])==8
   assert retained[1][0]['text'].startswith('Wishlist 10 ')
   print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('18 real private-channel sends; caps 16/8 and one-copy reception')
  for d in dev:do(d,'people',action='settings',ble=True,wifi=False,personal_only=False,window=30,rssi=-100)
  for d in dev:
   do(d,'people',action='change',delta=2);assert read(d,'people')['manual']>=2
   apply(d,{'apps':'quick people dice','dim_after':0,'notify_popup':0});command(d,'uikey 134');command(d,'uikey 13');assert read(d,'ui')['page']=='quick';command(d,'uikey 134');command(d,'uikey 13')
   u=read(d,'ui');row=u['quick_row'] if 'quick_row' in u else u['quick_index']
   for _ in range((11-row)%12):command(d,'uikey 13')
   command(d,'uikey 163');assert read(d,'ui')['page']=='people';old=read(d,'people')['manual'];command(d,'uikey 13');assert read(d,'people')['manual']==old+1
   command(d,'uikey 163');n=read(d,'ui')['menu_count']
   for _ in range(n-2):command(d,'uikey 13')
   command(d,'uikey 163');assert read(d,'ui')['page']=='dice'
   assert command(d,'dice mode rpg').startswith('OK');rolls=read(d,'dice')['rolls_total']
   command(d,'uikey 13');command(d,'uikey 13');assert read(d,'dice')['rolls_total']==rolls+2
   apply(d,{'dim_after':10,'notify_popup':1,'notify_wake':1});time.sleep(12);assert read(d,'ui')['screen_off']
   command(d,'uikey 13');assert read(d,'dice')['rolls_total']==rolls+3 and not read(d,'ui')['screen_off']
   i=dev.index(d);delivery(dev[1-i],d,before[1-i]['node'],before[i]['node'],'Dice press '+str(time.time_ns()))
   assert not read(d,'ui')['popup'];command(d,'uikey 13');assert read(d,'dice')['rolls_total']==rolls+4
   command(d,'uikey 163');n=read(d,'ui')['menu_count']
   for _ in range(n-2):command(d,'uikey 13')
   command(d,'uikey 163');assert read(d,'ui')['page']!='dice'
  print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('production Quick Send, manual counter, repeated/dark dice rolls and incoming RF without stealing the press; USB key events')
  # Saved settings and compaction must recover after a normal restart.
  time.sleep(4)
  saved_history=[[{k:m[k] for k in ['source','session','id','text']} for m in read(d,'messages')] for d in dev]
  for d in dev:assert command(d,'restart').startswith('OK')
  time.sleep(3)
  for d in dev:d.close()
  dev=[stack.enter_context(connect(p)) for p in [args.v4,args.t114]];time.sleep(4)
  for d in dev:
   assert read(d,'quick')['to']==channel;assert read(d,'people')['manual']>=3
   assert command(d,'clock '+json.dumps({'unix':int(time.time()),'source':'phone'})).startswith('OK')
  for d,rows in zip(dev,saved_history):
   restored=[{k:m[k] for k in ['source','session','id','text']} for m in read(d,'messages')]
   assert all(m in restored for m in rows),'saved history lost records across restart'
  if not args.after_history:assert len([m for m in read(dev[1],'messages') if m['destination']==channel])==8
  print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('templates, target, manual count, channel policy and compacted history survive reboot')
  # No stale target is redirected to Public.
  for d in dev:do(d,'channel',action='remove',channel=channel)
  added=[]
  for d in dev:assert command(d,'quick do '+json.dumps({'action':'send','index':0})).startswith('ERR')
  print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('removed configured recipient never falls back to Public')
  do(dev[0],'people',action='settings',wifi=True);time.sleep(15);wifi=read(dev[0],'people');assert wifi['wifi_state']=='listening';report['wifi_devices']=wifi['wifi_devices']
  delivery(dev[0],dev[1],before[0]['node'],before[1]['node'],'WiFi/BLE/LoRa '+str(time.time_ns()))
  print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('passive Wi-Fi listener runs with BLE; real LoRa ACK remains available')
  internet=read(dev[0],'internet')
  if not internet['saved'] and not internet['enabled'] and not internet['ntp_enabled']:
   ntp_saved='MeshMesh-NTP-check-unavailable'
   assert command(dev[0],'internet save '+json.dumps({'ssid':ntp_saved,'password':''})).startswith('OK')
   assert command(dev[0],'internet ntp on').startswith('OK');time.sleep(3);assert not read(dev[0],'internet')['ntp_active']
   print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('fresh phone time defers periodic NTP')
   assert command(dev[0],'restart').startswith('OK');dev[0].close();time.sleep(4);dev[0]=stack.enter_context(connect(args.v4))
   assert command(dev[0],'internet ntp on').startswith('OK');time.sleep(2);assert read(dev[0],'internet')['ntp_active']
   delivery(dev[1],dev[0],before[1]['node'],before[0]['node'],'NTP/LoRa '+str(time.time_ns()))
   time.sleep(46);info=read(dev[0],'internet');assert not info['ntp_active'] and info['state']=='off' and 500<=info['ntp_retry_seconds']<=600
   print('Stage passed',len(report['checks'])+1,flush=True);report['checks'].append('NTP stale-clock attempt ends within 45 seconds, retries in 10 minutes; real LoRa ACK while Wi-Fi scans')
   assert command(dev[0],'internet ntp off').startswith('OK');assert command(dev[0],'internet forget '+ntp_saved).startswith('OK');ntp_saved=None
  report['ntp_success_verified']=False
  after=[read(d,'status') for d in dev];assert all(s['radio'] and not s['radio_error'] for s in after);assert all(s['diagnostic_rx']==old['diagnostic_rx'] for s,old in zip(after,before))
  assert after[1]['boot']==before[1]['boot']+1 and after[0]['boot']==before[0]['boot']+(2 if 'fresh phone time defers periodic NTP' in report['checks'] else 1)
  report['after']=[summary(s) for s in after];report['passed']=True
 except Exception as e:
  report['passed']=False;report['failure_type']=type(e).__name__;raise
 finally:
  errors=[]
  if ntp_saved:
   try:command(dev[0],'internet ntp off');command(dev[0],'internet forget '+ntp_saved)
   except Exception as e:errors.append(type(e).__name__)
  for d in dev if channel and added else []:
   try:do(d,'channel',action='remove',channel=channel)
   except Exception:pass
  for d,c,q,p,k,di in zip(dev,cfg,quick,people,keys,dice):
   try:
    apply(d,{x:c[x] for x in ['apps','dim_after','sound','notify_led','notify_wake','notify_popup','notify_words','notify_failed']})
    for i,t in enumerate(q['presets']):
     if read(d,'quick')['presets'][i]!=t:do(d,'quick',action='set',index=i,text=t)
    do(d,'quick',action='target',to=q['to']);do(d,'quick',action='gps',enabled=q['gps'])
    do(d,'people',action='settings',**{x:p[x] for x in ['ble','wifi','personal_only','window','rssi']});delta=p['manual']-read(d,'people')['manual']
    while delta:step=max(-100,min(100,delta));do(d,'people',action='change',delta=step);delta-=step
    time.sleep(2.2);assert read(d,'people')['manual']==p['manual']
    assert command(d,'dice mode '+di['mode']).startswith('OK')
    now=read(d,'config');assert now==c,'user config was not restored';assert read(d,'key')==k,'key changed'
   except Exception as e:errors.append(type(e).__name__)
  report['settings_restored']=not errors
  if errors:report['passed']=False;report['restore_errors']=errors
  private(args.output,report)
  if errors:raise RuntimeError('Settings restoration failed; use the private recovery file')
print('PASS wishlist hardware:',len(report['checks']),'checks; user settings restored')
