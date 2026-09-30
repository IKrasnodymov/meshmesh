#!/usr/bin/env python3
"""Check M9's saved history, radio/key, maps and calibration after firmware update."""
import json
from pathlib import Path
from device import connect,command
from radio_check import RADIO_FIELDS

def read(d,n):return json.loads(command(d,n))
def main():
 original=json.loads(Path('backups/before-meshcore-0.3/m9-messages.json').read_text())
 original_config=json.loads(Path('backups/before-meshcore-0.3/m9-key.json').read_text())
 with connect('/dev/cu.wchusbserial10') as d:
  state=read(d,'status');config=read(d,'key');history=read(d,'messages');nav=read(d,'navigation')
  assert state['firmware']=='MeshMesh 0.3.0' and state['board']=='m9'
  index={(m['source'],m['session'],m['id']):m for m in history};interrupted=0
  visible=[i for i,m in enumerate(original) if (m['source'],m['session'],m['id']) in index]
  assert visible==list(range(len(original)-len(visible),len(original))), 'History gap inside retained window'
  assert len(visible)==len(original) or len(history)==64, 'Missing history before ring reached capacity'
  first=json.loads(Path('artifacts/meshcore-history-first.json').read_text())
  first_index={(m['source'],m['session'],m['id']):m for m in first}
  for m in original:
   identity=(m['source'],m['session'],m['id'])
   restored=index.get(identity,first_index[identity])
   assert all(restored[k]==m[k] for k in ['source','destination','session','id','text','name','time','outgoing'])
   expected=m['status']
   if m['outgoing'] and (m['status']==1 or (m['status']==2 and m['destination'] not in ('ALL','FFFFFFFFFFFFFFFF'))):expected=4;interrupted+=1
   assert restored['status']==expected
  pending_path=Path('artifacts/restart-pending-before.json');pending_checked=False
  if pending_path.exists():
   pending=json.loads(pending_path.read_text());identity=(pending['source'],pending['session'],pending['id']);restored=index.get(identity,first_index.get(identity));assert restored is not None
   assert restored['text']==pending['text']
   if pending['status'] in (1,2):assert restored['status']==4;pending_checked=True
  for k in RADIO_FIELDS:assert abs(config[k]-original_config[k])<.0001
  assert config['key']==original_config['key'] and nav['calibrated'] and not nav['calibrating']
  areas=read(d,'map areas')
  # Remove only the catalog entry created by our one-tile compression test.
  if any(a['id']=='1a766b0e' and 'тест сжатой' in a['name'] for a in areas):
   assert command(d,'map forget 1a766b0e').startswith('OK')
  areas=read(d,'map areas');assert sorted(a['tiles'] for a in areas)==[218,1022]
  city=next(a for a in areas if a['tiles']==1022);assert command(d,'map select '+city['id']).startswith('OK')
  report={'firmware':state['firmware'],'boot':state['boot'],'original_entries_in_current_ui':len(visible),'original_entries_verified_at_first_migration':len(original),'older_entries_outside_64_message_ui':len(original)-len(visible),'history_entries':len(history),
   'history_text_and_identity_unchanged':True,'confirmed_delivery_status_preserved':True,'interrupted_sends_marked_unconfirmed':interrupted,'pending_private_send_interrupted_by_update_verified':pending_checked,
   'radio_preserved':True,'key_preserved':True,'compass_calibration_preserved':True,'areas':areas,'sd':state['sd'],'storage':state['storage']}
  for name,value in [('persistence-0.3-check',report),('m9-final-status',state),('device-config',{k:v for k,v in config.items() if k!='key'})]:
   p=Path('artifacts',name+'.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
  print(f'PASS {len(original)} history entries verified on migration, {len(visible)} in current bounded UI window; confirmed ACKs, radio/key, two SD map areas and compass calibration retained; {interrupted} interrupted send(s) now unconfirmed')
if __name__=='__main__':main()
