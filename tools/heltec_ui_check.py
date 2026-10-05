#!/usr/bin/env python3
"""Verify the production ten-page OLED UI, its one-button menus and a physical-radio quick reply."""
import json,time
from pathlib import Path
from contextlib import ExitStack
from device import connect,command,screenshot
from radio_check import delivery
from ports import M9_PORT, HELTEC_PORT

CHESS_LIST=7 # "Game list" in the game menu: ChessAct in src/UiChessCompact.inc

def read(d,c):return json.loads(command(d,c))
def key(d,c):
 # The panel switches off after the configured idle time; its first key only wakes it.
 if json.loads(command(d,'ui')).get('screen_off'):assert command(d,'uikey 13').startswith('OK')
 assert command(d,f'uikey {c}').startswith('OK')
 time.sleep(.2)

def main():
 with ExitStack() as stack:
  m9=stack.enter_context(connect(M9_PORT));heltec=stack.enter_context(connect(HELTEC_PORT))
  before=[read(d,'status') for d in (m9,heltec)]
  # Put a known incoming personal message last, so hold replies to the M9.
  delivery(m9,heltec,before[0]['node'],before[1]['node'],f'Проверка ответа кнопкой Heltec {time.time_ns()}')
  # First key after screen-off only wakes the panel; a click closes the new-message popup.
  if read(heltec,'ui')['screen_off']:key(heltec,13)
  if read(heltec,'ui')['popup']:key(heltec,13)
  for _ in range(10):
   if read(heltec,'ui')['page']=='home':break
   key(heltec,13)
  snapshots=[]
  for name in ('home','messages','nodes','chess','radar','gps','wifi','ble','settings','modules'):
   ui=read(heltec,'ui');assert ui['page']==name and not ui['menu']
   if name=='chess' and 'chess_game' in ui:
    # An open board keeps the click for its choices: its menu goes back to the list.
    key(heltec,0xa3)
    for _ in range(8):
     if read(heltec,'ui').get('chess_act')==CHESS_LIST:break
     key(heltec,13)
    key(heltec,0xa3);assert 'chess_game' not in read(heltec,'ui')
   if name not in ('wifi','ble'):
    screenshot(heltec,f'artifacts/heltec-{name}-0.3.ppm');snapshots.append(name)
   if name=='messages':
    assert read(heltec,'ui')['unread']==0
    known={(m['source'],m['session'],m['id']) for m in read(m9,'messages')}
    key(heltec,0xa3);ui=read(heltec,'ui');assert ui['menu'] and ui['menu_index']==0 # several actions: hold opens the menu
    screenshot(heltec,'artifacts/heltec-menu-0.3.ppm');snapshots.append('menu')
    key(heltec,0xa3);ui=read(heltec,'ui');assert 'OK' in ui['action'] and not ui['menu'] # hold on "Reply: OK"
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
     outgoing=[m for m in read(heltec,'messages') if m['outgoing'] and m['text']=='OK' and m['session']==before[1]['boot']]
     if outgoing and outgoing[-1]['status']==3:
      sent=outgoing[-1];incoming=[m for m in read(m9,'messages') if not m['outgoing'] and m['source']==before[1]['node'] and m['text']=='OK' and (m['source'],m['session'],m['id']) not in known]
      assert len(incoming)==1 and incoming[0]['text']=='OK' and incoming[0]['destination']==before[0]['node'];break
     time.sleep(.25)
    else:raise TimeoutError('Heltec menu OK reply not delivered over physical LoRa')
   if name=='settings':
    # Walk the whole menu (language, battery, screen, contrast, mode) to "Close" without changing a setting.
    key(heltec,0xa3);assert read(heltec,'ui')['menu']
    for _ in range(5):key(heltec,13)
    key(heltec,0xa3);assert not read(heltec,'ui')['menu']
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
  p=Path('artifacts/heltec-ui-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps({'input':'simulated USB key events through production OLED UI; not physical-button automation','checks':['ten pages','hold opens menu; menu reply: physical LoRa delivery + ACK','settings menu closes unchanged','Wi-Fi/BLE controls','encryption action feedback'],'snapshots':snapshots,'before':before,'after':after},ensure_ascii=False,indent=2)+'\n')
  print('PASS ten OLED pages, menu quick reply with physical LoRa ACK, Wi-Fi/BLE controls and encryption feedback')
if __name__=='__main__':main()
