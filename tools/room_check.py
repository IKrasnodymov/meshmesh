#!/usr/bin/env python3
"""Exercise the MeshMesh M9 room server role from independent official MeshCore companion firmware.

The M9 must run the room role (`role room`); the Heltec runs official MeshCore companion_radio_usb
(temporary, see docs/repeater.md). Over LoRa: the room advert, a refused wrong password, login with
the room password, a post by the companion (ACK, stored with its author), a post by the room pushed
to the companion, admin login with status and remote CLI, read-only login when allowed. The room password is set for the check and restored. Requires
meshcore==2.3.14; passwords are not printed.
"""
import asyncio,json,secrets,time
from pathlib import Path
from meshcore import MeshCore,EventType
from device import connect,command
from ports import M9_PORT,HELTEC_PORT
from repeater_check import cli,retry

ROOT=Path(__file__).resolve().parents[1]

async def main():
 m9=connect(M9_PORT)
 def read(name):return json.loads(command(m9,name))
 stock=await MeshCore.create_serial(HELTEC_PORT,default_timeout=8)
 if stock is None:raise RuntimeError('Official MeshCore companion not responding on the Heltec')
 checks=[];stamp=time.strftime('%H%M%S')
 def passed(check,**extra):checks.append({'check':check,'passed':True,**extra});print('PASS',check,extra or '')
 received=[]
 stock.subscribe(EventType.CONTACT_MSG_RECV,lambda e:received.append(e.payload))
 try:
  assert read('role')['role']=='room','M9 is not in the room role'
  status=read('status');key=status['public_key'].lower();config=read('config');before=read('server secrets')
  info=await stock.commands.send_appstart();assert info.type==EventType.SELF_INFO
  assert (await stock.commands.set_time(int(time.time()))).type==EventType.OK # as the MeshCore app does; logins carry the companion's clock
  me=info.payload['public_key']
  command(m9,'server cli setperm '+me+' 0') # a fresh start: drop this companion from the ACL
  room_password='r'+secrets.token_hex(4)
  assert command(m9,'server cli set guest.password '+room_password).startswith('OK')
  assert command(m9,'server cli set allow.read.only off').startswith('OK')
  assert command(m9,'server cli advert.zerohop').startswith('OK')
  contact=None
  for _ in range(60):
   contacts=(await stock.commands.get_contacts()).payload
   if key in contacts:contact=contacts[key];break
   await asyncio.sleep(.5)
  assert contact and contact['type']==3 and contact['adv_name']==config['name'],contact
  passed('room advert: type 3, name, full key',name=contact['adv_name'])
  assert (await stock.commands.send_advert(flood=False)).type==EventType.OK
  login=await stock.commands.send_login_sync(contact,'wrong-'+room_password[:3],min_timeout=8)
  assert login is None or login.type!=EventType.LOGIN_SUCCESS,'Wrong password logged in'
  passed('wrong password refused (read-only login off)')
  login=await stock.commands.send_login_sync(contact,room_password,min_timeout=10)
  assert login and login.type==EventType.LOGIN_SUCCESS,'Room password login failed'
  assert not login.payload.get('is_admin'),login.payload
  passed('room password login: member',permissions=login.payload.get('permissions'))
  # A post by the companion: ACKed by the room and stored with its author.
  text=f'Пост участника {stamp}'
  sent,n=await retry('post ACK',lambda:stock.commands.send_msg_with_retry(contact,text,max_attempts=3,max_flood_attempts=3,timeout=12,min_timeout=12),tries=2)
  assert sent.type==EventType.MSG_SENT
  posts=read('server')['posts']
  mine=[p for p in posts if p['text']==text];assert len(mine)==1 and mine[0]['author']==me[:8].upper() and not mine[0]['own'],posts[:2]
  passed('member post: ACK, stored once with its author',attempts=n)
  # A post by the room reaches the logged-in member.
  text=f'Пост комнаты {stamp}'
  assert command(m9,'server post '+text).startswith('OK')
  for _ in range(90):
   if any(m.get('text')==text for m in received):break
   await stock.commands.get_msg(timeout=1) # fetched messages reach the subscription
   await asyncio.sleep(.3)
  got=[m for m in received if m.get('text')==text];assert len(got)>=1,'Room post not pushed'
  for _ in range(30):
   if read('server')['pushed']>=1:break
   await asyncio.sleep(.5)
  passed('room post pushed to the member',copies=len(got),txt_type=got[0].get('txt_type'),pushed=read('server')['pushed'])
  login,n=await retry('admin login',lambda:stock.commands.send_login_sync(contact,before['password'],min_timeout=10))
  assert login.type==EventType.LOGIN_SUCCESS and login.payload.get('is_admin'),'Admin login failed'
  passed('admin login',attempts=n,permissions=login.payload.get('permissions'))
  st,n=await retry('status',lambda:stock.commands.req_status_sync(contact,min_timeout=10))
  passed('status request',attempts=n,uptime=st.get('uptime'),recv=st.get('nb_recv'))
  reply=await cli(stock,contact,'get name');assert reply=='> '+config['name'],reply
  passed('remote CLI: get name',reply=reply)
  reply=await cli(stock,contact,'set allow.read.only on');assert reply.endswith('OK'),reply
  command(m9,'server cli setperm '+me+' 0')
  login=await stock.commands.send_login_sync(contact,'any-'+stamp,min_timeout=10)
  assert login and login.type==EventType.LOGIN_SUCCESS and login.payload.get('permissions')==2,'Read-only login failed' # the room's login reply byte: 2 = read-only guest
  passed('read-only login with any password when allowed',permissions=login.payload.get('permissions'))
  await stock.commands.send_logout(contact)
 finally:
  command(m9,'server cli set allow.read.only off');command(m9,'server cli set guest.password '+before['guest_password'])
  await stock.disconnect()
 count=len(read('server')['posts']);assert read('status')['boot']==status['boot'],'M9 restarted during the check'
 m9.close()
 out=ROOT/'artifacts/room-check.json'
 out.write_text(json.dumps({'m9_build_sha256':status['build_sha256'],'m9_boot':status['boot'],'posts':count,'companion':'MeshCore companion_radio_usb v1.17.1','checks':checks},ensure_ascii=False,indent=2))
 print('PASS',len(checks),'checks ->',out)

if __name__=='__main__':asyncio.run(main())
