#!/usr/bin/env python3
"""Verify delivery and ACKs over physical LoRa between two MeshMesh devices."""
import argparse
import json
import time
from contextlib import ExitStack
from pathlib import Path
from device import connect, command
from ports import M9_PORT, HELTEC_PORT

RADIO_FIELDS = ('frequency', 'bandwidth', 'sf', 'cr', 'power', 'hops')
# What two nodes must share to hear each other; power and hops are each node's own choice.
PROFILE_FIELDS = ('frequency', 'bandwidth', 'sf', 'cr')

def read(device, name):
    # nRF52 flash commits under BLE can postpone a large history reply.
    return json.loads(command(device, name, timeout=25 if name == 'messages' else 8))

def apply(device, settings):
    for _ in range(15):
        result = command(device, 'set ' + json.dumps(settings, ensure_ascii=False))
        if result == 'ERR radio busy; retry':
            time.sleep(.5)
            continue
        if not result.startswith('OK'):
            raise RuntimeError(result)
        return
    raise TimeoutError('Radio remained busy while applying configuration')

def delivery(sender, receiver, sender_id, receiver_id, text):
    started = time.monotonic()
    result = command(sender, f'send {receiver_id} {text}')
    if not result.startswith('OK'):
        raise RuntimeError(result)
    deadline = time.monotonic() + 100
    while time.monotonic() < deadline:
        outgoing = [m for m in read(sender, 'messages') if m['outgoing'] and m['text'] == text]
        incoming = [m for m in read(receiver, 'messages') if not m['outgoing'] and m['source'] == sender_id and m['text'] == text]
        if outgoing and outgoing[-1]['status'] == 3 and len(incoming) == 1:
            if incoming[0]['destination'] != receiver_id:
                raise AssertionError('Destination mismatch')
            return {'sender': sender_id, 'receiver': receiver_id, 'text': text,
                    'status': 'DELIVERED', 'receiver_copies': len(incoming), 'seconds': time.monotonic()-started}
        time.sleep(.4)
    raise TimeoutError(f'No physical delivery and ACK from {receiver_id}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--m9', default=M9_PORT)
    parser.add_argument('--heltec', required=True)
    parser.add_argument('--configure', action='store_true', help='Copy M9 radio settings and network key to Heltec')
    parser.add_argument('--output', type=Path, default=Path('artifacts/radio-check.json'))
    args = parser.parse_args()
    with ExitStack() as stack:
        m9 = stack.enter_context(connect(args.m9))
        heltec = stack.enter_context(connect(args.heltec))
        if args.configure:
            previous = read(heltec, 'key')
            path = Path('backups/heltec-meshmesh-config-before-radio-test.json')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch(mode=0o600, exist_ok=True)
            path.chmod(0o600)
            path.write_text(json.dumps(previous, indent=2) + '\n')
            source = read(m9, 'config')
            apply(heltec, {**{k: source[k] for k in RADIO_FIELDS}, 'name': 'Heltec V4'})
        configs = [read(d, 'config') for d in (m9, heltec)]
        before = [read(d, 'status') for d in (m9, heltec)]
        for key in PROFILE_FIELDS:
            if abs(configs[0][key] - configs[1][key]) > .0001:
                raise AssertionError(f'Radio setting mismatch: {key}')
        if before[0]['node'] == before[1]['node'] or before[0]['network'] != before[1]['network']:
            raise AssertionError('Distinct nodes in the same network required')
        if not all(s['radio'] for s in before):
            raise AssertionError('Both radios must be ready')
        if all(s.get('protocol')=='MeshCore' for s in before):
            for d in (m9,heltec):
                if not command(d,'hello').startswith('OK'):raise AssertionError('Advert rejected')
            for _ in range(30):
                if all(any(p.get('public_key')==other['public_key'] for p in read(d,'nodes')) for d,other in [(m9,before[1]),(heltec,before[0])]):break
                time.sleep(.4)
            else:raise AssertionError('Full-key MeshCore discovery missing')
        stamp = time.time_ns()
        checks = []
        for sender, receiver, a, b, label in ((m9, heltec, before[0], before[1], 'M9 → V4'),
                                             (heltec, m9, before[1], before[0], 'V4 → M9')):
            check = delivery(sender, receiver, a['node'], b['node'], f'Радиотест {label}: {stamp}')
            checks.append(check)
            print(f"PASS {label}: physical LoRa delivery and ACK", flush=True)
        after = [read(d, 'status') for d in (m9, heltec)]
        for old, new in zip(before, after):
            if new['boot'] != old['boot'] or new['diagnostic_rx'] != old['diagnostic_rx']:
                raise AssertionError('Device rebooted or USB injection occurred during physical test')
            if new['rx'] <= old['rx'] or new['tx'] <= old['tx']:
                raise AssertionError('Physical RX/TX counters did not increase')
        report = {'transport': 'physical LoRa; no USB packet injection',
                  'radio': {k: configs[0][k] for k in RADIO_FIELDS},
                  'radio_heltec': {k: configs[1][k] for k in RADIO_FIELDS},
                  'checks': checks, 'before': before, 'after': after}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.touch(mode=0o600, exist_ok=True)
        args.output.chmod(0o600)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
        print(f'Saved {args.output}')

if __name__ == '__main__':
    main()
