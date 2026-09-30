#!/usr/bin/env python3
"""Install MeshMesh or restore the verified full backup on the matching device."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
from version import VERSION
from ports import M9_PORT, HELTEC_PORT

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',default=M9_PORT)
    p.add_argument('--backup',type=Path,default=ROOT/'backups/m9-original-20260929.bin')
    p.add_argument('--package',type=Path,default=ROOT/f'artifacts/meshmesh-m9-{VERSION}')
    p.add_argument('--restore',action='store_true')
    p.add_argument('--check',action='store_true',help='Validate backup and package files without accessing USB');a=p.parse_args()
    info=json.loads(a.backup.with_suffix('.json').read_text());data=a.backup.read_bytes()
    if not info.get('verified') or len(data)!=16777216 or hashlib.sha256(data).hexdigest()!=info['sha256']:
        raise SystemExit('Full original backup is missing, unverified or corrupted')
    components=[('0x0','bootloader.bin'),('0x8000','partitions.bin'),('0xe000','boot_app0.bin'),('0x10000','firmware.bin')]
    if not a.restore:
        manifest=json.loads((a.package/'manifest.json').read_text())
        if not info.get('board') or manifest.get('target')!=info['board']:
            raise SystemExit('Firmware board does not match the original backup')
        for _,name in components:
            b=(a.package/name).read_bytes()
            if len(b)!=manifest[name]['bytes'] or hashlib.sha256(b).hexdigest()!=manifest[name]['sha256']:
                raise SystemExit(f'Firmware component failed checksum: {name}')
    if a.check:
        print('Verified original backup and matching firmware package' if not a.restore else 'Verified original backup')
        return
    base=[sys.executable,'-m','esptool','--chip','esp32s3','--port',a.port,'--baud','115200']
    found=subprocess.check_output(base+['--after','no-reset','read-mac'],text=True)
    actual=re.search(r'MAC:\s*([\da-f:]+)',found,re.I)
    if not actual or actual.group(1).lower()!=info['mac'].lower():raise SystemExit('Connected device does not match the backup')
    if a.restore:
        subprocess.run(base+['write-flash','0x0',str(a.backup)],check=True)
        return
    args=base+['write-flash','--flash-mode','dio','--flash-freq','80m','--flash-size','16MB']
    for offset,name in components:args+=[offset,str(a.package/name)]
    subprocess.run(args,check=True)

if __name__=='__main__':main()
