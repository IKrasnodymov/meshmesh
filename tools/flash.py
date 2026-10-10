#!/usr/bin/env python3
"""Install MeshMesh or restore the verified full backup on the matching device."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
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
    p.add_argument('--chunk-size',type=int,default=0,help='Write the application in separate sector-aligned blocks for unreliable USB bridges')
    p.add_argument('--check',action='store_true',help='Validate backup and package files without accessing USB');a=p.parse_args()
    if a.chunk_size and (a.chunk_size<4096 or a.chunk_size%4096 or a.restore):
        p.error('--chunk-size must be a positive multiple of 4096 and is only for installation')
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
    options=['write-flash','--flash-mode','dio','--flash-freq','80m','--flash-size','16MB']
    if a.chunk_size:
        args=base+['--after','no-reset']+options
        for offset,name in components[:-1]:args+=[offset,str(a.package/name)]
        subprocess.run(args,check=True)
        firmware=(a.package/'firmware.bin').read_bytes()
        with tempfile.TemporaryDirectory(prefix='meshmesh-flash-') as temporary:
            chunk=Path(temporary)/'application.bin'
            for start in range(0,len(firmware),a.chunk_size):
                chunk.write_bytes(firmware[start:start+a.chunk_size])
                print(f'Application block {start//a.chunk_size+1}/{(len(firmware)+a.chunk_size-1)//a.chunk_size}',flush=True)
                subprocess.run(base+['--after','no-reset']+options+[hex(0x10000+start),str(chunk)],check=True)
        # Check the complete application on flash before allowing it to boot.
        subprocess.run(base+['verify-flash','0x10000',str(a.package/'firmware.bin')],check=True)
        return
    args=base+options
    for offset,name in components:args+=[offset,str(a.package/name)]
    subprocess.run(args,check=True)

if __name__=='__main__':main()
