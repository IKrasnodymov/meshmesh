#!/usr/bin/env python3
"""Verify the physical Wi-Fi portal and map upload using a second MeshMesh board."""
import argparse,json,time
from pathlib import Path
from contextlib import ExitStack
from device import connect,command
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--server',default='/dev/cu.wchusbserial10');parser.add_argument('--client',default='/dev/cu.usbmodem1101');parser.add_argument('--output',type=Path,default=Path('artifacts/wifi-map-check.json'));args=parser.parse_args()
with ExitStack() as stack:
 server=stack.enter_context(connect(args.server));client=stack.enter_context(connect(args.client))
 before=[json.loads(command(d,'status')) for d in [server,client]]
 if not before[0]['wifi']:raise RuntimeError('Enable M9 Wi-Fi first')
 credentials=json.loads(command(server,'connections'))
 reply=command(client,'wifiprobe '+json.dumps({k:credentials[k] for k in ['ssid','password']}))
 if not reply.startswith('OK'):raise RuntimeError(reply)
 deadline=time.monotonic()+120
 while time.monotonic()<deadline:
  result=json.loads(command(client,'wifiprobe',timeout=35))
  if result['done']:break
  if not result['running']:
   state=json.loads(command(client,'status'))
   failed=args.output.with_name(args.output.stem+'-failed.json')
   failed.touch(mode=0o600,exist_ok=True);failed.chmod(0o600)
   failed.write_text(json.dumps({'result':result,'before':before,'client_after':state},ensure_ascii=False,indent=2)+'\n')
   raise RuntimeError('Wi-Fi client rebooted during probe' if state['boot']!=before[1]['boot'] else 'Wi-Fi probe stopped without a result')
  time.sleep(.5)
 else:raise TimeoutError('Physical Wi-Fi probe timed out')
 after=[json.loads(command(d,'status')) for d in [server,client]]
 report={'transport':'physical MeshMesh Wi-Fi client → MeshMesh access point; HTTP authentication; map upload if server has SD','result':result,'before':before,'after':after}
 p=args.output;p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 assert not result['error'],result
 assert all(result[k] for k in ['homepage','auth_rejected','apis']),result
 assert result['maps_supported']==before[0]['sd']
 if result['maps_supported']:assert result['binary_chunk'] and result['tile_readback'],result
 assert [s['boot'] for s in before]==[s['boot'] for s in after]
 print('PASS physical Wi-Fi page and authenticated APIs'+(' and exact map tile upload/readback' if result['maps_supported'] else '; no SD on this server'))
