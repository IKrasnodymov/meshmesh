#!/usr/bin/env python3
"""Drive the production M9 UI with USB key events and verify physical LoRa delivery."""
import json
import time
from contextlib import ExitStack
from pathlib import Path
from device import connect, command, screenshot
from ports import M9_PORT, HELTEC_PORT

def read(device, name):
    return json.loads(command(device, name))

def key(device, code):
    answer = command(device, f'uikey {code}')
    if not answer.startswith('OK'):
        raise RuntimeError(answer)

def main():
    with ExitStack() as stack:
        m9 = stack.enter_context(connect(M9_PORT))
        heltec = stack.enter_context(connect(HELTEC_PORT))
        config = read(m9, 'config')
        before = [read(d, 'status') for d in (m9, heltec)]
        stamp = str(time.time_ns())
        text = 'Привет ' + stamp
        # Select the tested Heltec by address; real MeshCore repeaters may precede it.
        if read(m9, 'ui')['locked']:
            key(m9, 0xa3) # Only a locked screen interprets this as unlock.
        key(m9, 0x82) # A sleeping screen consumes its first wake key.
        key(m9, 0x82)
        key(m9, 0xb7) # Home grid: Chats, Map, Nodes.
        key(m9, 0xb7)
        key(m9, 13)
        # Nodes are ordered by last reception; move focus by address, not by index.
        for _ in range(len(read(m9, 'nodes'))):
            if read(m9, 'ui').get('selected_node') == before[1]['node']:
                break
            key(m9, 0xb6)
        else:
            raise RuntimeError('Heltec is not listed in M9 nodes')
        key(m9, 13) # Node card; its first action is Message for chat nodes.
        assert read(m9, 'ui')['page'] == 'node'
        key(m9, 13)
        assert read(m9, 'ui')['page'] == 'chat'
        original_keyboard=read(m9,'ui')['keyboard_language']
        if original_keyboard=='EN':
            key(m9, 0x83)
        try:
            # Phonetic Russian: "Priveq" gives "Привея"; backspace removes one Cyrillic character.
            for character in 'Priveq':
                key(m9, ord(character))
            key(m9, 8)
            key(m9, ord('t'))
            key(m9, 0x83) # Digits 1-7 are Cyrillic letters in RU; type the stamp in EN.
            assert read(m9, 'ui')['keyboard_language'] == 'EN'
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
            if read(m9, 'ui')['keyboard_language'] != original_keyboard:
                key(m9, 0x83)
            key(m9, 0x82)
            key(m9, 0xb6) # Home grid: second row, then Connect.
            key(m9, 0xb7)
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
                  'checks': ['home grid', 'peer selection by address', 'node card', 'RU phonetic keyboard', '@ switches RU/EN', 'Cyrillic backspace', 'send with OK', 'DELIVERED', 'language restored'],
                  'before': before, 'after': after}
        path = Path('artifacts/ui-radio-check.json')
        path.touch(mode=0o600, exist_ok=True)
        path.chmod(0o600)
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
        print('PASS production UI: Cyrillic input/backspace and physical LoRa ACK')

if __name__ == '__main__':
    main()
