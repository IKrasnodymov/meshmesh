#!/usr/bin/env python3
"""nRF52 boards (GAT562 30S, Heltec T114): package, back up and install through the Adafruit UF2 bootloader.

  package ENV [--without chess,pet,dice] [--lang CODE]
                     firmware.bin, firmware.uf2 and INSTALL.txt in artifacts/meshmesh-<board>-<version>:
                     English and Russian, and in lang/<code>/ the image of English and that language
                     (1 MB flash: one image does not hold every language; tools/pio_lang.py).
                     --without leaves optional modules out (include/Modules.h; chess about 76 KB, pet 26 KB,
                     dice 17 KB on the T114) and builds the package itself, in its own folders, as
                     artifacts/meshmesh-<board>-<version>-without-<modules>; --lang builds that one language
                     image only (the full package builds all of them)
  backup             copy CURRENT.UF2 and INFO_UF2.TXT from the mounted bootloader drive (0600) into
                     backups/<board>/ (the board from the drive's Board-ID)
  flash PACKAGE [--lang CODE]  require a backup, enter the bootloader and write the package: a mounted UF2 drive
                     (RESET pressed twice) takes firmware.uf2; the 1200-baud touch of a running
                     MeshMesh opens the serial DFU bootloader, which takes firmware-dfu.zip
                     (adafruit-nrfutil, single bank). Then the running build hash is compared.

The bootloader writes only the blocks in the UF2 (application 0x26000-0xD4000): MeshMesh storage
at 0xD4000 and the stock MeshCore InternalFS at 0xED000 are kept.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import zipfile
from pathlib import Path

import serial
from serial.tools.list_ports import comports
from version import VERSION, check

ROOT = Path(__file__).resolve().parents[1]
BOARDS = {'gat562_30s': ('gat562-30s', 'GAT562 30S Mesh Kit'), 'heltec_t114': ('heltec-t114', 'Heltec Mesh Node T114')}
APP_START, APP_END = 0x26000, 0xD4000
FAMILY = 0xADA52840
VID = 0x239A
# Backups per board: the bootloader drive's Board-ID, the package target.
BACKUPS = {'gat562_30s': ROOT / 'backups' / 'gat562', 'heltec_t114': ROOT / 'backups' / 't114'}
BOARD_IDS = {'HT-n5262': 'heltec_t114'}  # any other Adafruit-style bootloader: the GAT562 (RAK4631 id)


def uf2(data, base):
    blocks = [data[i:i + 256] for i in range(0, len(data), 256)]
    out = bytearray()
    for n, chunk in enumerate(blocks):
        header = struct.pack('<8I', 0x0A324655, 0x9E5D5157, 0x2000, base + n * 256, 256, n, len(blocks), FAMILY)
        out += header + chunk.ljust(476, b'\0') + struct.pack('<I', 0x0AB16F30)
    return bytes(out)


def unuf2(data):
    """Address -> bytes of a UF2 file (CURRENT.UF2 of the bootloader)."""
    image = {}
    for i in range(0, len(data) - 511, 512):
        m0, m1, flags, addr, size, _, _, _ = struct.unpack('<8I', data[i:i + 32])
        if m0 != 0x0A324655 or m1 != 0x9E5D5157 or struct.unpack('<I', data[i + 508:i + 512])[0] != 0x0AB16F30:
            raise SystemExit(f'Bad UF2 block at {i}')
        image[addr] = data[i + 32:i + 32 + size]
    return image


def crc16(data):
    """CRC16 of the legacy DFU init packet, as adafruit-nrfutil and site/nrf52dfu.js."""
    crc = 0xFFFF
    for b in data:
        crc = ((crc >> 8) & 0xFF) | ((crc << 8) & 0xFF00)
        crc ^= b
        crc ^= (crc & 0xFF) >> 4
        crc ^= (crc << 12) & 0xFFFF
        crc ^= ((crc & 0xFF) << 5) & 0xFFFF
    return crc


def dfu_package(data, base_zip, out):
    """firmware-dfu.zip for an image: the init packet and manifest of base_zip with the image's CRC16."""
    with zipfile.ZipFile(base_zip) as z:
        dat, manifest = bytearray(z.read('firmware.dat')), json.loads(z.read('manifest.json'))
    crc = crc16(data)
    dat[-2:] = struct.pack('<H', crc)
    manifest['manifest']['application']['init_packet_data']['firmware_crc16'] = crc
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('firmware.bin', data)
        z.writestr('firmware.dat', bytes(dat))
        z.writestr('manifest.json', json.dumps(manifest, indent=4))


MODULES = ('chess', 'pet', 'dice')  # optional modules (include/Modules.h)
WITHOUT = ()  # the modules this run leaves out (package --without)


def slug():
    return '-'.join(sorted(WITHOUT))


