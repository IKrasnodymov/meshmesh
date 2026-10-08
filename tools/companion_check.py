#!/usr/bin/env python3
"""MeshCore companion protocol of MeshMesh over USB frames (docs/companion.md), against a second MeshMesh board.

  .venv/bin/python tools/companion_check.py --port APP_PORT --peer PEER_PORT

APP_PORT is driven as a MeshCore app would drive it ('<' + length + frame); PEER_PORT gets text commands.
The check: device and self info, the peer in the contact list (adverts if needed), a direct message from
the app with its ACK, one copy on the peer, a direct and a channel message from the peer synced by the app,
and text commands working again on APP_PORT. Keys are not printed.
"""
import argparse, json, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from device import connect, command

CMD_APP_START, CMD_SEND_TXT, CMD_GET_CONTACTS, CMD_SEND_ADVERT, CMD_SYNC_NEXT, CMD_DEVICE_QUERY = 1, 2, 4, 7, 10, 22
RESP_OK, RESP_ERR, RESP_CONTACTS_START, RESP_CONTACT, RESP_CONTACTS_END, RESP_SELF_INFO, RESP_SENT = 0, 1, 2, 3, 4, 5, 6
RESP_NO_MORE, RESP_DEVICE_INFO, RESP_CONTACT_MSG, RESP_CHANNEL_MSG = 10, 13, 16, 17
PUSH_ADVERT, PUSH_SEND_CONFIRMED, PUSH_MSG_WAITING, PUSH_NEW_ADVERT = 0x80, 0x82, 0x83, 0x8A

class App:
    def __init__(self, port):
        self.s = connect(port); self.buf = bytearray(); self.pushes = []
    def send(self, frame):
        self.s.write(b'<' + struct.pack('<H', len(frame)) + frame); self.s.flush()
    def frame(self, timeout):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.buf.extend(self.s.read(max(1, self.s.in_waiting)))
            i = self.buf.find(b'>')
            if i < 0: self.buf.clear(); continue
            del self.buf[:i]
            if len(self.buf) >= 3:
                n = struct.unpack('<H', self.buf[1:3])[0]
                if len(self.buf) >= 3 + n:
                    f = bytes(self.buf[3:3 + n]); del self.buf[:3 + n]; return f
        return None
    def call(self, frame, timeout=5):
        """The reply to one command; pushes on the way are kept."""
        self.send(frame); end = time.monotonic() + timeout
        while time.monotonic() < end:
            f = self.frame(end - time.monotonic())
            if f is None: break
            if f[0] >= 0x80: self.pushes.append(f); continue
            return f
        raise TimeoutError(f'no reply to command {frame[0]}')
    def push(self, code, timeout, match=lambda f: True):
        end = time.monotonic() + timeout
        while True:
            for f in self.pushes:
                if f[0] == code and match(f): self.pushes.remove(f); return f
            left = end - time.monotonic()
            if left <= 0: return None
            f = self.frame(left)
            if f is not None: self.pushes.append(f)
    def contacts(self):
        f = self.call(bytes([CMD_GET_CONTACTS])); assert f[0] == RESP_CONTACTS_START, f
        found = {}
        while True:
            f = self.frame(5)
            if f is None: raise TimeoutError('contact list')
            if f[0] >= 0x80: self.pushes.append(f); continue
            if f[0] == RESP_CONTACTS_END: return found
            assert f[0] == RESP_CONTACT, f
            found[f[1:33]] = (f[33], f[100:132].split(b'\0')[0].decode('utf-8', 'replace'))
    def sync(self):
        got = []
        while True:
            f = self.call(bytes([CMD_SYNC_NEXT]))
            if f[0] == RESP_NO_MORE: return got
            got.append(f)

