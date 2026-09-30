#!/usr/bin/env python3
"""Synchronize M9 UTC, verify RTC persistence over hardware reset, reject invalid input."""
import sys,json,time,subprocess
from pathlib import Path
from device import connect,command
from ports import M9_PORT, HELTEC_PORT

def read(d):return json.loads(command(d,'clock'))
def main():
 with connect(M9_PORT) as d:
  before=read(d);assert command(d,'clock {"unix":1}').startswith('ERR')
  assert abs(read(d)['unix']-before['unix'])<5
  assert command(d,'clock '+json.dumps({'unix':int(time.time())})).startswith('OK')
  synced=read(d);assert synced['trusted'] and synced['rtc_valid'] and abs(synced['unix']-time.time())<4
 with open('logs/clock-reset.log','w') as log:
  subprocess.run([sys.executable,'-m','esptool','--chip','esp32s3','--port',M9_PORT,'--baud','115200','read-mac'],stdout=log,stderr=subprocess.STDOUT,check=True)
 time.sleep(2)
 with connect(M9_PORT) as d:
  after=read(d);state=json.loads(command(d,'status'))
  assert after['trusted'] and after['rtc_valid'] and abs(after['unix']-time.time())<5
  assert command(d,'clock '+json.dumps({'unix':int(time.time())})).startswith('OK')
 report={'checks':['invalid clock rejected','UTC synchronized from host','RTC + trusted preference retained across hardware reset','current clock within five seconds of host'],
  'boot':state['boot'],'before':before,'synced':synced,'after_reset':after}
 p=Path('artifacts/clock-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps(report,indent=2)+'\n')
 print('PASS synchronized UTC and RTC persistence across hardware reset')
if __name__=='__main__':main()
