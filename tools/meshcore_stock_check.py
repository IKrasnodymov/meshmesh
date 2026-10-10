#!/usr/bin/env python3
"""Exercise MeshMesh M9 against independent official MeshCore USB firmware.

Only reads/configures the connected test companion; never flashes it. Requires
meshcore==2.3.14. The user-owned third device is not needed or modified.
"""
import asyncio,json,time,hashlib
from pathlib import Path
from meshcore import MeshCore,EventType
from meshcore.serial_cx import SerialConnection
import serial_asyncio_fast
from device import connect,command
from ports import M9_PORT, HELTEC_PORT

class StableUsbConnection(SerialConnection):
 async def connect(self,timeout=10):
  # Native ESP USB must have DTR/RTS set before open, as in device.py.
  # The library's default serial_for_url opens with RTS asserted first.
  self._connected_event.clear()
  serial=connect(self.port)
  try:
   transport,_=await serial_asyncio_fast.connection_for_serial(asyncio.get_running_loop(),lambda:self.MCSerialClientProtocol(self),serial)
   await asyncio.wait_for(self._connected_event.wait(),timeout)
  except BaseException:
   serial.close();raise
  return self.port

async def main():
 deadline=time.monotonic()+150
 while True:
  stock=MeshCore(StableUsbConnection(HELTEC_PORT,115200),default_timeout=8)
  if await stock.connect() is not None:break
  await stock.disconnect()
  if time.monotonic()>=deadline:raise RuntimeError('Independent MeshCore companion not responding after startup timeout')
  await asyncio.sleep(5)
 m9=connect(M9_PORT)
 def read(name):return json.loads(command(m9,name))
 events=[]
 stock.subscribe(EventType.ACK,lambda event:events.append(event.payload))
 try:
  before=read('status');assert before['protocol']=='MeshCore'
  info=await stock.commands.send_appstart();assert info.type==EventType.SELF_INFO
  key=info.payload['public_key'];node=key[:16].upper();mine=before['public_key'].lower()
  device=await stock.commands.send_device_query();assert device.payload['ver']=='v1.17.1'
  config=read('config')
  assert (await stock.commands.set_radio(round(config['frequency'],3),config['bandwidth'],config['sf'],config['cr'])).type==EventType.OK
  rtc=await stock.commands.get_time();assert rtc.type==EventType.CURRENT_TIME
  assert rtc.payload['time']<=int(time.time())+60,'Stock clock is unexpectedly far ahead'
  # Stock rejects backward corrections; its RTC can be slightly ahead of the host.
  assert (await stock.commands.set_time(max(int(time.time())+2,rtc.payload['time']+2))).type==EventType.OK
  assert command(m9,'hello').startswith('OK');assert (await stock.commands.send_advert(flood=True)).type==EventType.OK
  contacts={}
  for _ in range(25):
   contacts=(await stock.commands.get_contacts()).payload
   nodes=read('nodes')
   if mine in contacts and any(p['public_key'].lower()==key for p in nodes):break
   await asyncio.sleep(.5)
  else:raise AssertionError('Advertisements did not produce contacts on both nodes')
  checks=[{'check':'signed advertisements and full public-key contacts in both directions','passed':True}]
  stamp=time.time_ns()
  for i in range(2):
   text=f'Стандарт MeshCore → M9 / {i}: {stamp}'
   sent=await stock.commands.send_msg_with_retry(contacts[mine],text,max_attempts=3,max_flood_attempts=3,timeout=12,min_timeout=12);assert sent is not None and sent.type==EventType.MSG_SENT
   expected=sent.payload['expected_ack'].hex()
   for _ in range(60):
    incoming=[m for m in read('messages') if not m['outgoing'] and m['source']==node and m['text']==text]
    if len(incoming)==1 and any(e['code']==expected for e in events):break
    await asyncio.sleep(.3)
   else:raise AssertionError('Stock → M9 delivery / ACK missing')
   checks.append({'check':'stock → M9 direct message','copies':len(incoming),'ack':True,'text':text})
   text=f'M9 → стандарт MeshCore / {i}: {stamp}'
   assert command(m9,f'send {node} {text}').startswith('OK')
   received=[]
   for _ in range(60):
    msg=await stock.commands.get_msg()
    if msg.type==EventType.CONTACT_MSG_RECV and msg.payload.get('text')==text:received.append(msg.payload)
    outgoing=[m for m in read('messages') if m['outgoing'] and m['text']==text]
    if received and outgoing and outgoing[-1]['status']==3:break
    await asyncio.sleep(.3)
   else:raise AssertionError('M9 → stock delivery / ACK missing')
   assert len(received)==1 and received[0]['pubkey_prefix']==mine[:12]
   checks.append({'check':'M9 → stock direct message','copies':len(received),'ack':True,'text':text})
  # Public channel messages are intentionally not reported as delivered.
  public=await stock.commands.get_channel(0)
  import base64
  assert (await stock.commands.set_channel(0,'Public',base64.b64decode('izOH6cXN6mrJ5e26oRXNcg=='))).type==EventType.OK
  text=f'Public stock → M9 {stamp}'
  assert (await stock.commands.send_chan_msg(0,text)).type==EventType.OK
  for _ in range(50):
   rows=[m for m in read('messages') if not m['outgoing'] and m['destination']=='ALL' and m['text']==text]
   if len(rows)==1:break
   await asyncio.sleep(.3)
  else:raise AssertionError('Stock Public message missing')
  text=f'Public M9 → stock {stamp}';assert command(m9,'send ALL '+text).startswith('OK')
  for _ in range(50):
   msg=await stock.commands.get_msg()
   if msg.type==EventType.CHANNEL_MSG_RECV and msg.payload.get('text')==before['name']+': '+text:break
   await asyncio.sleep(.3)
  else:raise AssertionError('M9 Public message missing in stock companion')
  checks.append({'check':'Public channel both directions','ack_claimed':False,'passed':True})
  after=read('status');assert before['boot']==after['boot'] and before['diagnostic_rx']==after['diagnostic_rx']
  assert after['rx']>before['rx'] and after['tx']>before['tx']
  report={'result':'passed','transport':'physical LoRa; no USB RF packet injection','meshmesh':before['firmware'],'meshmesh_build_sha256':before.get('build_sha256'),'stock':device.payload,'self':info.payload,'checks':checks,'before':before,'after':after,'stock_build':'official companion-v1.17.1 with MeshMesh board JSON, DIO, matching bootloader and identical partition offsets/sizes with the SPIFFS label; protocol sources unchanged'}
  p=Path('artifacts/meshcore-stock-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
  print('PASS independent official MeshCore: advertisements, contacts, repeated direct messages/ACKs and Public in both directions',flush=True)
 finally:
  m9.close();await stock.disconnect()
if __name__=='__main__':asyncio.run(main())
