#!/usr/bin/env python3
"""Verify Wi-Fi CSI sensing between the boards over the air, both ways, through the production UI.

Heltec beacon -> M9 sensor, then M9 beacon -> Heltec sensor: the sensor must hear the beacon on
channel 1 at 30+ frames/s with changing CSI data and produce activity windows. Motion detection
itself needs a person moving between the boards and is not automated here.
"""
import json,time
from pathlib import Path
from contextlib import ExitStack
from device import connect,command
from ports import M9_PORT,HELTEC_PORT
from ui_navigation import compact_open,compact_home

def read(d,c):
 # Read-only queries: native USB occasionally cuts a long line; ask again.
 for attempt in range(3):
  try:return json.loads(command(d,c))
  except (TimeoutError,RuntimeError,ValueError):
   if attempt==2:raise
   time.sleep(.5)
def key(d,c):assert command(d,f'uikey {c}').startswith('OK')
def heltec_key(d,c):
 # The OLED switches off when idle; its first key only wakes it.
 if read(d,'ui')['screen_off']:key(d,13)
 key(d,c)
def heltec_action(d,index):
 """Hold PRG: one action runs at once, several open a menu where clicks select and hold runs."""
 heltec_key(d,0xa3)
 if read(d,'ui')['menu']:
  for _ in range(index):heltec_key(d,13)
  heltec_key(d,0xa3)
 time.sleep(1)
def heltec_radar(d):compact_open(d,'radar','radar')
def csi(d):return read(d,'radar')['csi']
def listened(d,what):
 first=csi(d);time.sleep(6);last=csi(d)
 assert last['heard'] and last['channel']==1 and last['rate']>=30,(what,last)
 assert not last['stale'] and last['windows']>first['windows'],(what,'CSI not changing',last)
 return {k:last[k] for k in ('rate','rssi','activity','windows','restarts','csi_frames','now_frames')}

def main():
 with ExitStack() as stack:
  m9=stack.enter_context(connect(M9_PORT));heltec=stack.enter_context(connect(HELTEC_PORT))
  before=[read(d,'status') for d in (m9,heltec)];result={}
  wifi=[s['wifi'] for s in before]
  # Each board is the sensor once, and a sensor needs the station, not the access point
  # (the Heltec UI check leaves the Heltec access point on).
  for d,on in zip((m9,heltec),wifi):
   if on:assert command(d,'wifi').startswith('OK')
  # Heltec: radar page, CSI beacon (first menu item).
  heltec_radar(heltec)
  role=csi(heltec)['role'] # a previous run may have left a role: sensor menu [calibrate, off], beacon one action
  if role!='off':heltec_action(heltec,1 if role=='sensor' else 0);assert csi(heltec)['role']=='off'
  time.sleep(3);r=read(heltec,'radar');listed=r['wifi_targets']+r['ble_targets']+r['lora_targets']>0
  heltec_action(heltec,0);assert csi(heltec)['role']=='beacon' # menu: CSI beacon, CSI sensor, ...
  # M9: Home -> Radar -> Right: Motion page, sensor role.
  if read(m9,'ui')['locked']:key(m9,0xa3)
  for k in (0x82,0x82,0xb6,0xb7,0xb7,13,0xb7):key(m9,k)
  assert read(m9,'ui')['page']=='motion'
  if csi(m9)['role']=='beacon':key(m9,ord('b')) # the page keeps the last chosen role
  assert csi(m9)['role']=='sensor'
  result['heltec_to_m9']=listened(m9,'M9 sensor')
  result['heltec_beacon_rate']=csi(heltec)['rate']
  # Swap: M9 beacon (B), Heltec sensor.
  key(m9,ord('b'));assert csi(m9)['role']=='beacon'
  heltec_action(heltec,0);assert csi(heltec)['role']=='off' # a beacon has one action: switch it off
  time.sleep(2);r=read(heltec,'radar');listed=r['wifi_targets']+r['ble_targets']+r['lora_targets']>0
  heltec_action(heltec,1);assert csi(heltec)['role']=='sensor'
  result['m9_to_heltec']=listened(heltec,'Heltec sensor')
  result['m9_beacon_rate']=csi(m9)['rate']
  # Leave: M9 home, Heltec sensor off and back to home.
  key(m9,0x86);assert read(m9,'ui')['page']=='home' and csi(m9)['role']=='off'
  if read(heltec,'ui')['menu']:time.sleep(11)
  heltec_action(heltec,1);assert csi(heltec)['role']=='off'
  compact_home(heltec)
  for d,on in zip((m9,heltec),wifi):
   if on:assert command(d,'wifi').startswith('OK')
  after=[read(d,'status') for d in (m9,heltec)]
  for a,b in zip(before,after):assert a['boot']==b['boot'] and a['diagnostic_rx']==b['diagnostic_rx'],'reboot or USB-injected frame during CSI check'
  p=Path('artifacts/csi-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600)
  p.write_text(json.dumps({'input':'simulated USB key events through the production UI; real Wi-Fi reception','not_checked':'motion detection needs a person moving between the boards','result':result,'before':before,'after':after},ensure_ascii=False,indent=2)+'\n')
  print('PASS CSI beacon/sensor both ways on channel 1 with changing channel data')
if __name__=='__main__':main()
