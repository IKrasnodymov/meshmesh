#!/usr/bin/env python3
"""Exercise the MeshMesh M9 repeater role from independent official MeshCore companion firmware.

The M9 must run the repeater role (`role repeater`); the Heltec runs official MeshCore
companion_radio_usb (temporary, see docs/repeater.md). Over LoRa: adverts, a refused wrong
password, guest and admin login, status, telemetry, neighbours, ACL and remote CLI including a
setting change seen over USB and restored. Requires meshcore==2.3.14. Passwords are not printed.
"""
import asyncio,json,time
from pathlib import Path
from meshcore import MeshCore,EventType
from device import connect,command
from ports import M9_PORT,HELTEC_PORT

ROOT=Path(__file__).resolve().parents[1]

async def cli(stock,contact,line,wait=20):
 sent=await stock.commands.send_cmd(contact,line)
 assert sent.type==EventType.MSG_SENT,f'CLI {line!r} not sent'
 deadline=time.monotonic()+wait
 while time.monotonic()<deadline:
  msg=await stock.commands.get_msg(timeout=2)
  if msg and msg.type==EventType.CONTACT_MSG_RECV and msg.payload.get('pubkey_prefix')==contact['public_key'][:12]:return msg.payload['text']
  await asyncio.sleep(.3)
 raise AssertionError(f'No CLI reply to {line!r}')

async def retry(name,call,tries=3):
 # LoRa requests can be lost; the MeshCore app retries too. Attempts are reported.
 for attempt in range(1,tries+1):
  r=await call()
  if r is not None:return r,attempt
  print(f'  {name}: no reply, attempt {attempt}')
 raise AssertionError(f'No {name} response in {tries} attempts')

