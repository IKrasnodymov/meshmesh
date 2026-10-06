#!/usr/bin/env python3
"""Check idle waits on every board and USB/UI recovery; optionally exercise LoRa IRQs with a peer.

The peer sends real adverts. This does not measure CPU sleep/current or prove direct-message ACKs.
Temporarily sets dim_after=10 and restores it; no radio profile or role changes.
"""
import argparse
import json
import time
from contextlib import ExitStack
from pathlib import Path

from device import connect, command
from radio_check import apply, read, PROFILE_FIELDS


def private_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.touch(mode=0o600, exist_ok=True)
    path.chmod(0o600)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def summary(status):
    fields = ('board', 'version', 'revision', 'build_sha256', 'boot', 'role', 'uptime', 'ble', 'radio', 'radio_error',
              'rx', 'tx', 'diagnostic_rx', 'idle_waits', 'idle_wait_ms', 'idle_radio_events')
    return {key: status[key] for key in fields if key in status}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--peer', help='another MeshMesh board; used only to send adverts')
    parser.add_argument('--output', type=Path, default=Path('artifacts/power-check.json'))
    args = parser.parse_args()
    if args.peer == args.port:
        parser.error('The peer must use a different port')
    report = {'passed': False, 'checks': [], 'cpu_sleep_measured': False, 'current_measured': False,
              'physical_buttons_tested': False, 'physical_lora_tested': False}
    with ExitStack() as stack:
        device = stack.enter_context(connect(args.port))
        before, config = read(device, 'status'), read(device, 'config')
        if 'idle_waits' not in before:
            raise RuntimeError('A build with idle diagnostics is required')
        if not before['radio']:
            raise RuntimeError('Radio is not ready')
        peer = stack.enter_context(connect(args.peer)) if args.peer else None
        if peer:
            peer_config = read(peer, 'config')
            if any(abs(config[k] - peer_config[k]) > .0001 for k in PROFILE_FIELDS):
                raise RuntimeError('Radio profiles differ; settings were not changed')
        recovery = Path('backups') / ('power-check-' + time.strftime('%Y%m%d-%H%M%S') + '.json')
        private_json(recovery, {'config': config, 'status': before})
        report['before'] = summary(before)
        checks_complete = False
        try:
            apply(device, {'dim_after': 10})
            if read(device, 'ui')['screen_off']:
                assert command(device, 'uikey 13').startswith('OK')
            active = read(device, 'status')
            time.sleep(.25)
            assert read(device, 'status')['idle_waits'] == active['idle_waits'], 'Waited during active input'
            report['checks'].append('recent USB input retains fast loop')
            print('PASS active input; waiting 30 seconds without polling the board', flush=True)
            time.sleep(30)
            idle = read(device, 'status')
            assert idle['idle_waits'] > active['idle_waits'], 'No idle waits'
            assert idle['idle_wait_ms'] - active['idle_wait_ms'] > 5000, 'Too little idle wait time'
            assert read(device, 'ui')['screen_off'], 'Screen did not turn off'
            report['idle'] = summary(idle)
            report['checks'].append('screen-off idle waits accumulate; USB responds afterwards')
            if peer:
                print('Sending peer adverts after the USB grace period', flush=True)
                time.sleep(13)
                for _ in range(3):
                    assert command(peer, 'hello').startswith('OK'), 'Peer advert rejected'
                    time.sleep(3)
                received = read(device, 'status')
                assert received['rx'] > idle['rx'], 'No LoRa reception'
                assert received['idle_radio_events'] > idle['idle_radio_events'], 'No LoRa notification during idle'
                report['received'] = summary(received)
                report['physical_lora_tested'] = True
                report['checks'].append('physical LoRa reception and task notification during idle')
            assert command(device, 'uikey 13').startswith('OK')
            assert not read(device, 'ui')['screen_off'], 'USB key did not wake the screen'
            report['checks'].append('production UI wakes through a simulated USB key')
            after = read(device, 'status')
            assert after['boot'] == before['boot'], 'Board restarted'
            assert after['diagnostic_rx'] == before['diagnostic_rx'], 'USB packet injection detected'
            assert after['radio'] and after['radio_error'] == 0, 'Radio error'
            report['after'] = summary(after)
            checks_complete = True
        finally:
            apply(device, {'dim_after': config['dim_after']})
            report['config_restored'] = read(device, 'config') == config
            report['passed'] = checks_complete and report['config_restored']
            private_json(args.output, report)
        assert report['config_restored'], 'Configuration changed'
    print('PASS idle/USB/UI checks; configuration restored', flush=True)
    if not peer:
        print('LoRa wake-up was not tested: no peer supplied', flush=True)


if __name__ == '__main__':
    main()
