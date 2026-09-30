#!/usr/bin/env python3
"""Drive the production M9 UI with USB key events and verify physical LoRa delivery."""
import json
import time
from contextlib import ExitStack
from pathlib import Path
from device import connect, command, screenshot

def read(device, name):
    return json.loads(command(device, name))

def key(device, code):
    answer = command(device, f'uikey {code}')
    if not answer.startswith('OK'):
        raise RuntimeError(answer)

def main():
    with ExitStack() as stack:
        m9 = stack.enter_context(connect('/dev/cu.wchusbserial10'))
        heltec = stack.enter_context(connect('/dev/cu.usbmodem1101'))
        config = read(m9, 'config')
        before = [read(d, 'status') for d in (m9, heltec)]
        stamp = str(time.time_ns())
        text = 'Привет ' + stamp
        # Select the tested Heltec by address; real MeshCore repeaters may precede it.
        if read(m9, 'ui')['locked']:
            key(m9, 0xa3) # Only a locked screen interprets this as unlock.
        key(m9, 0x82) # A sleeping screen consumes its first wake key.
        key(m9, 0x82)
        key(m9, 0xb6)
        key(m9, 0xb6)
        key(m9, 13)
        peers=read(m9,'nodes');peer_index=next(i for i,p in enumerate(peers) if p['id']==before[1]['node'])
        for _ in range(peer_index):key(m9,0xb6)
        key(m9, 13)
        original_keyboard=read(m9,'ui')['keyboard_language']
        changed=original_keyboard=='EN'
        if changed:
            key(m9, 0x83)
        try:
            for character in 'Ghbdtn':
                key(m9, ord(character))
            key(m9, 8) # Remove the last Cyrillic character as one character.
            key(m9, ord('n'))
            for character in ' ' + stamp:
                key(m9, ord(character))
            time.sleep(.2)
            screenshot(m9, 'artifacts/ui-compose.ppm')
            key(m9, 13)
            deadline = time.monotonic() + 100
            while time.monotonic() < deadline:
                sent = [m for m in read(m9, 'messages') if m['outgoing'] and m['text'] == text]
                received = [m for m in read(heltec, 'messages') if not m['outgoing'] and m['text'] == text]
                if sent and sent[-1]['status'] == 3 and len(received) == 1:
                    assert received[0]['source'] == before[0]['node']
                    assert received[0]['destination'] == before[1]['node']
                    break
                time.sleep(.4)
            else:
                raise TimeoutError('UI message not confirmed over physical LoRa')
        finally:
            if changed:
                key(m9, 0x83)
            key(m9, 0x82)
            for _ in range(4):
                key(m9, 0xb6)
            key(m9, 13) # Leave current Wi-Fi and BLE credentials visible.
        after = [read(d, 'status') for d in (m9, heltec)]
        for old, new in zip(before, after):
            assert old['boot'] == new['boot']
            assert old['diagnostic_rx'] == new['diagnostic_rx']
            assert new['rx'] > old['rx'] and new['tx'] > old['tx']
        assert read(m9, 'config')['russian'] == config['russian']
        assert read(m9, 'ui')['keyboard_language']==original_keyboard
        report = {'input': 'simulated USB keys through real M9 UI; not physical button automation',
                  'transport': 'physical LoRa with delivery ACK', 'text': text,
                  'checks': ['peer selection', 'RU keyboard mapping', 'Cyrillic backspace', 'send with OK', 'DELIVERED', 'language restored'],
                  'before': before, 'after': after}
        path = Path('artifacts/ui-radio-check.json')
        path.touch(mode=0o600, exist_ok=True)
        path.chmod(0o600)
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
        print('PASS production UI: Cyrillic input/backspace and physical LoRa ACK')

if __name__ == '__main__':
    main()
