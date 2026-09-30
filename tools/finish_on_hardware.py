#!/usr/bin/env python3
"""Install and check M9 first, then Heltec V4; stop at the first failed step.

Run from a terminal with USB access. --check validates both installation
packages and backups without opening USB or changing either device.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from device import command, connect
from version import VERSION
from ports import M9_PORT, HELTEC_PORT

ROOT = Path(__file__).resolve().parents[1]
M9 = M9_PORT
HELTEC = HELTEC_PORT
PACKAGES = {
    'm9': ROOT / f'artifacts/meshmesh-m9-{VERSION}',
    'heltec_v4': ROOT / f'artifacts/meshmesh-heltec-v4-{VERSION}',
}
RECEIPT = ROOT / 'artifacts/hardware-finish.json'


def save(value):
    RECEIPT.touch(mode=0o600, exist_ok=True)
    RECEIPT.chmod(0o600)
    RECEIPT.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def package_identity():
    return {board: hashlib.sha256((path / 'manifest.json').read_bytes()).hexdigest()
            for board, path in PACKAGES.items()}


def expect_ok(device, request):
    if not command(device, request).startswith('OK'):
        raise RuntimeError('Device rejected ' + request.split()[0])


def interfaces(port, wifi=None, ble=None):
    with connect(port) as device:
        state = json.loads(command(device, 'status'))
        for name, wanted in [('wifi', wifi), ('ble', ble)]:
            if wanted is not None and state[name] != wanted:
                expect_ok(device, name)
                state = json.loads(command(device, 'status'))
                if state[name] != wanted:
                    raise RuntimeError(name + ' state did not change')


def m9_page(code):
    with connect(M9) as device:
        if json.loads(command(device, 'ui'))['locked']:
            expect_ok(device, 'uikey 163')
        for key in [130, 130, code]:
            expect_ok(device, f'uikey {key}')
            time.sleep(.2)


def ready(port, board):
    deadline = time.monotonic() + 20
    while True:
        try:
            with connect(port) as device:
                state = json.loads(command(device, 'status'))
            if state.get('board') != board or not state['radio']:
                raise RuntimeError('Unexpected board or radio initialization failure')
            return
        except (OSError, TimeoutError):
            if time.monotonic() >= deadline:
                raise
            time.sleep(.5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--resume', action='store_true', help='Continue a failed suite only when packages and verified radio-test boots are unchanged')
    args = parser.parse_args()
    os.chdir(ROOT)
    os.umask(0o077)
    (ROOT / 'logs').mkdir(exist_ok=True)
    flash_m9 = ['tools/flash.py']
    flash_heltec = ['tools/flash.py', '--port', HELTEC, '--backup',
                   'backups/heltec-v4-original-20260929.bin', '--package', str(PACKAGES['heltec_v4'])]
    for step in [flash_m9, flash_heltec]:
        subprocess.run([sys.executable, *step, '--check'], check=True)
    if args.check:
        print('PASS both packages and original backups; no USB access or device changes')
        return

    # Check access to BOTH ports before touching flash. Preserve earlier evidence.
    for port in [M9, HELTEC]:
        with connect(port) as device:
            json.loads(command(device, 'status'))
    if args.resume:
        receipt=json.loads(RECEIPT.read_text())
        if receipt['status']!='failed' or receipt['package_manifest_sha256']!=package_identity() or 'radio' not in receipt['completed']:
            raise RuntimeError('Cannot resume without unchanged packages and a passed radio stage')
        reference=json.loads((ROOT/'artifacts/radio-check.json').read_text())['after']
        for board,port,previous in zip(['m9','heltec_v4'],[M9,HELTEC],reference):
            with connect(port) as device:state=json.loads(command(device,'status'))
            manifest=json.loads((PACKAGES[board]/'manifest.json').read_text())
            if state['node']!=previous['node'] or state['boot']!=previous['boot'] or state['build_sha256']!=manifest['app_elf_sha256']:
                raise RuntimeError('Device or firmware changed; full installation suite required')
        receipt['status']='running';receipt.pop('error_type',None)
        receipt.setdefault('notes',[]).append(
            'Resumed on unchanged firmware and boots; completed stages retain their recorded evidence.')
    else:
        archive = ROOT / 'artifacts/evidence-before-finish' / time.strftime('%Y%m%d-%H%M%S')
        archive.mkdir(parents=True)
        for report in (ROOT / 'artifacts').glob('*check.json'):
            target = archive / report.name
            target.write_bytes(report.read_bytes())
            target.chmod(0o600)
        receipt = {'status': 'running', 'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                   'package_manifest_sha256': package_identity(), 'completed': [],
                   'evidence_archive': str(archive.relative_to(ROOT))}
    save(receipt)

    def run(name, *arguments):
        if name in receipt['completed']:return
        print(name, flush=True)
        receipt['active_step'] = name
        save(receipt)
        log = ROOT / 'logs' / ('finish-' + name + '.log')
        with log.open('w') as output:
            subprocess.run([sys.executable, *arguments], stdout=output,
                           stderr=subprocess.STDOUT, check=True)
        receipt['completed'].append(name)
        save(receipt)

    try:
        run('flash-m9', *flash_m9)
        ready(M9, 'm9')
        # The clock test resets M9. All remaining M9 evidence uses that boot.
        run('clock', 'tools/clock_check.py')
        run('persistence', 'tools/persistence_check.py')
        run('map-ui', 'tools/map_ui_check.py')
        # User requested the Heltec update after completing M9.
        run('flash-heltec', *flash_heltec)
        ready(HELTEC, 'heltec_v4')
        with connect(HELTEC) as device:
            expect_ok(device, 'clock ' + json.dumps({'unix': int(time.time())}))
        interfaces(M9, wifi=True, ble=True)
        interfaces(HELTEC, wifi=False, ble=True)
        run('radio', 'tools/radio_check.py', '--heltec', HELTEC)
        run('m9-ui', 'tools/ui_check.py')
        m9_page(133)  # MAP: include map rendering in concurrent radio/USB load.
        run('concurrency', 'tools/concurrency_check.py')
        run('heltec-ui', 'tools/heltec_ui_check.py')
        run('radar', 'tools/radar_check.py')
        run('csi', 'tools/csi_check.py')
        interfaces(HELTEC, wifi=False)
        run('m9-wifi', 'tools/wifi_probe.py')
        run('m9-ble', 'tools/ble_probe.py')
        interfaces(M9, wifi=False)
        interfaces(HELTEC, wifi=True)
        run('heltec-wifi', 'tools/wifi_probe.py', '--server', HELTEC, '--client', M9,
            '--output', 'artifacts/heltec-wifi-check.json')
        interfaces(M9, wifi=True, ble=True)
        interfaces(HELTEC, wifi=True, ble=True)
        run('heltec-ble', 'tools/ble_probe.py', '--server', HELTEC, '--client', M9,
            '--output', 'artifacts/heltec-ble-radio-check.json')
        m9_page(130)
        with connect(M9) as device:
            for key in [182, 183, 13]:  # Home grid: Connect; leaves credentials visible.
                expect_ok(device, f'uikey {key}')
                time.sleep(.2)
        run('release', 'tools/release_check.py')
        receipt['status'] = 'passed'
        receipt.pop('active_step', None)
        save(receipt)
        print('PASS installed packages and hardware suite on both devices; see artifacts/release-check.json')
    except Exception as error:
        receipt['status'] = 'failed'
        receipt['error_type'] = type(error).__name__
        save(receipt)
        print('STOP: failed at ' + receipt['active_step'] + '; see its logs/finish-*.log', file=sys.stderr)
        raise


if __name__ == '__main__':
    main()
