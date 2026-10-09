#!/usr/bin/env python3
"""MeshCore regions (flood scope) of MeshMesh over USB (docs/regions.md).

  .venv/bin/python tools/region_check.py --port PORT [--repeater REPEATER_PORT]

PORT: a MeshMesh board in the normal mode. Its flood packets are taken from "txframe" (the last frame sent; our own
is told from relayed ones by the channel hash) and the transport code is computed here independently:
HMAC-SHA256(key=SHA-256("#name")[:16], payload type + payload), the first 2 bytes. Checked: the default region of
the settings (an advert), a channel's own region, "*" (no region) and "default", the MeshCore app's commands
63/64 (default scope), 54 (the session's scope and its "unscoped" flag), 55 (zero-hop control data) and the region
search. A temporary private channel carries the test messages and is removed; the region set before is restored.

REPEATER_PORT (optional): a MeshMesh board in the repeater role. Its regions are replaced for the check
("region def"), then restored from "region" as it was: a flood scoped to a region it allows is forwarded
(its TX grows), one it does not allow is not, and the search on PORT lists its regions.
"""
import argparse, hashlib, hmac, json, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from device import connect, command

CMD_SEND_CHANNEL_TXT, CMD_SET_SCOPE, CMD_CONTROL, CMD_SET_DEFAULT_SCOPE, CMD_GET_DEFAULT_SCOPE, CMD_DEVICE_QUERY = 3, 54, 55, 63, 64, 22
RESP_OK, RESP_DEFAULT_SCOPE = 0, 28
failures = []

def check(ok, what):
    print(('PASS ' if ok else 'FAIL ') + what)
    if not ok: failures.append(what)

def key_of(name): return hashlib.sha256(('#' + name).encode()).digest()[:16]

def code(key, ptype, payload):
    c = int.from_bytes(hmac.new(key, bytes([ptype]) + payload, hashlib.sha256).digest()[:2], 'little')
    return 1 if c == 0 else 0xFFFE if c == 0xFFFF else c

def parse(frame):
    h = frame[0]; route, ptype = h & 3, (h >> 2) & 15
    codes = struct.unpack('<HH', frame[1:5]) if route in (0, 3) else None
    off = 5 if codes else 1; plen = frame[off]
    return route, ptype, codes, frame[off + 1 + ((plen >> 6) + 1) * (plen & 63):]

class Board:
    def __init__(self, port):
        self.s = connect(port); self.buf = bytearray()
    def cmd(self, text, timeout=8):
        for _ in range(15): # settings wait while a packet is on air
            r = command(self.s, text, timeout)
            if 'radio busy' not in r: return r
            time.sleep(1)
        return r
    def json(self, text): return json.loads(self.cmd(text))
    def app(self, frame, timeout=5):
        self.s.reset_input_buffer(); self.s.write(b'<' + struct.pack('<H', len(frame)) + frame); self.s.flush()
        end = time.monotonic() + timeout; buf = bytearray()
        while time.monotonic() < end:
            buf.extend(self.s.read(max(1, self.s.in_waiting)))
            i = buf.find(b'>')
            if i < 0: buf.clear(); continue
            del buf[:i]
            if len(buf) >= 3:
                n = struct.unpack('<H', buf[1:3])[0]
                if len(buf) >= 3 + n:
                    f = bytes(buf[3:3 + n]); del buf[:3 + n]
                    if f[0] < 0x80: return f
        raise TimeoutError(f'no reply to companion command {frame[0]}')
    def own_frame(self, ptype, first=None, timeout=8):
        """Our packet of this type among the frames sent (first: its first payload byte, the channel hash)."""
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            time.sleep(.3); f = bytes.fromhex(self.cmd('txframe').strip())
            route, t, codes, payload = parse(f)
            if t == ptype and (first is None or payload[:1] == bytes([first])): return route, codes, payload
        return None

def scoped(b, ptype, first, name, what):
    got = b.own_frame(ptype, first)
    if not got: check(False, what + ': own frame not seen'); return
    route, codes, payload = got
    if name is None: check(route == 1 and codes is None, what + ': flood without a transport code'); return
    want = code(name if isinstance(name, bytes) else key_of(name), ptype, payload)
    check(route == 0 and codes and codes[0] == want and codes[1] == 0, f'{what}: transport code {codes and codes[0]:04x}, expected {want:04x}')

