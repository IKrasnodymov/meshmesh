#!/usr/bin/env python3
"""Verify the production six-page OLED UI and its physical-radio quick reply."""
import json,time
from pathlib import Path
from contextlib import ExitStack
from device import connect,command,screenshot
from radio_check import delivery

def read(d,c):return json.loads(command(d,c))
def key(d,c):
 assert command(d,f'uikey {c}').startswith('OK')
 time.sleep(.2)

def main():
 with ExitStack() as stack:
  m9=stack.enter_context(connect('/dev/cu.wchusbserial10'));heltec=stack.enter_context(connect('/dev/cu.usbmodem1101'))
  before=[read(d,'status') for d in (m9,heltec)]
  # Put a known incoming personal message last, so hold replies to the M9.
  delivery(m9,heltec,before[0]['node'],before[1]['node'],f'Проверка ответа кнопкой Heltec {time.time_ns()}')
  for _ in range(6):
   if read(heltec,'ui')['page']=='home':break
   key(heltec,13)
  snapshots=[]
  for index,name in enumerate(('home','messages','nodes','wifi','ble','modules')):
   assert read(heltec,'ui')['page']==name
   if name not in ('wifi','ble'):
    screenshot(heltec,f'artifacts/heltec-{name}-0.3.ppm');snapshots.append(name)
   if name=='messages':
    known={(m['source'],m['session'],m['id']) for m in read(m9,'messages')}
    key(heltec,0xa3);assert 'OK' in read(heltec,'ui')['action']
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
     outgoing=[m for m in read(heltec,'messages') if m['outgoing'] and m['text']=='OK' and m['session']==before[1]['boot']]
     if outgoing and outgoing[-1]['status']==3:
      sent=outgoing[-1];incoming=[m for m in read(m9,'messages') if not m['outgoing'] and m['source']==before[1]['node'] and m['text']=='OK' and (m['source'],m['session'],m['id']) not in known]
      assert len(incoming)==1 and incoming[0]['text']=='OK' and incoming[0]['destination']==before[0]['node'];break
     time.sleep(.25)
    else:raise TimeoutError('Heltec hold-OK reply not delivered over physical LoRa')
   if name in ('wifi','ble'):
    flag=name
    if not read(heltec,'status')[flag]:key(heltec,0xa3)
    assert read(heltec,'status')[flag]
   if name=='modules':
    key(heltec,0xa3);assert 'OK' in read(heltec,'ui')['action'];screenshot(heltec,'artifacts/heltec-crypto-0.3.ppm')
   key(heltec,13)
  assert read(heltec,'ui')['page']=='home'
  after=[read(d,'status') for d in (m9,heltec)]
  for a,b in zip(before,after):assert a['boot']==b['boot'] and a['diagnostic_rx']==b['diagnostic_rx']
  p=Path('artifacts/heltec-ui-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps({'input':'simulated USB key events through production OLED UI; not physical-button automation','checks':['six pages','hold quick reply: physical LoRa delivery + ACK','Wi-Fi/BLE controls','encryption action feedback'],'snapshots':snapshots,'before':before,'after':after},ensure_ascii=False,indent=2)+'\n')
  print('PASS six OLED pages, hold quick reply with physical LoRa ACK, Wi-Fi/BLE controls and encryption feedback')
if __name__=='__main__':main()
