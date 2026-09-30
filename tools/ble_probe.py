#!/usr/bin/env python3
"""Use a second MeshMesh board as a physical BLE client, then verify LoRa ACK."""
import argparse
import json
import time
from contextlib import ExitStack
from pathlib import Path
from device import connect, command

def read(device, name):
    return json.loads(command(device, name))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', default='/dev/cu.usbmodem1101')
    parser.add_argument('--server', default='/dev/cu.wchusbserial10')
    parser.add_argument('--pin', type=int)
    parser.add_argument('--output', type=Path, default=Path('artifacts/ble-radio-check.json'))
    args = parser.parse_args()
    with ExitStack() as stack:
        client = stack.enter_context(connect(args.client))
        server = stack.enter_context(connect(args.server))
        before = [read(device, 'status') for device in (server, client)]
        if not before[0]['ble']:
            raise RuntimeError('Enable BLE on the server first')
        credentials=read(server, 'connections')
        pin=args.pin if args.pin is not None else credentials['pin']
        text = f'BLE → LoRa: проверка {time.time_ns()}'
        options = {'name': credentials['ble_name'], 'pin': pin, 'message': text}
        response = command(client, 'bleprobe ' + json.dumps(options, ensure_ascii=False),timeout=35)
        if not response.startswith('OK'):
            raise RuntimeError(response)
        deadline = time.monotonic() + 150
        previous = None
        while time.monotonic() < deadline:
            result = read(client, 'bleprobe')
            if result['stage'] != previous:
                print('BLE stage:', result['stage'], flush=True)
                previous = result['stage']
            if result['done']:
                break
            time.sleep(.5)
        else:
            raise TimeoutError('BLE diagnostic task did not finish')
        report = {'transport': 'physical BLE with passkey, followed by physical LoRa', 'ble': result, 'text': text}
        if result['error'] or not all(result[k] for k in ('encrypted','authenticated','selftest','status','config','messages','sent')):
            save(args.output, report)
            raise AssertionError('BLE diagnostic failed: ' + result['error'])
        deadline = time.monotonic() + 100
        while time.monotonic() < deadline:
            outgoing = [m for m in read(server, 'messages') if m['outgoing'] and m['text'] == text]
            incoming = [m for m in read(client, 'messages') if not m['outgoing'] and m['text'] == text]
            if outgoing and outgoing[-1]['status'] == 3 and len(incoming) == 1:
                assert incoming[0]['source'] == before[0]['node']
                assert incoming[0]['destination'] == before[1]['node']
                report['delivery'] = 'DELIVERED'
                break
            time.sleep(.4)
        else:
            save(args.output, report)
            raise TimeoutError('BLE command did not produce confirmed LoRa delivery')
        after = [read(device, 'status') for device in (server, client)]
        for old, new in zip(before, after):
            assert old['boot'] == new['boot'], 'Device rebooted'
            assert old['diagnostic_rx'] == new['diagnostic_rx'], 'USB injection occurred'
            assert new['rx'] > old['rx'] and new['tx'] > old['tx'], 'Physical LoRa counters unchanged'
        report.update(before=before, after=after)
        save(args.output, report)
        print('PASS authenticated BLE commands and BLE → LoRa → delivery ACK', flush=True)

def save(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.touch(mode=0o600, exist_ok=True)
    path.chmod(0o600)
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')

if __name__ == '__main__':
    main()
