#!/usr/bin/env python3
"""Serve web/index.html on this computer and answer its /api calls with USB commands of a board.

For checking the web page in a desktop or headless browser without joining the device's Wi-Fi:
GET APIs map to the same JSON as the device's HTTP server, POST /api/command runs the USB command
line, so actions really go out over LoRa. Any password is accepted. The device's own HTTP server
and Wi-Fi are checked separately by wifi_probe.py. Usage: web_usb_bridge.py --port PORT [--http 8090]
"""
import argparse
import base64
import json
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlparse, parse_qs
from device import connect, command

ROOT = Path(__file__).resolve().parents[1]
GETS = {'/api/status': 'status', '/api/messages': 'messages', '/api/nodes': 'nodes', '/api/config': 'config',
        '/api/navigation': 'navigation', '/api/maps': 'map info', '/api/maps/areas': 'map areas',
        '/api/clock': 'clock', '/api/key': 'key'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True)
    p.add_argument('--http', type=int, default=8090)
    p.add_argument('--host', default='127.0.0.1')
    a = p.parse_args()
    device = connect(a.port)
    lock = threading.Lock()

    def usb(line):
        with lock:
            return command(device, line, timeout=12)

    class Handler(BaseHTTPRequestHandler):
        def reply(self, code, body, kind='application/json'):
            data = body.encode() if isinstance(body, str) else body
            self.send_response(code)
            self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            url = urlparse(self.path)
            try:
                if url.path == '/':
                    self.reply(200, (ROOT / 'web/index.html').read_bytes(), 'text/html; charset=utf-8')
                elif url.path == '/api/chess':
                    query = parse_qs(url.query);game = query.get('id', [''])[0]
                    self.reply(200, usb('chess rating') if 'rating' in query else usb(f'chess show {game}') if game else usb('chess web'))
                elif url.path == '/api/connections':
                    self.reply(200, usb('connections'))
                elif url.path == '/api/radar':
                    self.reply(200, usb('radar web'))  # holds the radar open, as the device's own page does
                elif url.path == '/api/maps/tile':
                    q = parse_qs(url.query)
                    z, x, y = (q.get(k, [''])[0] for k in 'zxy')
                    data, size = b'', None
                    while size is None or len(data) < size:
                        answer = usb(f'map tile {z} {x} {y} {len(data)} 6144')
                        if not answer.startswith('OK tile '):
                            self.reply(404, 'Map tile not saved', 'text/plain')
                            return
                        parts = answer.split(' ', 4)
                        size = int(parts[2])
                        data += base64.b64decode(parts[4] if len(parts) > 4 else '')
                    self.reply(200, data, 'application/octet-stream')
                elif url.path in GETS:
                    self.reply(200, usb(GETS[url.path]))
                else:
                    self.reply(404, 'Not found', 'text/plain')
            except Exception as e:  # report the failed USB command to the page
                self.reply(400, str(e), 'text/plain')

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers.get('Content-Length', 0))) or b'{}')
            path = urlparse(self.path).path
            line = (body.get('command') if path == '/api/command' else
                    'sendjson ' + json.dumps({'to': body.get('to'), 'text': body.get('text')}, ensure_ascii=False) if path == '/api/send' else
                    'set ' + json.dumps(body, ensure_ascii=False) if path == '/api/config' else
                    'radar do ' + json.dumps(body) if path == '/api/radar' else None)
            if not line:
                self.reply(404, 'Not found', 'text/plain')
                return
            try:
                answer = usb(line)
            except Exception as e:
                answer = 'ERR ' + str(e)
            ok = answer.startswith('OK') if path != '/api/command' else not answer.startswith('ERR')
            self.reply(200 if ok else 400, answer)  # raw text and status code, as the device sends them

        def log_message(self, *args):
            pass

    print(f'http://{a.host}:{a.http}/ -> {a.port}', flush=True)
    HTTPServer((a.host, a.http), Handler).serve_forever()


if __name__ == '__main__':
    main()