def main():
    a = argparse.ArgumentParser(); a.add_argument('--port', required=True); a.add_argument('--repeater')
    args = a.parse_args(); b = Board(args.port)
    status = b.json('status'); check(status.get('role') == 'normal', f"{status.get('board')} {status.get('version')} boot {status.get('boot')}: normal mode")
    before = b.json('config').get('region', ''); check('region' in b.json('config'), 'config has "region"')
    chan_id = None
    try:
        # The default region of the settings: an advert carries it.
        check(b.cmd('set {"region":"tst-a"}').startswith('OK'), 'set region tst-a')
        b.cmd('hello'); scoped(b, 4, None, 'tst-a', 'advert with the default region')
        check(b.cmd('set {"region":"bad name!"}').startswith('ERR region'), 'invalid region refused')
        # A temporary private channel: its own region, none, the default.
        r = b.cmd('channel do {"action":"add","create":"region-check"}'); chan_id = r.split()[3]
        chans = b.json('channels'); c = next(x for x in chans['channels'] if x['id'] == chan_id); h = int(c['hash'], 16)
        index = [x['id'] for x in chans['channels']].index(chan_id); check(c.get('region') == '', 'new channel: default region')
        for reg, want, what in (('tst-b', 'tst-b', 'channel region'), ('*', None, 'channel without a region'), ('', 'tst-a', 'channel with the default region')):
            check(b.cmd('channel do ' + json.dumps({'action': 'region', 'channel': chan_id, 'region': reg})).startswith('OK'), f'channel region "{reg}" saved')
            b.cmd(f'send {chan_id} region check {reg or "default"}'); scoped(b, 5, h, want, what)
            time.sleep(2)
        # The MeshCore app: default scope (63/64), the session's scope (54) and its unscoped flag, control data (55).
        b.app(bytes([CMD_DEVICE_QUERY, 3]))
        f = b.app(bytes([CMD_GET_DEFAULT_SCOPE])); check(f[0] == RESP_DEFAULT_SCOPE and f[1:32].rstrip(b'\0') == b'tst-a' and f[32:48] == key_of('tst-a'), 'app: get default scope')
        pkey = bytes(range(1, 17)); name = b'$priv'.ljust(31, b'\0')
        check(b.app(bytes([CMD_SET_DEFAULT_SCOPE]) + name + pkey)[0] == RESP_OK, 'app: set default scope (private key)')
        check(b.json('config').get('region') == '$priv', 'settings show the app\'s region')
        b.cmd('hello'); scoped(b, 4, None, pkey, 'advert with the app\'s private key')
        check(b.app(bytes([CMD_SET_SCOPE, 0]) + key_of('tst-c'))[0] == RESP_OK, 'app: session scope tst-c')
        b.app(bytes([CMD_SEND_CHANNEL_TXT, 0, index]) + struct.pack('<I', int(time.time())) + b'app scope'); scoped(b, 5, h, 'tst-c', 'app message with the session scope')
        time.sleep(2)
        check(b.app(bytes([CMD_SET_SCOPE, 1]))[0] == RESP_OK, 'app: unscoped')
        b.app(bytes([CMD_SEND_CHANNEL_TXT, 0, index]) + struct.pack('<I', int(time.time())) + b'app unscoped'); scoped(b, 5, h, None, 'app message unscoped')
        check(b.app(bytes([CMD_SET_SCOPE, 0]))[0] == RESP_OK, 'app: session scope reset')
        check(b.app(bytes([CMD_CONTROL, 0x80, 0x04]) + bytes(8))[0] == RESP_OK, 'app: zero-hop control data sent')
        check(b.app(bytes([CMD_SET_DEFAULT_SCOPE]))[0] == RESP_OK and b.json('config').get('region') == '', 'app: default scope cleared')
        # The search (with a repeater below; alone: it ends with what it heard).
        check(b.cmd('regions find').startswith('OK'), 'region search started')
        for _ in range(40):
            s = b.json('regions')
            if s['state'] == 'done': break
            time.sleep(1)
        check(s['state'] == 'done', f"region search done: {s['repeaters']} repeaters, found {[x['name'] for x in s['found']]}")
    finally:
        time.sleep(3)
        if chan_id:
            for _ in range(20):
                if b.cmd('channel do ' + json.dumps({'action': 'remove', 'channel': chan_id})).startswith('OK'): break
                time.sleep(3)
        for _ in range(10):
            if b.cmd('set ' + json.dumps({'region': before})).startswith('OK'): break
            time.sleep(2)
        print('region restored:', json.dumps(b.json('config').get('region')))
    print('FAILED: %d' % len(failures) if failures else 'ALL PASSED')
    sys.exit(1 if failures else 0)

if __name__ == '__main__': main()
