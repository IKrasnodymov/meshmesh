#!/usr/bin/env python3
"""Verify the signal radar with real radios through the production UI (USB key events).

M9: the Wi-Fi sweep finds the Heltec access point, homing on it counts beacons
received from its BSSID; the Bluetooth scan finds the Heltec BLE service and homing
counts its advertisements; the Wi-Fi portal then takes the radio over. The M9 BLE
service stays on throughout (coexistence). Heltec: the radar lists the M9 as a
directly heard LoRa node, and homing takes a sample from a new M9 advert.
"""
import json,time
from pathlib import Path
from contextlib import ExitStack
from device import connect,command,screenshot
from ports import M9_PORT,HELTEC_PORT

def read(d,c):return json.loads(command(d,c))
def key(d,c):assert command(d,f'uikey {c}').startswith('OK')
def wait(what,timeout,check):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  value=check()
  if value:return value
  time.sleep(.5)
 raise TimeoutError(what)
def switch(d,name,wanted):
 if read(d,'status')[name]!=wanted:assert command(d,name).startswith('OK')
 assert read(d,'status')[name]==wanted

def main():
 with ExitStack() as stack:
  m9=stack.enter_context(connect(M9_PORT));heltec=stack.enter_context(connect(HELTEC_PORT))
  before=[read(d,'status') for d in (m9,heltec)];m9_node=before[0]['node'];result={}
  m9_wifi,heltec_wifi,heltec_ble=before[0]['wifi'],before[1]['wifi'],before[1]['ble']
  switch(m9,'wifi',False);switch(heltec,'wifi',True);switch(heltec,'ble',True)
  # M9: Home -> Radar, the last tile of the second row. A sleeping screen consumes its first key.
  if read(m9,'ui')['locked']:key(m9,0xa3)
  key(m9,0x82);key(m9,0x82)
  key(m9,0xb6);key(m9,0xb7);key(m9,0xb7)
  key(m9,13);assert read(m9,'ui')['page']=='radar'
  own=lambda t:t['kind']=='wifi' and t['meshmesh']
  sweep=wait('M9 sweep did not find the Heltec access point',30,lambda:(r:=read(m9,'radar'))['wifi']=='ready' and any(own(t) for t in r['strongest']) and r)
  result['m9_sweep']={k:sweep[k] for k in ('wifi','ble','sweeps','scan_ms','wifi_targets','ble_targets','lora_targets','personal')}
  screenshot(m9,'artifacts/m9-radar.ppm')
  # Select a MeshMesh target; the list re-sorts by strength, so check the focus after OK.
  def home_in(wanted,what):
   for _ in range(70):
    selected=read(m9,'ui')['radar_selected'];strongest=read(m9,'radar')['strongest']
    if 0<=selected<len(strongest) and wanted(strongest[selected]):
     key(m9,13);state=read(m9,'radar')
     if state['tracking'] and wanted(state['focus']):return
     key(m9,0x86)
    else:key(m9,0xb6)
   raise RuntimeError(what+' could not be selected')
  home_in(own,'Heltec access point');assert read(m9,'ui')['page']=='homing'
  first=read(m9,'radar')['focus']['samples']
  focus=wait('No beacons from the Heltec BSSID',20,lambda:(f:=read(m9,'radar')['focus'])['samples']>=first+20 and f['rate']>0 and f)
  result['m9_wifi_homing']={k:focus[k] for k in ('channel','samples','rate','rssi','smoothed','peak','trend','fresh')}
  screenshot(m9,'artifacts/m9-homing.ppm')
  # Bluetooth: the Heltec BLE service advertises as "MeshMesh XXXX".
  key(m9,0x86);assert read(m9,'ui')['page']=='radar'
  own_ble=lambda t:t['kind']=='ble' and t['meshmesh']
  wait('M9 Bluetooth scan did not find the Heltec BLE service',30,lambda:(r:=read(m9,'radar'))['ble']=='ready' and any(own_ble(t) for t in r['strongest']))
  home_in(own_ble,'Heltec BLE service')
  first=read(m9,'radar')['focus']['samples']
  focus=wait('No advertisements from the Heltec BLE service',30,lambda:(f:=read(m9,'radar')['focus'])['samples']>=first+10 and f['rate']>0 and f)
  result['m9_ble_homing']={k:focus[k] for k in ('samples','rate','rssi','smoothed','peak','trend','fresh')}
  # The access point takes Wi-Fi over from the radar; leaving the radar keeps the portal up.
  assert command(m9,'wifi').startswith('OK') and read(m9,'status')['wifi']
  assert read(m9,'radar')['wifi']=='portal'
  key(m9,0x86);assert read(m9,'ui')['page']=='radar' and not read(m9,'radar')['tracking']
  key(m9,0x86);assert read(m9,'ui')['page']=='home'
  assert not read(m9,'radar')['active'] and read(m9,'status')['wifi']
  switch(m9,'wifi',m9_wifi)
  # Heltec: open the radar page, then make the M9 a directly heard LoRa node.
  for _ in range(12):
   if read(heltec,'ui')['page']=='radar':break
   key(heltec,13)
  assert read(heltec,'ui')['page']=='radar'
  assert command(m9,'hello').startswith('OK')
  mine=lambda t:t['kind']=='lora' and t.get('node')==m9_node
  heard=wait('Heltec radar did not list the M9 over LoRa',30,lambda:(r:=read(heltec,'radar'))['active'] and any(mine(t) for t in r['strongest']) and r)
  result['heltec_sweep']={k:heard[k] for k in ('wifi','sweeps','wifi_targets','lora_targets')}
  # One button: hold opens the menu; "Next signal" keeps it open, "Home in" runs homing.
  for _ in range(12):
   ui=read(heltec,'ui');strongest=read(heltec,'radar')['strongest'];selected=ui.get('radar_selected',-1)
   if not ui['menu']:key(heltec,0xa3)
   if 0<=selected<len(strongest) and mine(strongest[selected]):key(heltec,0xa3);break
   key(heltec,13);key(heltec,0xa3)
  state=read(heltec,'radar');assert state['tracking'] and mine(state['focus']),state
  first=state['focus']['samples'];time.sleep(2) # adverts carry a timestamp in seconds
  assert command(m9,'hello').startswith('OK')
  focus=wait('Heltec homing took no sample from a new M9 advert',30,lambda:(f:=read(heltec,'radar')['focus'])['samples']>first and f)
  result['heltec_homing']={k:focus[k] for k in ('rssi','samples','peak','trend','age_ms')}
  screenshot(heltec,'artifacts/heltec-homing.ppm')
  key(heltec,0xa3);assert read(heltec,'ui')['menu'];key(heltec,0xa3) # menu item 1: stop homing
  assert not read(heltec,'radar')['tracking']
  for _ in range(12):
   if read(heltec,'ui')['page']=='home':break
   key(heltec,13)
  assert read(heltec,'ui')['page']=='home' and not read(heltec,'radar')['active']
  switch(heltec,'wifi',heltec_wifi);switch(heltec,'ble',heltec_ble)
  after=[read(d,'status') for d in (m9,heltec)]
  for a,b in zip(before,after):assert a['boot']==b['boot'] and a['diagnostic_rx']==b['diagnostic_rx'],'reboot or USB-injected frame during radar check'
  p=Path('artifacts/radar-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600)
  p.write_text(json.dumps({'input':'simulated USB key events through the production UI; real Wi-Fi and LoRa reception','result':result,'before':before,'after':after},ensure_ascii=False,indent=2)+'\n')
  print('PASS M9 Wi-Fi and BLE sweeps, homing on the Heltec BSSID and BLE service, portal handover, Heltec LoRa radar and homing on a new M9 advert')
if __name__=='__main__':main()
