#!/usr/bin/env python3
"""Pass a board's USB command line to one TCP client: the Android app's "USB через компьютер".

For checking the app in the Android emulator (it reaches this computer as 10.0.2.2) with a real
board: the app speaks exactly the USB protocol, so its command layer is the one a phone uses over
USB. Control lines as tools/device.py (no reset). Baud switching is refused: the bridge stays at
115200. --raw passes bytes as they are, for binary protocols (the MeshCore companion frames of
tools/chess_companion_check.py). Usage: usb_tcp_bridge.py [--port PORT] [--listen 8771] [--raw]
"""
import argparse
import socket
import threading
from device import connect
from ports import M9_PORT


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', default=M9_PORT)
    p.add_argument('--listen', type=int, default=8771)
    p.add_argument('--host', default='127.0.0.1')
    p.add_argument('--raw', action='store_true', help='no line handling: bytes as they are')
    a = p.parse_args()
    device = connect(a.port)
    device.timeout = 0.05
    server = socket.create_server((a.host, a.listen))
    print(f'tcp://{a.host}:{a.listen} -> {a.port}', flush=True)
    while True:
        client, peer = server.accept()
        client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        print('client', peer, flush=True)
        alive = threading.Event()
        alive.set()

        def board_to_client():
            while alive.is_set():
                data = device.read(4096)
                if data:
                    try:
                        client.sendall(data)
                    except OSError:
                        break
            alive.clear()

        reader = threading.Thread(target=board_to_client, daemon=True)
        reader.start()
        pending = b''
        try:
            while alive.is_set():
                data = client.recv(4096)
                if not data:
                    break
                if a.raw:
                    device.write(data)
                    continue
                pending += data
                while b'\n' in pending:
                    line, _, pending = pending.partition(b'\n')
                    if line.strip().startswith(b'baud '):  # the bridge's own serial rate stays fixed
                        client.sendall(b'ERR baud switching is not available through the bridge\n')
                        continue
                    device.write(line + b'\n')
                if pending.strip(b'\r') == b'':  # keep-alive carriage returns
                    device.write(pending)
                    pending = b''
        except OSError:
            pass
        alive.clear()
        reader.join(1)
        client.close()
        print('client closed', flush=True)


if __name__ == '__main__':
    main()
