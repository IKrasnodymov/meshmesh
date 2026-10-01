#!/usr/bin/env python3
"""Assemble the GitHub Pages site: site/ plus browser-flashable firmware from tools/package.py packages.

tools/pages.py OUTDIR ENV...   (run tools/package.py ENV for each board first)
Each board gets firmware/<env>/manifest.json for ESP Web Tools and a zip of its package.
The browser installer writes the four components at their offsets, as an update with esptool
does: NVS (key, settings, contacts) and LittleFS (history) stay untouched unless the user
chooses to erase the device.
"""
import json
import re
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from package import COMMUNITY, TARGETS  # noqa: E402

# Boards whose packages are installed and checked on real hardware (docs/verification.md).
VERIFIED = {'m9', 'heltec_v4'}
README = """MeshMesh {version} — {board}

Update (keeps key, settings, contacts and history):
  python -m esptool --chip {chip} --port PORT write-flash --flash-mode dio --flash-freq {freq} --flash-size {size} \\
    {boot} bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin

Or in Chrome/Edge: https://ikrasnodymov.github.io/meshmesh/#install
"""


def main():
    out, envs = Path(sys.argv[1]), sys.argv[2:]
    version = re.search(r'MESHMM_VERSION "([^"]+)"', (ROOT/'include/Version.h').read_text())[1]
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(ROOT/'site', out)
    boards = []
    for env in envs:
        package = ROOT/'artifacts'/f'meshmesh-{TARGETS[env]}-{version}'
        meta = json.loads((package/'manifest.json').read_text())
        board_json = json.loads((ROOT/f'boards/meshmesh_{env}.json').read_text())
        chip = COMMUNITY[env][1] if env in COMMUNITY else 'esp32s3'
        size = board_json['upload']['flash_size']
        boot = 0x1000 if chip == 'esp32' else 0
        loader = (package/'bootloader.bin').read_bytes()
        # The image header must say DIO (byte 2 == 2): QIO boot-looped these boards.
        if loader[0] != 0xE9 or loader[2] != 2:
            raise SystemExit(f'{env}: bootloader is not a DIO image')
        target = out/'firmware'/env
        target.mkdir(parents=True)
        parts = [('bootloader.bin', boot), ('partitions.bin', 0x8000), ('boot_app0.bin', 0xe000), ('firmware.bin', 0x10000)]
        for name, _ in parts:
            shutil.copyfile(package/name, target/name)
        name = board_json['name'].removeprefix('MeshMesh / ')
        manifest = {'name': f'MeshMesh — {name}', 'version': version, 'new_install_prompt_erase': True,
                    'builds': [{'chipFamily': 'ESP32' if chip == 'esp32' else 'ESP32-S3',
                                'parts': [{'path': n, 'offset': o} for n, o in parts]}]}
        (target/'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        archive = out/'firmware'/f'meshmesh-{TARGETS[env]}-{version}.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
            for f in sorted(package.iterdir()):
                z.write(f, f'{package.name}/{f.name}')
            if not (package/'INSTALL.txt').exists():
                z.writestr(f'{package.name}/README.txt', README.format(
                    version=version, board=name, chip=chip, size=size, boot=hex(boot),
                    freq='40m' if chip == 'esp32' else '80m'))
        boards.append({'env': env, 'name': name, 'chip': manifest['builds'][0]['chipFamily'], 'flash': size,
                       'verified': env in VERIFIED, 'community': env in COMMUNITY,
                       'bytes': meta['firmware.bin']['bytes'], 'zip': f'firmware/{archive.name}'})
    (out/'firmware/boards.json').write_text(json.dumps({'version': version, 'boards': boards}, ensure_ascii=False, indent=2) + '\n')
    print(out, len(boards), 'boards')


if __name__ == '__main__':
    main()