def plain_build():
    """The build folder of the plain image: .pio/build, or one per module set."""
    return ROOT / '.pio/build' if not WITHOUT else ROOT / '.pio/build-mods' / slug()


def lang_build():
    """The language images (without Russian) build apart from the plain one."""
    return ROOT / '.pio/build-lang' if not WITHOUT else ROOT / '.pio/build-mods-lang' / slug()


def build(env, lang=None):
    """PlatformIO build of env; lang adds that screen language (MM_LANG, tools/pio_lang.py), WITHOUT the modules."""
    environ = {k: v for k, v in os.environ.items() if k not in ('MM_LANG', 'PLATFORMIO_BUILD_DIR', 'PLATFORMIO_BUILD_FLAGS')}
    if lang:
        environ['MM_LANG'] = lang
        environ['PLATFORMIO_BUILD_DIR'] = str(lang_build())
    elif WITHOUT:
        environ['PLATFORMIO_BUILD_DIR'] = str(plain_build())
    if WITHOUT:
        environ['PLATFORMIO_BUILD_FLAGS'] = ' '.join(f'-DMM_NO_{m.upper()}' for m in WITHOUT)
    subprocess.run([sys.executable, '-m', 'platformio', 'run', '-s', '-e', env], cwd=ROOT, env=environ, check=True)


def image(env, objcopy, out, lang=False):
    elf = (lang_build() if lang else plain_build()) / env / 'firmware.elf'
    subprocess.run([objcopy, '-O', 'binary', elf, out], check=True)
    data = out.read_bytes()
    if APP_START + len(data) > APP_END:
        raise SystemExit(f'Image of {len(data)} bytes runs into MeshMesh storage at 0x{APP_END:X}')
    return data


def languages(env, objcopy, target, base, only=None):
    """lang/<code>/: firmware.bin, firmware.uf2 and firmware-dfu.zip per screen language after en and ru
    (English and that language, without Russian). They build in their own folder, where after the first one
    each recompiles I18n.cpp only; then the plain image is built again and checked unchanged."""
    from i18n import CODES
    out = {}
    try:
        for code in [only] if only else CODES[2:]:
            build(env, code)
            folder = target / 'lang' / code
            folder.mkdir(parents=True, exist_ok=True)
            data = image(env, objcopy, folder / 'firmware.bin', lang=True)
            if data.count(b'MMLANG:--') != 1:
                raise SystemExit(f'{code}: the image needs exactly one language field')
            (folder / 'firmware.uf2').write_bytes(uf2(data, APP_START))
            dfu_package(data, target / 'firmware-dfu.zip', folder / 'firmware-dfu.zip')
            out[code] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'crc16': crc16(data)}
            print(f'  {code}: {len(data)} bytes, {(APP_END - APP_START - len(data)) // 1024} KB free')
    finally:
        build(env)
    if image(env, objcopy, target / 'check.bin') != base:
        raise SystemExit('The plain image changed after the language builds')
    (target / 'check.bin').unlink()
    return out


def version():
    check()
    return VERSION


