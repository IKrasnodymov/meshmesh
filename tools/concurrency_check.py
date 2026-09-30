#!/usr/bin/env python3
"""Verify that a slow M9 USB screenshot does not block LoRa reception/ACK."""
import json
import threading
import time
from contextlib import ExitStack
from pathlib import Path
from device import connect, command, screenshot
from ports import M9_PORT, HELTEC_PORT

def read(device, name):
    return json.loads(command(device, name))

def main():
    with ExitStack() as stack:
        m9 = stack.enter_context(connect(M9_PORT))
        heltec = stack.enter_context(connect(HELTEC_PORT))
        before = [read(device, 'status') for device in (m9, heltec)]
        captured = {}
        started = threading.Event()
        def capture():
            captured['start'] = time.monotonic()
            started.set()
            try:
                captured['file'] = screenshot(m9, 'artifacts/concurrent-screen.ppm')
            except BaseException as error:
                captured['error'] = str(error)
            finally:
                captured['end'] = time.monotonic()
        worker = threading.Thread(target=capture)
        worker.start()
        try:
            assert started.wait(1), 'Screenshot thread did not start'
            time.sleep(1)
            assert worker.is_alive(), 'Screenshot already complete; no concurrent load'
            text = f'USB + LoRa: {time.time_ns()}'
            begin = time.monotonic()
            result = command(heltec, f"send {before[0]['node']} {text}")
            assert result.startswith('OK'), result
            deadline = begin + 8
            while time.monotonic() < deadline:
                rows = [m for m in read(heltec, 'messages') if m['outgoing'] and m['text'] == text]
                if rows and rows[-1]['status'] == 3:
                    duration = time.monotonic() - begin
                    assert worker.is_alive(), 'ACK arrived after screenshot, not concurrently'
                    break
                time.sleep(.1)
            else:
                raise TimeoutError('M9 did not ACK while streaming screenshot (8-second bound)')
        finally:
            worker.join(timeout=35)
        assert not worker.is_alive() and 'error' not in captured, captured
        incoming = [m for m in read(m9, 'messages') if not m['outgoing'] and m['text'] == text]
        assert len(incoming) == 1 and incoming[0]['source'] == before[1]['node']
        after = [read(device, 'status') for device in (m9, heltec)]
        for old, new in zip(before, after):
            assert old['boot'] == new['boot']
            assert old['diagnostic_rx'] == new['diagnostic_rx']
            assert new['rx'] > old['rx'] and new['tx'] > old['tx']
        report = {'transport': 'physical LoRa while streaming USB framebuffer',
                  'delivery': 'DELIVERED', 'ack_during_screenshot': True,
                  'delivery_seconds': duration,
                  'screenshot_seconds': captured['end']-captured['start'],
                  'text': text, 'before': before, 'after': after}
        path = Path('artifacts/concurrency-check.json')
        path.touch(mode=0o600, exist_ok=True);path.chmod(0o600)
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
        print(f'PASS LoRa ACK in {duration:.2f}s during {report["screenshot_seconds"]:.2f}s USB screenshot')

if __name__ == '__main__':
    main()
