#!/usr/bin/env python3
"""Collect the final hardware release evidence without exporting credentials."""
import hashlib,json,time
from pathlib import Path
from contextlib import ExitStack
from device import connect,command
from radio_check import RADIO_FIELDS
from version import VERSION,FIRMWARE
from ports import M9_PORT, HELTEC_PORT

def read(d,name):return json.loads(command(d,name))
def load(name):return json.loads(Path('artifacts',name+'.json').read_text())
def main():
 receipt=load('hardware-finish')
 required=['flash-m9','clock','persistence','map-ui','gps-serial','meshcore-stock','flash-heltec','radio','m9-ui','concurrency','heltec-ui','radar','csi','m9-wifi','m9-ble','heltec-wifi','heltec-ble']
 assert all(step in receipt['completed'] for step in required),'Run finish_on_hardware.py to install and verify the final packages'
 for board,folder in [('m9',f'meshmesh-m9-{VERSION}'),('heltec_v4',f'meshmesh-heltec-v4-{VERSION}')]:
  manifest=Path('artifacts',folder,'manifest.json')
  assert hashlib.sha256(manifest.read_bytes()).hexdigest()==receipt['package_manifest_sha256'][board],'Package changed after hardware suite'
 with ExitStack() as stack:
  devices=[stack.enter_context(connect(p)) for p in [M9_PORT,HELTEC_PORT]]
  status=[read(d,'status') for d in devices];config=[read(d,'key') for d in devices]
  # The network's radio profile is fixed; power and hops are each node's own (user) choice and
  # must only survive the update (M9: the config saved before installation).
  expected=dict(frequency=868.731,bandwidth=62.5,sf=8,cr=6)
  for s,c in zip(status,config):
   assert s['firmware']==FIRMWARE and s['radio'] and s['radio_error']==0 and s['storage'] and s['psram']>0
   for k,v in expected.items():assert abs(c[k]-v)<.0001,(k,c[k])
  before=Path('artifacts/m9-config-before-finish.json')
  if before.exists():
   for k,v in json.loads(before.read_text()).items():assert config[0][k]==v,('M9 setting changed by the update',k)
  for board,s in zip(['m9','heltec_v4'],status):assert s['build_sha256']==json.loads(Path('artifacts','meshmesh-'+('m9' if board=='m9' else 'heltec-v4')+'-'+VERSION,'manifest.json').read_text())['app_elf_sha256']
  assert [s['board'] for s in status]==['m9','heltec_v4']
  for board,c in zip(['m9','heltec_v4'],config):assert c['key']==json.loads(Path('backups/before-meshcore-0.3',board+'-key.json').read_text())['key']
  assert all(s['protocol']=='MeshCore' and len(s['public_key'])==64 for s in status)
  prior=load('meshcore-pair-check')['before']
  assert all(new['public_key']==old['public_key'] and new['node']==old['node'] for new,old in zip(status,prior)), 'MeshCore identity changed across reset/installation'
  assert all(status[0][k] for k in ['keyboard','sd','rtc_valid','compass_sample','imu_sample'])
  assert status[0]['gps_sentences']>0 and status[0]['gps_bytes']>0
  assert all(s['wifi'] and s['ble'] for s in status)
  areas=read(devices[0],'map areas');nav=read(devices[0],'navigation')
  assert sorted(a['tiles'] for a in areas)==[218,1022] and nav['calibrated'] and not nav['calibrating']
  current={s['node']:s['boot'] for s in status};reports={}
  names=['radio-check','ble-radio-check','heltec-ble-radio-check','ui-radio-check','concurrency-check','wifi-map-check','heltec-wifi-check','heltec-ui-check','radar-check','csi-check','chess-check']
  for name in names:
   data=load(name)
   for entry in data['before']+data['after']:assert current[entry['node']]==entry['boot'],name+' belongs to an earlier firmware boot'
   if name in ['wifi-map-check','heltec-wifi-check']:
    result=data['result'];assert not result['error'] and all(result[k] for k in ['homepage','auth_rejected','apis'])
    assert result['maps_supported']==data['before'][0]['sd']
    if result['maps_supported']:assert result['binary_chunk'] and result['tile_readback']
   if name in ['ble-radio-check','heltec-ble-radio-check']:
    assert not data['ble']['error'] and all(data['ble'][k] for k in ['encrypted','authenticated','selftest','status','config','messages','sent'])
    assert data['delivery']=='DELIVERED'
   reports[name]='PASS'
  maps=load('map-ui-check');assert maps['after'][0]['boot']==status[0]['boot'];reports['map-ui-check']='PASS'
  gps=load('gps-serial-check');assert gps['result']=='passed' and gps['preference_restored'] and gps['after']['boot']==status[0]['boot'];reports['gps-serial-check']='PASS'
  persistence=load('persistence-0.3-check');assert persistence['boot']==status[0]['boot'];reports['persistence-0.3-check']='PASS'
  interoperability=load('meshcore-stock-check');assert interoperability['result']=='passed' and interoperability['meshmesh_build_sha256']==status[0]['build_sha256'] and interoperability['after']['boot']==status[0]['boot'];reports['meshcore-stock-check']='PASS'
  clock=load('clock-check');assert clock['boot']==status[0]['boot'];reports['clock-check']='PASS'
  clocks=[read(d,'clock') for d in devices]
  assert clocks[0]['trusted'] and clocks[0]['rtc_valid']
  for s,c in zip(status,clocks):
   assert abs(c['unix']-time.time())<5,'Clock is not synchronized'
   if s['clock_conflict']:assert not s['gps_fix'],'Conflicting GNSS date must not be used as a current fix'
  for d in devices:assert command(d,'selftest').startswith('OK crypto/UTF-8/tamper selftest')
  report={'firmware':FIRMWARE,'result':'passed_hardware_suite','package_manifest_sha256':receipt['package_manifest_sha256'],'devices':['ThinkNode M9','Heltec V4'],'boot':[s['boot'] for s in status],
   'radio':{k:config[0][k] for k in RADIO_FIELDS},'radio_heltec':{k:config[1][k] for k in RADIO_FIELDS},'checks':reports,'legacy_keys_preserved':True,'radio_protocol':'MeshCore',
   'maps':areas,'compass_calibration_preserved':True,'status':status,'clock':clocks,'test_notes':receipt.get('notes',[]),
   'web_codec':'Python/browser exact roundtrip, malformed packages rejected; 70379-feature city index checked',
   'limitations':['New web layout not visually tested in a browser; physical Wi-Fi/HTTP/API tested on both boards',
    'Compass axis orientation and outdoor accuracy not measured; magnetic heading requires a flat device',
    'Fresh outdoor GPS fix/date not verified; receiver previously reported 2026-07-08 indoors',
    'Outdoor LoRa range and third-node relay not tested','macOS BLE with old MeshCore bond not retested',
    'Street maps only: satellite imagery, address search, route planning, voice and OTA updater absent']}
  p=Path('artifacts/release-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
  print('PASS final installed firmware, matching radio/key, maps/calibration, module health and current-boot evidence on both devices')
if __name__=='__main__':main()
