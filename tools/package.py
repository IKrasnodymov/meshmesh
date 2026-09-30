#!/usr/bin/env python3
"""Package a completed PlatformIO build for a specific MeshMesh board."""
import argparse
import hashlib
import json
import shutil
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {'m9': 'm9', 'heltec_v4': 'heltec-v4', 'heltec_v4_r8': 'heltec-v4-r8'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('environment', choices=TARGETS)
    args = parser.parse_args()
    version = re.search(r'MESHMM_VERSION "([^"]+)"', (ROOT/'include/Version.h').read_text())[1]
    build = ROOT / '.pio/build' / args.environment
    package = ROOT / 'artifacts' / f'meshmesh-{TARGETS[args.environment]}-{version}'
    framework = Path.home() / '.platformio/packages/framework-arduinoespressif32'
    files = {name: build / name for name in ('bootloader.bin', 'partitions.bin', 'firmware.bin')}
    files['boot_app0.bin'] = framework / 'tools/partitions/boot_app0.bin'
    if not all(path.is_file() for path in files.values()):
        raise SystemExit('Build files missing; run PlatformIO for this environment first')
    sources = [ROOT/'platformio.ini',ROOT/'partitions.csv']
    for directory in ['src','include','lib','boards']:
        sources.extend(p for p in (ROOT/directory).rglob('*') if p.is_file() and p.suffix in ('.cpp','.c','.h','.json'))
    if any(p.stat().st_mtime > files['firmware.bin'].stat().st_mtime for p in sources):
        raise SystemExit('Firmware is older than project sources; run a successful build before packaging')
    package.mkdir(parents=True, exist_ok=True)
    manifest = {'target': args.environment, 'version': version}
    for name, source in files.items():
        shutil.copyfile(source, package / name)
        data = (package / name).read_bytes()
        manifest[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    manifest['app_elf_sha256'] = (package/'firmware.bin').read_bytes()[0xb0:0xd0].hex().upper()
    (package / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(package)

if __name__ == '__main__':
    main()