async def main():
 m9=connect(M9_PORT)
 def read(name):return json.loads(command(m9,name))
 stock=await MeshCore.create_serial(HELTEC_PORT,default_timeout=8)
 if stock is None:raise RuntimeError('Official MeshCore companion not responding on the Heltec')
 checks=[]
 def passed(check,**extra):checks.append({'check':check,'passed':True,**extra});print('PASS',check,extra or '')
 try:
  assert read('role')['role']=='repeater','M9 is not in the repeater role'
  status=read('status');key=status['public_key'].lower();config=read('config')
  secrets=read('server secrets');assert secrets['password']
  info=await stock.commands.send_appstart();assert info.type==EventType.SELF_INFO
  assert (await stock.commands.set_time(int(time.time()))).type==EventType.OK # as the MeshCore app does; logins carry the companion's clock
  device=await stock.commands.send_device_query();assert device.payload['ver']=='v1.17.1'
  # Same network profile as the M9; the companion's own storage was reset by the temporary install.
  p=info.payload
  if (round(p['radio_freq'],3),p['radio_bw'],p['radio_sf'],p['radio_cr'])!=(round(config['frequency'],3),config['bandwidth'],config['sf'],config['cr']):
   assert (await stock.commands.set_radio(round(config['frequency'],3),config['bandwidth'],config['sf'],config['cr'])).type==EventType.OK
   assert (await stock.commands.set_tx_power(10)).type==EventType.OK
  passed('official companion v1.17.1 on the network profile',profile=[round(config['frequency'],3),config['bandwidth'],config['sf'],config['cr']])
  assert command(m9,'server cli advert.zerohop').startswith('OK')
  contact=None
  for _ in range(60):
   contacts=(await stock.commands.get_contacts()).payload
   if key in contacts:contact=contacts[key];break
   await asyncio.sleep(.5)
  assert contact,'Repeater advert not received by the companion'
  assert contact['type']==2 and contact['adv_name']==config['name'],contact
  passed('repeater advert: type 2, name, full key',name=contact['adv_name'])
  assert (await stock.commands.send_advert(flood=False)).type==EventType.OK
  command(m9,'server cli setperm '+info.payload['public_key']+' 0') # a fresh start: drop this companion from the ACL ("Err" when absent)
  assert read('server')['admins']==0,'Another admin is logged in'
  # A wrong password gets no reply, and no client is added.
  login=await stock.commands.send_login_sync(contact,'wrong-'+secrets['password'][:4],min_timeout=8)
  assert login is None or login.type!=EventType.LOGIN_SUCCESS,'Wrong password logged in'
  assert read('server')['admins']==0
  passed('wrong password refused')
  if not secrets['guest_password']:
   login=await stock.commands.send_login_sync(contact,'',min_timeout=10)
   assert login and login.type==EventType.LOGIN_SUCCESS,'Guest (blank) login failed'
   assert not login.payload.get('is_admin'),login.payload
   passed('blank password: guest login',permissions=login.payload.get('permissions'))
  login,n=await retry('admin login',lambda:stock.commands.send_login_sync(contact,secrets['password'],min_timeout=10))
  assert login.type==EventType.LOGIN_SUCCESS,'Admin login failed'
  passed('admin login',attempts=n,permissions=login.payload.get('permissions'),is_admin=login.payload.get('is_admin'))
  rpt=read('server');assert rpt['admins']>=1,rpt
  st,n=await retry('status',lambda:stock.commands.req_status_sync(contact,min_timeout=10))
  assert st.get('nb_recv',0)>0 and st.get('uptime',0)>0,st
  passed('status request',attempts=n,uptime=st.get('uptime'),recv=st.get('nb_recv'),sent=st.get('nb_sent'),bat=st.get('bat'))
  tel,n=await retry('telemetry',lambda:stock.commands.req_telemetry_sync(contact,min_timeout=10))
  items=tel if isinstance(tel,list) else tel.get('lpp') or [];assert any(x.get('type')=='voltage' for x in items),tel
  passed('telemetry request',attempts=n,fields=[(x.get('type'),x.get('value')) for x in items])
  nb,n=await retry('neighbours',lambda:stock.commands.req_neighbours_sync(contact,min_timeout=10))
  passed('neighbours request',attempts=n,total=nb.get('neighbours_count'),results=nb.get('results_count'))
  acl,n=await retry('ACL',lambda:stock.commands.req_acl_sync(contact,min_timeout=10))
  me=info.payload['public_key'][:12]
  assert any(e.get('key','')[:12]==me and (e.get('perm',0)&3)==3 for e in acl),acl
  passed('access list: companion is admin',attempts=n,entries=len(acl))
  reply=await cli(stock,contact,'get name');assert reply=='> '+config['name'],reply
  passed('remote CLI: get name',reply=reply)
  before=read('server')['advert_minutes']
  reply=await cli(stock,contact,'set advert.interval 180');assert reply.endswith('OK'),reply
  assert read('server')['advert_minutes']==180
  reply=await cli(stock,contact,f'set advert.interval {before}');assert reply.endswith('OK'),reply
  assert read('server')['advert_minutes']==before
  passed('remote CLI: setting changed, seen over USB, restored',restored=before)
  reply=await cli(stock,contact,'set tx 30');assert reply.startswith('Error'),reply
  assert read('config')['power']==config['power']
  passed('remote CLI: out-of-range power refused',reply=reply[:40])
  await stock.commands.send_logout(contact)
  after=read('status');assert after['boot']==status['boot'],'M9 restarted during the check'
  passed('no M9 restart',boot=after['boot'])
 finally:
  await stock.disconnect();m9.close()
 out=ROOT/'artifacts/repeater-check.json'
 out.write_text(json.dumps({'m9_build_sha256':status['build_sha256'],'m9_boot':status['boot'],'companion':'MeshCore companion_radio_usb v1.17.1','checks':checks},ensure_ascii=False,indent=2))
 print('PASS',len(checks),'checks ->',out)

if __name__=='__main__':asyncio.run(main())