def package(env, only=None):
    name, title = BOARDS[env]
    if WITHOUT:
        build(env)
    build_dir = plain_build() / env
    elf = build_dir / 'firmware.elf'
    if not elf.is_file():
        raise SystemExit('Build missing; run PlatformIO for this environment first')
    sources = [ROOT / 'platformio.ini', *(p for d in ('src', 'include', 'lib', 'boards', 'variants') for p in (ROOT / d).rglob('*')
               if p.is_file() and p.suffix in ('.cpp', '.c', '.h', '.inc', '.json', '.ld'))]
    if any(p.stat().st_mtime > elf.stat().st_mtime for p in sources):
        raise SystemExit('Firmware is older than project sources; run a successful build before packaging')
    objcopy = Path.home() / '.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-objcopy'
    target = ROOT / 'artifacts' / (f'meshmesh-{name}-{version()}' + (f'-without-{slug()}' if WITHOUT else ''))
    target.mkdir(parents=True, exist_ok=True)
    data = image(env, objcopy, target / 'firmware.bin')
    (target / 'firmware.uf2').write_bytes(uf2(data, APP_START))
    shutil.copy(build_dir / 'firmware.zip', target / 'firmware-dfu.zip')
    shutil.rmtree(target / 'lang', ignore_errors=True)
    langs = languages(env, objcopy, target, data, only)
    digest = hashlib.sha256(data).hexdigest()
    (target / 'INSTALL.txt').write_text(f"""MeshMesh {version()} для {title}

Установка через загрузчик UF2 (Adafruit nRF52):
  1. Дважды быстро нажмите RESET: появится диск (GAT562-BOOT, у T114 — HT-n5262).
     Плата с MeshMesh входит в загрузчик и сама: python tools/nrf52.py flash <пакет>.
  2. Перед первой установкой сохраните CURRENT.UF2 с этого диска: это копия прежней прошивки.
  3. Скопируйте firmware.uf2 на диск. Плата перезапустится с MeshMesh.

Записывается только область приложения 0x26000-0x{APP_END:X}. Хранилище MeshMesh (0xD4000-0xED000)
и данные штатной MeshCore (InternalFS, 0xED000) сохраняются. При первом запуске хранилище MeshMesh
создаётся, если область пуста; если там остались байты прежней прошивки, USB-команда fsformat
создаёт его по запросу.
Обновление по Bluetooth (DFU) не поддерживается: firmware-dfu.zip — для adafruit-nrfutil по USB.

{('Без модулей: ' + ', '.join(WITHOUT) + ' (tools/nrf52.py package --without; включить их обратно — установить полный пакет).' + chr(10) + chr(10)) if WITHOUT else ''}Языки экрана: образ в корне пакета — английский и русский. Образ английского и другого языка
(без русского) — в lang/<код>/ (uk, es, pt, fr, de, it, pl, tr, zh, ja, ko, ar, id): весь набор языков
не помещается в 1 МБ flash. Язык включается в настройках платы после установки.

SHA-256 firmware.bin: {digest}
""")
    uf2_digest = hashlib.sha256((target / 'firmware.uf2').read_bytes()).hexdigest()
    manifest = {'target': env, 'board': env, 'version': version(), 'chip': 'nrf52840', 'firmware_sha256': digest, 'size': len(data),
                'uf2_sha256': uf2_digest, 'firmware.bin': {'bytes': len(data), 'sha256': digest},
                'firmware.uf2': {'family': hex(FAMILY), 'base': hex(APP_START), 'sha256': uf2_digest}, 'languages': langs,
                'modules': [m for m in MODULES if m not in WITHOUT]}
    release = json.loads((build_dir / 'release.json').read_text())
    if release['version'] != version():
        raise SystemExit('Build version is stale')
    manifest.update(release)
    (target / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(target, len(data), digest)


def app_port():
    found = [p for p in comports() if p.vid == VID and p.pid & 0xFF00 == 0x8000 and p.device.startswith('/dev/cu.')]
    return found[0] if len(found) == 1 else None


def drive():
    for volume in Path('/Volumes').iterdir():
        info = volume / 'INFO_UF2.TXT'
        try:
            if info.is_file() and 'SoftDevice' in info.read_text(errors='replace'):
                return volume
        except OSError:
            pass
    return None


def wait(condition, seconds, what):
    end = time.time() + seconds
    while time.time() < end:
        value = condition()
        if value:
            return value
        time.sleep(0.5)
    raise SystemExit(f'Timed out waiting for {what}')


def dfu_port():
    found = [p for p in comports() if p.vid == VID and p.pid & 0xFF00 == 0 and p.device.startswith('/dev/cu.')]
    return found[0].device if len(found) == 1 else None


def enter_bootloader(serial_ok=False):
    """The mounted UF2 drive, or with serial_ok the serial DFU port after a 1200-baud touch."""
    volume = drive()
    if volume:
        return volume
    if serial_ok and dfu_port():
        return dfu_port()
    port = app_port()
    if not port:
        raise SystemExit('No nRF52 board: neither a bootloader drive nor an application port')
    print('1200-baud touch on', port.device)
    s = serial.Serial()
    s.port, s.baudrate, s.dtr = port.device, 1200, True
    s.open()
    time.sleep(0.2)
    s.dtr = False
    time.sleep(0.2)
    s.close()
    try:
        return wait(lambda: drive() or (serial_ok and dfu_port()), 20, 'the bootloader')
    except SystemExit:
        raise SystemExit('The firmware did not enter the bootloader: press RESET twice quickly, then run again')


def check_bootloader(volume):
    info = (volume / 'INFO_UF2.TXT').read_text(errors='replace')
    if 'S140 6.1.1' not in info:
        raise SystemExit('Unexpected SoftDevice (the build needs S140 6.1.1):\n' + info)
    return info


def drive_board(info):
    found = re.search(r'Board-ID: *(\S+)', info)
    return BOARD_IDS.get(found[1] if found else '', 'gat562_30s')


def backup():
    volume = enter_bootloader()
    info = check_bootloader(volume)
    folder = BACKUPS[drive_board(info)]
    folder.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    data = (volume / 'CURRENT.UF2').read_bytes()
    image = unuf2(data)
    span = (min(image), max(image) + len(image[max(image)]))
    current = folder / f'{stamp}-CURRENT.UF2'
    current.write_bytes(data)
    (folder / f'{stamp}-INFO_UF2.TXT').write_text(info)
    meta = {'captured': stamp, 'volume': str(volume), 'info': info, 'uf2_sha256': hashlib.sha256(data).hexdigest(),
            'bytes': len(data), 'blocks': len(image), 'range': [hex(span[0]), hex(span[1])]}
    (folder / f'{stamp}-backup.json').write_text(json.dumps(meta, indent=2) + '\n')
    for path in folder.glob(f'{stamp}-*'):
        os.chmod(path, 0o600)
    # A second read must match: the drive serves the flash as it is now.
    again = (volume / 'CURRENT.UF2').read_bytes()
    if again != data:
        raise SystemExit('Two reads of CURRENT.UF2 differ; backup not trusted')
    print(json.dumps(meta, indent=2))
    return current


def status(port, tries=40):
    for _ in range(tries):
        try:
            with serial.Serial(port, 115200, timeout=0.5) as s:
                s.dtr = True
                time.sleep(0.3)
                s.reset_input_buffer()
                s.write(b'status\n')
                end, buf = time.time() + 4, b''
                while time.time() < end:
                    buf += s.read(4096)
                    for line in buf.split(b'\n'):
                        if line.startswith(b'{') and b'"board"' in line:
                            return json.loads(line)
        except (OSError, serial.SerialException, ValueError):
            pass
        time.sleep(1)
    raise SystemExit('No status from the board')


def flash(path, lang=None):
    target = Path(path)
    manifest = json.loads((target / 'manifest.json').read_text())
    expected = manifest['firmware_sha256']
    if lang and lang not in ('en', 'ru'):  # the image with this screen language (lang/<code>/)
        if lang not in manifest.get('languages', {}):
            raise SystemExit(f'No image with language {lang} in {target}')
        expected = manifest['languages'][lang]['sha256']
        target = target / 'lang' / lang
    board = manifest['target']
    if not list(BACKUPS[board].glob('*-CURRENT.UF2')):
        raise SystemExit(f'No backup of the board in {BACKUPS[board].relative_to(ROOT)}: run "backup" first')
    running = app_port()
    if running and not drive() and status(running.device).get('board') != board:
        raise SystemExit(f'The board on {running.device} is not a {BOARDS[board][1]}')
    where = enter_bootloader(serial_ok=True)
    if isinstance(where, Path):
        if drive_board(check_bootloader(where)) != board:
            raise SystemExit(f'The bootloader drive {where} is not a {BOARDS[board][1]}')
        print('Copying', target / 'firmware.uf2', 'to', where)
        try:
            shutil.copyfile(target / 'firmware.uf2', where / 'firmware.uf2')
        except OSError as error:  # the bootloader restarts once the last block arrives; macOS may report it
            print('Drive closed during the copy:', error)
    else:
        # The DFU package names its SoftDevice (S140 6.1.1, 0xB6); the bootloader refuses another.
        nrfutil = Path.home() / '.platformio/packages/tool-adafruit-nrfutil/adafruit-nrfutil.py'
        print('Serial DFU on', where)
        subprocess.run([sys.executable, nrfutil, 'dfu', 'serial', '-p', where, '-b', '115200', '--singlebank',
                        '-pkg', target / 'firmware-dfu.zip'], check=True)
    port = wait(app_port, 60, 'the board to restart').device
    time.sleep(2)
    running = status(port)
    ok = str(running.get('build_sha256', '')).lower() == expected
    print(json.dumps({k: running.get(k) for k in ('board', 'firmware', 'build_sha256', 'boot', 'reset_reason', 'storage', 'radio', 'heap')}, indent=2))
    if not ok:
        raise SystemExit(f'Running build {running.get("build_sha256")} is not the package {expected}')
    print('OK installed and running', manifest['version'], 'on', port)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    pk = sub.add_parser('package')
    pk.add_argument('environment', choices=BOARDS)
    pk.add_argument('--without', default='', help='optional modules to leave out, comma-separated: ' + ', '.join(MODULES))
    pk.add_argument('--lang', help='build only this language image (besides English and Russian)')
    sub.add_parser('backup')
    f = sub.add_parser('flash')
    f.add_argument('package')
    f.add_argument('--lang', help='screen language of the image (en and ru are in every image)')
    args = parser.parse_args()
    if args.command == 'package':
        global WITHOUT
        WITHOUT = tuple(sorted({m.strip() for m in args.without.split(',') if m.strip()}))
        if any(m not in MODULES for m in WITHOUT):
            raise SystemExit('--without takes ' + ', '.join(MODULES))
        from i18n import CODES
        if args.lang and args.lang not in CODES[2:]:
            raise SystemExit('--lang takes one of ' + ' '.join(CODES[2:]))
        package(args.environment, args.lang)
    elif args.command == 'backup':
        backup()
    else:
        flash(args.package, args.lang)


if __name__ == '__main__':
    main()
