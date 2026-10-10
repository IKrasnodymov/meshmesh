#!/usr/bin/env python3
"""Temporarily test official MeshCore on V4, then restore and verify its whole flash.

Invoked by the full hardware suite after M9's final clock boot, before V4's final
installation. Requires the local companion-v1.17.1 DIO build; never edits protocol
sources. Recovery images and raw esptool logs stay private and outside Git.
"""
import argparse,hashlib,json,os,re,subprocess,sys,time
from pathlib import Path
from device import connect,command
from ports import HELTEC_PORT
from version import VERSION

ROOT=Path(__file__).resolve().parents[1]
def restore_saved(folder):
 os.chdir(ROOT);os.umask(0o077)
 backup=folder/'v4-current.bin';meta=json.loads(backup.with_suffix('.json').read_text())
 data=backup.read_bytes()
 assert meta['verified'] and meta['board']=='heltec_v4' and len(data)==16777216 and hashlib.sha256(data).hexdigest()==meta['sha256']
 original=json.loads((ROOT/'backups/heltec-v4-original-20260929.json').read_text())
 assert meta['mac']==original['mac']
 base=[sys.executable,'-m','esptool','--chip','esp32s3','--port',HELTEC_PORT,'--baud','460800']
 with (folder/'restore-retry.log').open('w') as log:
  found=subprocess.check_output(base+['--after','no-reset','read-mac'],text=True);log.write(found)
  actual=re.search(r'MAC:\s*([\da-f:]+)',found,re.I);assert actual and actual.group(1).lower()==meta['mac'].lower()
  subprocess.run(base+['--after','no-reset','write-flash','--flash-mode','dio','--flash-freq','80m','--flash-size','16MB','0x0',str(backup)],stdout=log,stderr=subprocess.STDOUT,check=True)
  verified=folder/'v4-restored-readback.bin'
  subprocess.run(base+['--after','no-reset','read-flash','0x0','0x1000000',str(verified)],stdout=log,stderr=subprocess.STDOUT,check=True)
  assert hashlib.sha256(verified.read_bytes()).hexdigest()==meta['sha256'],'Restored V4 flash differs'
  subprocess.run(base+['--after','watchdog-reset','read-mac'],stdout=log,stderr=subprocess.STDOUT,check=True)
 time.sleep(4)
 settings=json.loads((folder/'settings.json').read_text())
 with connect(HELTEC_PORT) as d:
  assert json.loads(command(d,'key',timeout=25))==settings
  after=json.loads(command(d,'status'));assert after['radio']
  manifest=json.loads((ROOT/f'artifacts/meshmesh-heltec-v4-{VERSION}/manifest.json').read_text())
  assert after['build_sha256']==meta.get('meshmesh_build_sha256',manifest['app_elf_sha256'])
 p=ROOT/'artifacts/stock-roundtrip-check.json'
 p.write_text(json.dumps({'flash_restored_byte_exact':True,'settings_restored':True,'restored_build_sha256':after['build_sha256'],'recovery_folder':str(folder.relative_to(ROOT)),'stock_tag':meta['stock_tag'],'restoration_retried_after_usb_reset':True},indent=2)+'\n');p.chmod(0o600)
 print('PASS V4 full-flash recovery, byte-exact readback and original settings')
