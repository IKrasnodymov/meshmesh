#!/usr/bin/env python3
"""Real LoRa messages/ACKs from idle on both boards; no receiver USB polling until ACK.

Temporarily selects normal mode and dim_after=10; restores configurations, roles and BLE states.
Reports and recovery snapshots are private. This does not measure current or physical buttons.
"""
import argparse
import json
import time
from contextlib import ExitStack
from pathlib import Path
from device import connect, command
from radio_check import apply, read, PROFILE_FIELDS
from power_check import private_json, summary


def set_role(port, target):
    with connect(port) as device:
        if read(device, 'status')['role'] == target:
            return
        if not command(device, 'role ' + target).startswith('OK'):
            raise RuntimeError('Role change rejected')
    time.sleep(5)
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        try:
            with connect(port) as device:
                if read(device, 'status')['role'] == target:
                    return
        except (OSError, TimeoutError):
            pass
        time.sleep(1)
    raise RuntimeError('Role change did not finish')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ports', nargs=2)
    parser.add_argument('--output', type=Path, default=Path('artifacts/power-pair-check.json'))
    args = parser.parse_args()
    if args.ports[0] == args.ports[1]:
        parser.error('Two different ports are required')
    originals = []
    for port in args.ports:
        with connect(port) as device:
            originals.append({'status': read(device, 'status'), 'config': read(device, 'config')})
    if any(abs(originals[0]['config'][k] - originals[1]['config'][k]) > .0001 for k in PROFILE_FIELDS):
        raise RuntimeError('Radio profiles differ; no settings changed')
    private_json(Path('backups') / f'power-pair-{time.time_ns()}.json', originals)
    report = {'passed': False, 'checks': [], 'current_measured': False, 'physical_buttons_tested': False}
    try:
        for port in args.ports:
            set_role(port, 'normal')
        with ExitStack() as stack:
            devices = [stack.enter_context(connect(p)) for p in args.ports]
            for device in devices:
                apply(device, {'dim_after': 10})
            before = [read(d, 'status') for d in devices]
            if not all(s['radio'] and 'idle_waits' in s for s in before):
                raise RuntimeError('Both boards need the event-wait firmware and working radios')
            report['before'] = [summary(s) for s in before]
            for device in devices:
                assert command(device, 'hello').startswith('OK')
            deadline = time.monotonic() + 30
            while not all(any(n.get('public_key') == before[1-i]['public_key'] for n in read(d, 'nodes')) for i, d in enumerate(devices)):
                if time.monotonic() >= deadline:
                    raise RuntimeError('Mutual contact discovery missing')
                time.sleep(1)
            stamp = time.time_ns()
            for cycle in range(3):
                for receiver_index in (0, 1):
                    receiver, sender = devices[receiver_index], devices[1-receiver_index]
                    old = read(receiver, 'status')
                    time.sleep(15)  # screen timeout and input grace expire; no receiver USB polling
                    text = f'Power check {stamp}-{cycle}-{receiver_index}'
                    assert command(sender, 'send ' + before[receiver_index]['node'] + ' ' + text).startswith('OK')
                    deadline = time.monotonic() + 100
                    while not any(m['outgoing'] and m['text'] == text and m['status'] == 3 for m in read(sender, 'messages')):
                        if time.monotonic() >= deadline:
                            raise RuntimeError('No ACK within the normal radio-check timeout')
                        time.sleep(.5)
                    received = [m for m in read(receiver, 'messages') if not m['outgoing'] and m['text'] == text
                                and m['source'] == before[1-receiver_index]['node']]
                    new = read(receiver, 'status')
                    assert len(received) == 1 and received[0]['destination'] == before[receiver_index]['node']
                    assert new['idle_waits'] > old['idle_waits'] and new['idle_radio_events'] > old['idle_radio_events']
                    assert new['rx'] > old['rx'] and new['tx'] > old['tx']
                    report['checks'].append({'receiver': new['board'], 'before': summary(old), 'after': summary(new), 'copies': 1, 'ack': True})
                    print(f'PASS {new["board"]}: idle LoRa notification, one copy, ACK (round {cycle+1})', flush=True)
            after = [read(d, 'status') for d in devices]
            for old, new in zip(before, after):
                assert old['boot'] == new['boot'] and old['diagnostic_rx'] == new['diagnostic_rx']
                assert new['radio'] and new['radio_error'] == 0
            report['after'] = [summary(s) for s in after]
            report['passed'] = True
    except Exception as error:
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        restored = []
        for port, original in zip(args.ports, originals):
            with connect(port) as device:
                apply(device, {'dim_after': original['config']['dim_after']})
            set_role(port, original['status']['role'])
            with connect(port) as device:
                now = read(device, 'status')
                restored.append(read(device, 'config') == original['config'] and all(now[k] == original['status'][k] for k in ('public_key', 'node', 'role', 'ble')))
                report.setdefault('final', []).append(summary(now))
        report['restored'] = restored
        report['passed'] = report['passed'] and all(restored)
        private_json(args.output, report)
        assert all(restored), 'Original settings/roles/identities were not restored'
    print('PASS both boards; settings, roles, BLE and identities restored', flush=True)


if __name__ == '__main__':
    main()
