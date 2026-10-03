#!/usr/bin/env python3
"""MeshCore channels between MeshMesh (M9) and the official MeshCore companion (on the Heltec, flashed
temporarily as for tools/repeater_check.py; see docs/repeater.md for the backup and restore steps).

Over the air, in both directions: a hashtag channel joined on each side by its name, a private channel
created on MeshMesh and given to the companion by the key from its link, an invitation sent as a direct
message, a channel MeshMesh has not joined seen on air and opened by probing its hashtag, and leaving a
channel. Uses fresh channel names, so no channel other people use is written to. Requires meshcore==2.3.14.
"""
import asyncio,json,time,hashlib,re,secrets
from pathlib import Path
from urllib.parse import urlparse,parse_qs
from meshcore import MeshCore,EventType
from device import connect,command
from ports import M9_PORT,HELTEC_PORT

async def main():
 stock=await MeshCore.create_serial(HELTEC_PORT,default_timeout=8)
 if stock is None:raise RuntimeError('Official MeshCore companion not responding')
 m9=connect(M9_PORT)
 def read(name):return json.loads(command(m9,name))
 def do(**request):return command(m9,'channel do '+json.dumps(request,ensure_ascii=False,separators=(',',':')),timeout=10)
 tag=secrets.token_hex(3);hashtag='#mmch'+tag;probe='#mmprobe'+tag;stamp=time.time_ns();checks=[]
 async def stock_gets(index,text,limit=60):
  for _ in range(limit):
   msg=await stock.commands.get_msg()
   if msg.type==EventType.CHANNEL_MSG_RECV and msg.payload.get('channel_idx')==index and msg.payload.get('text')==text:return msg.payload
   await asyncio.sleep(.3)
  raise AssertionError(f'companion did not get {text!r} on channel {index}')
 def m9_gets(channel,text,limit=60):
  for _ in range(limit):
   rows=[m for m in read('messages') if not m['outgoing'] and m['destination']==channel and m['text']==text]
   if rows:return rows
   time.sleep(.3)
  raise AssertionError(f'M9 did not get {text!r} in {channel}')
 try:
  before=read('status');assert before['protocol']=='MeshCore' and before['role']=='normal'
  info=await stock.commands.send_appstart();assert info.type==EventType.SELF_INFO
  device=await stock.commands.send_device_query();stock_key=info.payload['public_key'];stock_node=stock_key[:16].upper()
  # Contacts both ways, for the invitation.
  assert command(m9,'hello').startswith('OK');assert (await stock.commands.send_advert(flood=True)).type==EventType.OK
  for _ in range(25):
   contacts=(await stock.commands.get_contacts()).payload
   if before['public_key'].lower() in contacts and any(p['public_key'].lower()==stock_key for p in read('nodes')):break
   await asyncio.sleep(.5)
  else:raise AssertionError('advertisements did not make contacts on both nodes')
  # 1. Hashtag channel: the same name gives the same key on both firmwares.
  reply=do(action='add',hashtag=hashtag.upper().replace('#','# '));assert reply.startswith('OK channel added '),reply
  tag_id=reply.split()[-1];listed=[c for c in read('channels')['channels'] if c['id']==tag_id][0];assert listed['name']==hashtag and listed['kind']=='hashtag'
  assert (await stock.commands.set_channel(1,hashtag)).type==EventType.OK
  key=hashlib.sha256(hashtag.encode()).digest()[:16].hex();assert parse_qs(urlparse(listed['link']).query)['secret'][0]==key
  text=f'stock → M9 {hashtag} {stamp}';assert (await stock.commands.send_chan_msg(1,text)).type==EventType.OK
  rows=m9_gets(tag_id,text);assert len(rows)==1 and rows[0]['name']==info.payload['name']
  text=f'M9 → stock {hashtag} {stamp}';assert command(m9,f'send {tag_id} {text}').startswith('OK')
  await stock_gets(1,before['name']+': '+text)
  checks.append({'check':'hashtag channel joined by name on both sides, messages both ways','channel':hashtag,'id':tag_id,'copies_on_m9':len(rows),'passed':True})
  # 2. Private channel made on MeshMesh; the companion takes its key from the link.
  name='Проверка '+tag;reply=do(action='add',create=name);assert reply.startswith('OK channel added '),reply
  private_id=reply.split()[-1];listed=[c for c in read('channels')['channels'] if c['id']==private_id][0];assert listed['kind']=='private' and listed['name']==name
  query=parse_qs(urlparse(listed['link']).query);assert query['name'][0]==name
  assert (await stock.commands.set_channel(2,name,bytes.fromhex(query['secret'][0]))).type==EventType.OK
  text=f'stock → M9 private {stamp}';assert (await stock.commands.send_chan_msg(2,text)).type==EventType.OK;m9_gets(private_id,text)
  text=f'M9 → stock private {stamp}';assert command(m9,f'send {private_id} {text}').startswith('OK');await stock_gets(2,before['name']+': '+text)
  checks.append({'check':'private channel created on MeshMesh, key from its link, messages both ways','id':private_id,'passed':True})
  # 3. Invitation: a direct message with the link, delivered with an ACK.
  reply=do(action='invite',channel=private_id,to=stock_node);assert reply.startswith('OK'),reply
  for _ in range(60):
   msg=await stock.commands.get_msg()
   if msg.type==EventType.CONTACT_MSG_RECV and 'meshcore://channel/add?' in msg.payload.get('text',''):break
   await asyncio.sleep(.3)
  else:raise AssertionError('invitation not received')
  invite=parse_qs(urlparse(re.search(r'meshcore://\S+',msg.payload['text']).group(0)).query);assert invite['secret'][0]==query['secret'][0]
  for _ in range(60):
   out=[m for m in read('messages') if m['outgoing'] and 'meshcore://channel/add?' in m['text']]
   if out and out[-1]['status']==3:break
   time.sleep(.3)
  else:raise AssertionError('invitation not acknowledged')
  checks.append({'check':'invitation as a direct message: link with the key received, ACK','passed':True})
  # 4. A channel MeshMesh has not joined: counted by its hash, opened by probing its hashtag.
  assert (await stock.commands.set_channel(3,probe)).type==EventType.OK
  probe_hash='%02X'%hashlib.sha256(hashlib.sha256(probe.encode()).digest()[:16]).digest()[0]
  assert (await stock.commands.send_chan_msg(3,f'probe {stamp}')).type==EventType.OK
  for _ in range(50):
   heard=[h for h in read('channels')['heard'] if h['hash']==probe_hash]
   if heard:break
   time.sleep(.3)
  else:raise AssertionError('unjoined channel not counted')
  result=json.loads(do(action='probe',hashtag=probe));assert result['opened']>=1 and not result['joined'],result
  heard=[h for h in read('channels')['heard'] if h['hash']==probe_hash][0];assert heard.get('name')==probe
  checks.append({'check':'unjoined channel heard on air and opened by its hashtag','hash':probe_hash,'packets':heard['packets'],'opened':result['opened'],'passed':True})
  # 5. Leaving keeps the history; joining again shows it.
  assert do(action='remove',channel=tag_id).startswith('OK');assert tag_id not in [c['id'] for c in read('channels')['channels']]
  assert any(m['destination']==tag_id for m in read('messages'))
  assert do(action='add',hashtag=hashtag)=='OK channel added '+tag_id
  for c in (private_id,tag_id):assert do(action='remove',channel=c).startswith('OK')
  checks.append({'check':'leave and rejoin: same ID, history kept','passed':True})
  after=read('status');assert before['boot']==after['boot'] and after['rx']>before['rx'] and after['tx']>before['tx']
  report={'result':'passed','transport':'physical LoRa; no USB RF packet injection','meshmesh':before['firmware'],'meshmesh_build_sha256':before.get('build_sha256'),'stock':device.payload,'checks':checks,'boot':after['boot'],'rx':[before['rx'],after['rx']],'tx':[before['tx'],after['tx']]}
  p=Path('artifacts/channel-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
  print('PASS channels with the official MeshCore companion: hashtag and private channels both ways, invitation, heard on air, leave and rejoin',flush=True)
 finally:
  m9.close();await stock.disconnect()
if __name__=='__main__':asyncio.run(main())