def main():
 os.chdir(ROOT);os.umask(0o077)
 stock=ROOT/'upstream/meshcore-stock';image=stock/'.pio/build/meshmesh_stock_interop/firmware.bin'
 bootloader=image.with_name('bootloader.bin')
 partitions=image.with_name('partitions.bin')
 tag=subprocess.check_output(['git','-C',str(stock),'describe','--tags','--always'],text=True).strip()
 assert tag=='companion-v1.17.1' and all(p.is_file() for p in [image,bootloader,partitions])
 expected=(ROOT/'partitions.csv').read_text().replace('littlefs,data,spiffs,','spiffs,data,spiffs,')
 assert (stock/'meshmesh-partitions.csv').read_text()==expected,'Stock partition offsets or sizes changed'
 subprocess.run(['git','-C',str(stock),'diff','--exit-code'],check=True)
 board=json.loads((stock/'boards/meshmesh_heltec_v4.json').read_text())
 assert board['build']['flash_mode']=='dio' and board['build']['arduino']['memory_type']=='dio_qspi'
 original=ROOT/'backups/heltec-v4-original-20260929.bin';identity=json.loads(original.with_suffix('.json').read_text())
 subprocess.run([sys.executable,'tools/flash.py','--backup',str(original),'--package',f'artifacts/meshmesh-heltec-v4-{VERSION}','--check'],check=True)
 with connect(HELTEC_PORT) as d:
  before=json.loads(command(d,'status'));settings=json.loads(command(d,'key',timeout=25))
 assert before['board']=='heltec_v4' and before['radio']
 folder=ROOT/'backups'/('stock-roundtrip-'+time.strftime('%Y%m%d-%H%M%S'));folder.mkdir()
 backup=folder/'v4-current.bin';verified=folder/'v4-restored-readback.bin';log=folder/'esptool.log'
 for p in [backup,verified,log]:p.touch(mode=0o600);p.chmod(0o600)
 (folder/'settings.json').write_text(json.dumps(settings,indent=2)+'\n')
 base=[sys.executable,'-m','esptool','--chip','esp32s3','--port',HELTEC_PORT,'--baud','460800']
 def run(*args):
  with log.open('a') as output:subprocess.run([*base,*args],stdout=output,stderr=subprocess.STDOUT,check=True)
 with log.open('a') as output:
  found=subprocess.check_output([*base,'--after','no-reset','read-mac'],text=True,stderr=subprocess.STDOUT);output.write(found)
 actual=re.search(r'MAC:\s*([\da-f:]+)',found,re.I);assert actual and actual.group(1).lower()==identity['mac'].lower()
 print('Saving full V4 flash before the independent companion test',flush=True)
 run('--after','no-reset','read-flash','0x0','0x1000000',str(backup))
 data=backup.read_bytes();assert len(data)==16777216;digest=hashlib.sha256(data).hexdigest()
 backup.with_suffix('.json').write_text(json.dumps({'verified':True,'bytes':len(data),'sha256':digest,'board':'heltec_v4','mac':identity['mac'],'meshmesh_build_sha256':before['build_sha256'],'stock_tag':tag,'stock_sha256':hashlib.sha256(image.read_bytes()).hexdigest()},indent=2)+'\n')
 changed=False;restored=False
 try:
  changed=True
  print('Installing official MeshCore companion temporarily on V4',flush=True)
  # SPIFFS.begin() expects the spiffs label. Only that name differs from our
  # table; offsets and sizes are checked above. Restore the whole flash later.
  run('--after','hard-reset','write-flash','--flash-mode','dio','--flash-freq','80m','--flash-size','16MB','0x0',str(bootloader),'0x8000',str(partitions),'0x10000',str(image))
  # The companion initializes its filesystem on its first boot. Capture its
  # startup privately so radio/boot failures cannot look like host timeouts.
  with connect(HELTEC_PORT) as serial, (folder/'stock-startup.log').open('wb') as startup:
   until=time.monotonic()+45
   while time.monotonic()<until:
    startup.write(serial.read(max(1,min(4096,serial.in_waiting))))
  subprocess.run([sys.executable,'tools/meshcore_stock_check.py'],check=True)
 finally:
  if changed:
   print('Restoring the complete V4 flash and verifying every byte',flush=True)
   run('--after','no-reset','write-flash','--flash-mode','dio','--flash-freq','80m','--flash-size','16MB','0x0',str(backup))
   run('--after','no-reset','read-flash','0x0','0x1000000',str(verified))
   restored=hashlib.sha256(verified.read_bytes()).hexdigest()==digest
   run('--after','watchdog-reset','read-mac');time.sleep(4)
   if not restored:raise RuntimeError('V4 restored flash differs; private recovery backup: '+str(backup))
   with connect(HELTEC_PORT) as d:
    assert json.loads(command(d,'key',timeout=25))==settings,'V4 user settings changed'
    after=json.loads(command(d,'status'));assert after['build_sha256']==before['build_sha256'] and after['radio']
   p=ROOT/'artifacts/stock-roundtrip-check.json';p.touch(mode=0o600);p.chmod(0o600)
   p.write_text(json.dumps({'flash_restored_byte_exact':True,'settings_restored':True,'restored_build_sha256':after['build_sha256'],'recovery_folder':str(folder.relative_to(ROOT)),'stock_tag':tag},indent=2)+'\n')
 print('PASS independent MeshCore test; V4 full flash and user settings restored',flush=True)
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--restore-only',type=Path,help='Recover a verified private snapshot after an interrupted restoration')
 args=parser.parse_args()
 if args.restore_only:restore_saved(args.restore_only.resolve())
 else:main()