def main():
    a = argparse.ArgumentParser(); a.add_argument('--port', required=True); a.add_argument('--peer', required=True)
    a.add_argument('--timeout', type=float, default=60); args = a.parse_args()
    result = {'checks': {}}; ok = result['checks']
    peer = connect(args.peer); ps = json.loads(command(peer, 'status'))
    peer_key = bytes.fromhex(ps['public_key']); peer_id = ps['node']
    app = App(args.port)
    info = app.call(bytes([CMD_DEVICE_QUERY, 3])); ok['device_info'] = info[0] == RESP_DEVICE_INFO and info[1] >= 3
    result['firmware'] = info[60:80].split(b'\0')[0].decode()
    me = app.call(bytes([CMD_APP_START]) + bytes(7) + b'companion_check'); ok['self_info'] = me[0] == RESP_SELF_INFO
    my_key = me[4:36]; my_id = my_key[:8].hex().upper()
    app.sync()  # older frames of the inbox
    if peer_key not in app.contacts():
        command(peer, 'hello'); app.call(bytes([CMD_SEND_ADVERT, 1]))
        app.push(PUSH_NEW_ADVERT, args.timeout, lambda f: f[1:33] == peer_key) or app.push(PUSH_ADVERT, 1, lambda f: f[1:33] == peer_key)
    contacts = app.contacts(); ok['peer_contact'] = peer_key in contacts
    if not ok['peer_contact']: print(json.dumps(result, ensure_ascii=False, indent=1)); return 1
    # A direct message from the app: RESP_SENT with the expected ACK, then PUSH_SEND_CONFIRMED with it.
    text = f'companion check {int(time.time())}'
    f = app.call(bytes([CMD_SEND_TXT, 0, 0]) + struct.pack('<I', int(time.time())) + peer_key[:6] + text.encode())
    ok['sent'] = f[0] == RESP_SENT
    if ok['sent']:
        ack = f[2:6]; result['flood'] = bool(f[1]); result['timeout_ms'] = struct.unpack('<I', f[6:10])[0]
        c = app.push(PUSH_SEND_CONFIRMED, args.timeout, lambda x: x[1:5] == ack); ok['ack'] = c is not None
        if c: result['round_trip_ms'] = struct.unpack('<I', c[5:9])[0]
    time.sleep(1); peer_msgs = json.loads(command(peer, 'messages', 15))
    ok['one_copy_on_peer'] = sum(1 for m in peer_msgs if m['text'] == text and not m['outgoing']) == 1
    # A direct and a channel message from the peer, synced by the app.
    reply = f'reply {int(time.time())}'; chan = f'public {int(time.time())}'
    command(peer, f'send {my_id} {reply}')
    app.push(PUSH_MSG_WAITING, args.timeout); got = app.sync()
    ok['direct_received'] = any(g[0] == RESP_CONTACT_MSG and g[4:10] == peer_key[:6] and g[16:].decode('utf-8', 'replace') == reply for g in got)
    # The channel message waits for the ACK of the reply: a node sending its ACK does not hear the air.
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        delivered = [m for m in json.loads(command(peer, 'messages', 15)) if m['text'] == reply and m['outgoing']]
        if delivered and delivered[-1]['status'] == 3: break
        time.sleep(2)
    ok['peer_got_ack'] = bool(delivered) and delivered[-1]['status'] == 3
    command(peer, f'send ALL {chan}')
    app.push(PUSH_MSG_WAITING, args.timeout); got = app.sync()
    ok['channel_received'] = any(g[0] == RESP_CHANNEL_MSG and g[4] == 0 and g[11:].decode('utf-8', 'replace').endswith(chan) for g in got)
    # Text commands work again on the app's port.
    app.s.close(); s = connect(args.port); st = json.loads(command(s, 'status'))
    ok['text_after_frames'] = st['node'] == my_id; result['app_rx_tx'] = [st['rx'], st['tx']]
    print(json.dumps(result, ensure_ascii=False, indent=1))
    return 0 if all(ok.values()) else 1

if __name__ == '__main__':
    sys.exit(main())
