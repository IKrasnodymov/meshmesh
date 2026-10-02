#!/usr/bin/env python3
"""Check the site's browser installer for nRF52 boards on real hardware.

The board (running MeshMesh) is touched at 1200 baud into its serial bootloader; the bootloader
port is bridged to TCP and node runs site/nrf52dfu.js (tools/nrf52_dfu_check.mjs) on it - the code
the website runs on Web Serial. Then the running build hash is compared with the package.
Usage: nrf52_dfu_check.py PACKAGE_DIR
"""
import json
import socket
import subprocess
import sys
import threading
import time
import zipfile
from pathlib import Path

import serial

import nrf52

ROOT = Path(__file__).resolve().parents[1]


def bridge(device, server):
    s = serial.Serial(device, 115200, timeout=0.01)
    client, _ = server.accept()
    client.settimeout(0.01)
    alive = True

    def up():
        nonlocal alive
        while alive:
            try:
                data = client.recv(4096)
                if not data:
                    alive = False
                    break
                s.write(data)
            except socket.timeout:
                pass
            except OSError:
                alive = False
    threading.Thread(target=up, daemon=True).start()
    while alive:
        try:
            data = s.read(4096)
        except serial.SerialException:
            break  # the bootloader restarts into the new firmware
        if data:
            client.sendall(data)
    client.close()


def main():
    package = Path(sys.argv[1])
    manifest = json.loads((package / 'manifest.json').read_text())
    with zipfile.ZipFile(package / 'firmware-dfu.zip') as z:
        work = Path('/tmp') if not (ROOT / 'artifacts').exists() else ROOT / 'artifacts'
        (work / 'dfu-check.dat').write_bytes(z.read('firmware.dat'))
    if not list(nrf52.BACKUPS.glob('*-CURRENT.UF2')):
        raise SystemExit('No backup of the board in backups/gat562')
    where = nrf52.enter_bootloader(serial_ok=True)
    if isinstance(where, Path):
        raise SystemExit('The UF2 drive is mounted; this check needs the serial bootloader')
    server = socket.create_server(('127.0.0.1', 0))
    port = server.getsockname()[1]
    threading.Thread(target=bridge, args=(where, server), daemon=True).start()
    subprocess.run(['node', str(ROOT / 'tools/nrf52_dfu_check.mjs'), str(port), str(package / 'firmware.bin'),
                    str(work / 'dfu-check.dat')], check=True)
    app = nrf52.wait(nrf52.app_port, 60, 'the board to restart').device
    time.sleep(2)
    running = nrf52.status(app)
    ok = str(running.get('build_sha256', '')).lower() == manifest['firmware_sha256']
    print(json.dumps({k: running.get(k) for k in ('firmware', 'build_sha256', 'boot', 'storage', 'radio', 'role')}, indent=2))
    if not ok:
        raise SystemExit('Running build differs from the package')
    print('OK the browser installer code wrote', manifest['version'], 'and the board runs it')


if __name__ == '__main__':
    main()
