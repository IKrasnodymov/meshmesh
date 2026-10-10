#!/usr/bin/env python3
"""Verify M9 GPS serial traffic while preserving the user's GPS preference.

Serial traffic is not evidence of a fresh position or trustworthy GNSS time.
"""
import json,os,time
from pathlib import Path
from device import connect,command
from ports import M9_PORT

def read(d,name):return json.loads(command(d,name,timeout=25))
def enable(d,value):
 for _ in range(20):
  r=command(d,'set '+json.dumps({'gps':value}),timeout=25)
  if r.startswith('OK'):return
  if r!='ERR radio busy; retry':raise RuntimeError(r)
  time.sleep(.5)
 raise TimeoutError('Radio busy while restoring GPS preference')

def main():
 os.umask(0o077)
 with connect(M9_PORT) as d:
  before=read(d,'status');config=read(d,'config');clock=read(d,'clock')
  try:
   if not config['gps']:enable(d,True)
   deadline=time.monotonic()+45
   while time.monotonic()<deadline:
    observed=read(d,'status')
    if observed['gps_bytes']>before['gps_bytes'] and observed['gps_sentences']>before['gps_sentences']:break
    time.sleep(.5)
   else:raise TimeoutError('No GPS serial sentences on M9')
  finally:
   if read(d,'config')['gps']!=config['gps']:enable(d,config['gps'])
  after=read(d,'status');assert after['boot']==before['boot']
  assert read(d,'config')==config
  now=read(d,'clock');assert now['trusted']==clock['trusted']
  assert abs(now['unix']-time.time())<5
  p=Path('artifacts/gps-serial-check.json')
  p.write_text(json.dumps({'result':'passed','serial_only':True,'fresh_fix_verified':False,'preference_restored':True,'before':before,'observed':observed,'after':after},indent=2)+'\n');p.chmod(0o600)
 print('PASS GPS UART sentences; original setting and trusted clock preserved; fresh fix not verified')
if __name__=='__main__':main()
