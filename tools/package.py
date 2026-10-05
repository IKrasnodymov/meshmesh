#!/usr/bin/env python3
"""Package a completed PlatformIO build for a specific MeshMesh board."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {'m9': 'm9', 'heltec_v4': 'heltec-v4', 'heltec_v4_r8': 'heltec-v4-r8'}
# Community boards: built here, checked on hardware by their owners (docs/boards.md).
# environment: (package name, chip, flash size)
COMMUNITY = {
    'heltec_v3': ('heltec-v3', 'esp32s3', '8MB'),
    'heltec_tracker': ('heltec-tracker', 'esp32s3', '8MB'),
    'tdeck': ('tdeck', 'esp32s3', '16MB'),
    'tbeam': ('tbeam', 'esp32', '4MB'),
    'tbeam_supreme': ('tbeam-supreme', 'esp32s3', '8MB'),
    't3s3': ('t3s3', 'esp32s3', '4MB'),
    'tlora_v2_1_6': ('tlora-v2-1-6', 'esp32', '4MB'),
    'xiao_s3_wio': ('xiao-s3-wio', 'esp32s3', '8MB'),
    'station_g2': ('station-g2', 'esp32s3', '16MB'),
    'thinknode_m2': ('thinknode-m2', 'esp32s3', '4MB'),
}
TARGETS.update({env: name for env, (name, _, _) in COMMUNITY.items()})
# nRF52 boards (tools/nrf52.py): UF2 and DFU packages instead of ESP images.
NRF52 = {'gat562_30s': 'gat562-30s', 'heltec_t114': 'heltec-t114'}
TARGETS.update(NRF52)

INSTALL = """MeshMesh {version} для {board}

Сборка проверена только на компьютере: на этой плате прошивку ещё никто не запускал.
Сообщите, что работает и что нет (docs/boards.md).

Первая установка: образ занимает всю flash и стирает настройки и данные прежней прошивки.
  python -m esptool --chip {chip} --port PORT write-flash 0x0 {factory}

Обновление MeshMesh (сохраняет ключ, настройки, контакты и историю):
  python -m esptool --chip {chip} --port PORT write-flash --flash-mode dio --flash-freq {freq} --flash-size {size} \\
    {boot} bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin

{factory} можно записать и браузерным загрузчиком ESP (Web Serial) с адреса 0x0.
Если плата не входит в загрузчик сама: удерживайте BOOT, нажмите RESET, отпустите BOOT.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('environment', choices=TARGETS)
    args = parser.parse_args()
    if args.environment in NRF52:
        import nrf52
        nrf52.package(args.environment)
        return
    version = re.search(r'MESHMM_VERSION "([^"]+)"', (ROOT/'include/Version.h').read_text())[1]
    build = ROOT / '.pio/build' / args.environment
    package = ROOT / 'artifacts' / f'meshmesh-{TARGETS[args.environment]}-{version}'
    framework = Path.home() / '.platformio/packages/framework-arduinoespressif32'
    files = {name: build / name for name in ('bootloader.bin', 'partitions.bin', 'firmware.bin')}
    files['boot_app0.bin'] = framework / 'tools/partitions/boot_app0.bin'
    if not all(path.is_file() for path in files.values()):
        raise SystemExit('Build files missing; run PlatformIO for this environment first')
    sources = [ROOT/'platformio.ini', *ROOT.glob('partitions*.csv')]
    for directory in ['src','include','lib','boards']:
        sources.extend(p for p in (ROOT/directory).rglob('*') if p.is_file() and p.suffix in ('.cpp','.c','.h','.inc','.json'))
    if any(p.stat().st_mtime > files['firmware.bin'].stat().st_mtime for p in sources):
        raise SystemExit('Firmware is older than project sources; run a successful build before packaging')
    package.mkdir(parents=True, exist_ok=True)
    manifest = {'target': args.environment, 'version': version}
    for name, source in files.items():
        shutil.copyfile(source, package / name)
        data = (package / name).read_bytes()
        manifest[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    manifest['app_elf_sha256'] = (package/'firmware.bin').read_bytes()[0xb0:0xd0].hex().upper()
    if args.environment in COMMUNITY:
        name, chip, size = COMMUNITY[args.environment]
        boot = '0x1000' if chip == 'esp32' else '0x0'
        freq = '40m' if chip == 'esp32' else '80m'
        factory = f'meshmesh-{name}-{version}-factory.bin'
        # Whole flash: the blank NVS and LittleFS areas make the first boot create fresh storage.
        subprocess.run([sys.executable, '-m', 'esptool', '--chip', chip, 'merge-bin', '-o', str(package/factory),
                        '--flash-mode', 'dio', '--flash-freq', freq, '--flash-size', size, '--pad-to-size', size,
                        boot, str(package/'bootloader.bin'), '0x8000', str(package/'partitions.bin'),
                        '0xe000', str(package/'boot_app0.bin'), '0x10000', str(package/'firmware.bin')],
                       check=True, stdout=subprocess.DEVNULL)
        data = (package/factory).read_bytes()
        manifest.update({'chip': chip, 'flash_size': size, 'hardware_verified': False,
                         'offsets': {'bootloader.bin': boot, 'partitions.bin': '0x8000', 'boot_app0.bin': '0xe000', 'firmware.bin': '0x10000'},
                         'factory': {'file': factory, 'offset': '0x0', 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}})
        board = json.loads((ROOT/f'boards/meshmesh_{args.environment}.json').read_text())['name'].removeprefix('MeshMesh / ')
        (package/'INSTALL.txt').write_text(INSTALL.format(version=version, board=board, chip=chip, factory=factory, boot=boot, freq=freq, size=size))
    (package / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(package)

if __name__ == '__main__':
    main()
