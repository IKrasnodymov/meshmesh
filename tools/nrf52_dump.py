#!/usr/bin/env python3
"""Read raw nRF52 flash over USB ("flashread", MeshMesh firmware): python tools/nrf52_dump.py PORT START END OUT."""
import sys
import time
import serial


def dump(port, start, end):
    data = bytearray()
    with serial.Serial(port, 115200, timeout=0.2) as s:
        s.dtr = True
        time.sleep(0.3)
        s.reset_input_buffer()
        at = start
        while at < end:
            n = min(0x800, end - at)
            s.write(f'flashread {at:x} {n:x}\n'.encode())
            buf, deadline = b'', time.time() + 5
            while time.time() < deadline:
                buf += s.read(8192)
                rows = [r.strip() for r in buf.split(b'\n') if r.startswith(b'FLASH ') and buf.endswith(b'\n')]
                if rows and len(rows[-1].split(b' ')[2]) == 2 * n:
                    break
            else:
                raise SystemExit(f'No reply for 0x{at:X}')
            _, address, hexdata = rows[-1].split(b' ')
            if int(address, 16) != at:
                raise SystemExit('Address mismatch')
            data += bytes.fromhex(hexdata.decode())
            at += n
    return bytes(data)


if __name__ == '__main__':
    port, start, end, out = sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3], 0), sys.argv[4]
    first = dump(port, start, end)
    if dump(port, start, end) != first:
        raise SystemExit('Two reads differ')
    open(out, 'wb').write(first)
    print(out, len(first))
