#!/usr/bin/env python3
"""Pass a board's USB command line to one TCP client: the Android app's "USB через компьютер".

For checking the app in the Android emulator (it reaches this computer as 10.0.2.2) with a real
board: the app speaks exactly the USB protocol, so its command layer is the one a phone uses over
USB. Control lines as tools/device.py (no reset). Baud switching is refused: the bridge stays at
115200. --raw passes bytes as they are, for binary protocols (the MeshCore companion frames of
tools/chess_companion_check.py). Usage: usb_tcp_bridge.py [--port PORT] [--listen 8771] [--raw]

Firmware over USB (android flash/SerialIo.kt): a client that starts with the line "MMRAW1" gets
"MMRAW1 native" or "MMRAW1 uart", then raw bytes from the board, and sends frames
[type, length u16 LE, payload]: 0 data, 1 DTR and RTS (one byte each), 2 baud rate u32 LE,
3 nRF52 bootloader: the 1200-baud touch, then the bridge moves to the bootloader's port and answers
"MMBOOT ok" (or "MMBOOT fail <why>"); when the client leaves, it returns to the application's port.
The ESP32 ROM loader then has the port as esptool would; when the client leaves, the bridge
returns to 115200 and the no-reset control lines. A port that disappears (native USB after a
reset) is opened again.
"""
import argparse
import socket
import struct
import threading
import time
import serial
from serial.tools.list_ports import comports
from device import connect
from ports import M9_PORT

NRF_VID = 0x239A  # Adafruit nRF52: the application's product ID has 0x8000, the bootloader's has not


def nrf_ports(bootloader):
    return [p.device for p in comports() if p.vid == NRF_VID and bool(p.pid & 0x8000) != bootloader and p.device.startswith('/dev/cu.')]


class Port:
    """The serial port, opened again when the board's USB device comes back."""

    def __init__(self, name):
        self.name = name
        self.lock = threading.Lock()
        self.switching = False
        self.device = self.open()

    def open(self):
        end = time.monotonic() + 15
        while True:
            try:
                d = connect(self.name)
                d.timeout = 0.05
                return d
            except (serial.SerialException, OSError):
                if time.monotonic() > end:
                    raise
                time.sleep(0.3)

    def reopen(self):
        with self.lock:
            try:
                self.device.close()
            except Exception:
                pass
            print('port lost, reopening', flush=True)
            self.device = self.open()

    def read(self):
        if self.switching:
            time.sleep(0.1)
            return b''
        try:
            return self.device.read(4096)
        except (serial.SerialException, OSError, TypeError, AttributeError):
            time.sleep(0.2)
            if self.switching or getattr(self, 'nrf', False):
                return b''  # the nRF52 bootloader left the bus: the session end moves to the application
            try:
                self.reopen()
            except (serial.SerialException, OSError):
                pass
            return b''

    def write(self, data):
        try:
            self.device.write(data)
        except (serial.SerialException, OSError):
            self.reopen()
            self.device.write(data)

    def move(self, find, seconds, what):
        """Waits for a port that find() returns and opens it instead of the current one."""
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            found = find()
            if found:
                with self.lock:
                    self.name = found[0]
                    self.device = self.open()
                print('moved to', self.name, flush=True)
                return
            time.sleep(0.3)
        raise OSError(f'no {what} port')

    def nrf_bootloader(self):
        self.switching = True
        try:
            if not nrf_ports(True):  # not already there after a failed update
                d = self.device
                d.baudrate = 1200
                d.dtr = True
                time.sleep(0.1)
                d.dtr = False
                time.sleep(0.1)
                try:
                    d.close()
                except Exception:
                    pass
            self.move(lambda: nrf_ports(True), 20, 'bootloader')
        finally:
            self.switching = False

    def nrf_application(self):
        self.switching = True
        try:
            self.move(lambda: nrf_ports(False), 20, 'application')
        finally:
            self.switching = False

    def default_lines(self):
        """115200 and the lines of tools/device.py: native USB DTR on, RTS off; no reset."""
        try:
            d = self.device
            d.baudrate = 115200
            d.rts = False
            d.dtr = bool(getattr(d, 'meshmesh_native', False))
        except (serial.SerialException, OSError):
            self.reopen()


def raw_session(port, client, alive):
    lines = {'dtr': None, 'rts': None}
    buffer = b''
    while alive.is_set():
        data = client.recv(65536)
        if not data:
            break
        buffer += data
        while len(buffer) >= 3:
            kind, size = buffer[0], struct.unpack_from('<H', buffer, 1)[0]
            if len(buffer) < 3 + size:
                break
            payload, buffer = buffer[3:3 + size], buffer[3 + size:]
            if kind == 0:
                port.write(payload)
            elif kind == 1:  # only the line that changed, in the order esptool sets them
                dtr, rts = bool(payload[0]), bool(payload[1])
                if lines['dtr'] != dtr:
                    port.device.dtr = lines['dtr'] = dtr
                if lines['rts'] != rts:
                    port.device.rts = lines['rts'] = rts
            elif kind == 2:
                port.device.baudrate = struct.unpack('<I', payload)[0]
            elif kind == 3:
                try:
                    port.nrf_bootloader()
                    port.nrf = True
                    client.sendall(b'MMBOOT ok\n')
                except OSError as e:
                    client.sendall(f'MMBOOT fail {e}\n'.encode())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', default=M9_PORT)
    p.add_argument('--listen', type=int, default=8771)
    p.add_argument('--host', default='127.0.0.1')
    p.add_argument('--raw', action='store_true', help='no line handling: bytes as they are')
    a = p.parse_args()
    port = Port(a.port)
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
                data = port.read()
                if data:
                    try:
                        client.sendall(data)
                    except OSError:
                        break
            alive.clear()

        pending = b''
        reader = None
        try:
            first = client.recv(4096)
            if first.startswith(b'MMRAW1\n'):
                pending = None
                native = bool(getattr(port.device, 'meshmesh_native', False))
                port.device.reset_input_buffer()
                client.sendall(b'MMRAW1 native\n' if native else b'MMRAW1 uart\n')
                print('flash session', 'native' if native else 'uart', flush=True)
                reader = threading.Thread(target=board_to_client, daemon=True)
                reader.start()
                raw_session(port, client, alive)
            else:
                reader = threading.Thread(target=board_to_client, daemon=True)
                reader.start()
                data = first
                while alive.is_set() and data:
                    if a.raw:
                        port.write(data)
                    else:
                        pending += data
                        while b'\n' in pending:
                            line, _, pending = pending.partition(b'\n')
                            if line.strip().startswith(b'baud '):  # the bridge's own serial rate stays fixed
                                client.sendall(b'ERR baud switching is not available through the bridge\n')
                                continue
                            port.write(line + b'\n')
                        if pending.strip(b'\r') == b'':  # keep-alive carriage returns
                            port.write(pending)
                            pending = b''
                    data = client.recv(4096)
        except OSError:
            pass
        alive.clear()
        if reader:
            reader.join(1)
        client.close()
        if pending is None:
            time.sleep(0.5)
            if getattr(port, 'nrf', False):  # the bootloader starts the new application
                port.nrf = False
                try:
                    port.nrf_application()
                except OSError as e:
                    print(e, flush=True)
            port.default_lines()
        print('client closed', flush=True)


if __name__ == '__main__':
    main()
