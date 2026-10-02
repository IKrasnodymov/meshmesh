#!/usr/bin/env python3
"""Chess between the site's companion page and an M9 on MeshMesh, over physical LoRa.

A board runs the official MeshCore companion (USB build: the Heltec as in docs/repeater.md, or the
GAT562 with upstream GAT562_30S_Mesh_Kit_companion_radio_usb). web/chess-companion.js, the code of the site's chess page, runs in Node
(tools/chess/companion_node.cjs) and reaches the companion through tools/usb_tcp_bridge.py with the
frames Web Serial carries. Game 1: the page challenges M9 and mates; a repeat and an illegal move
typed as text are ignored. Game 2: M9 challenges the page, a draw is offered on M9 and accepted on
the page. A plain message from M9 reaches the page's "other messages". Every command must be
acknowledged, positions must agree after every ply, M9 must not restart or show chess in its chat.
"""
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path
from device import connect, command
from ports import M9_PORT, HELTEC_PORT

ROOT = Path(__file__).resolve().parents[1]
TAG = '♟'
MATE = ['e2e4', 'e7e5', 'f1c4', 'b8c6', 'd1h5', 'g8f6', 'h5f7']  # White (the page) mates on move 4


def wait(what, check, timeout=120):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = check()
        if value:
            return value
        time.sleep(1)
    raise TimeoutError(what)


class Page:
    """The companion page's code in Node: JSON lines in and out."""
    def __init__(self, listen):
        self.proc = subprocess.Popen(['node', str(ROOT/'tools/chess/companion_node.cjs'), str(listen)],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.ready = json.loads(self.proc.stdout.readline() or '{}')
        if not self.ready.get('ready'):
            raise RuntimeError(f'companion page not attached: {self.ready}')
        self.n = 0

    def ask(self, what, **kw):
        self.n += 1
        self.proc.stdin.write(json.dumps({'id': self.n, 'do': what, **kw}) + '\n')
        answer = json.loads(self.proc.stdout.readline())
        if 'error' in answer:
            raise RuntimeError(f'{what}: {answer["error"]}')
        return answer['result']

    def run(self, line):
        answer = self.ask('command', line=line)
        if not str(answer).startswith('OK'):
            raise RuntimeError(f'page {line}: {answer}')
        return answer

    def game(self, gid):
        return next((g for g in self.ask('web')['games'] if g['id'] == gid), None)

    def close(self):
        self.proc.terminate()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--m9', default=M9_PORT)
    p.add_argument('--companion', default=HELTEC_PORT, help='the board with the official companion')
    p.add_argument('--listen', type=int, default=8772)
    p.add_argument('--output', type=Path, default=ROOT/'artifacts/chess-companion-check.json')
    a = p.parse_args()
    bridge = subprocess.Popen([sys.executable, str(ROOT/'tools/usb_tcp_bridge.py'), '--port', a.companion, '--listen', str(a.listen), '--raw'],
                              stdout=subprocess.PIPE, text=True)
    page = None
    try:
        if not bridge.stdout.readline().startswith('tcp://'):
            raise RuntimeError('bridge did not start')
        page = Page(a.listen)
        stock = page.ready['self']
        with connect(a.m9) as m9:
            read = lambda name: json.loads(command(m9, name))  # noqa: E731

            def ok(line):
                answer = command(m9, line)
                if not answer.startswith('OK'):
                    raise RuntimeError(f'M9 {line}: {answer}')
                return answer

            def m9_game(gid):
                return next((g for g in read('chess') if g['id'] == gid), None)

            def chess_texts():
                return sum(TAG in m['text'] for m in read('messages'))

            before = read('status')
            texts = chess_texts()
            m9_prefix = before['public_key'][:12].upper()
            # Both sides must know each other: adverts by flood from both.
            ok('hello')
            page.ask('advert')
            wait('the page knows M9', lambda: any(x['id'] == m9_prefix and x['type'] == 1 for x in page.ask('peers')), 180)
            m9_node = wait('M9 knows the companion', lambda: next((n['id'] for n in read('nodes')
                                                                    if n.get('public_key', '').lower() == stock['key'].lower()), None), 180)
            report = {'companion': {'name': stock['name'], 'firmware': page.ready['device']['version'], 'model': page.ready['device']['model'],
                                    'radio': f"{stock['freq']} MHz SF{stock['sf']} BW{stock['bw']} CR4/{stock['cr']}"},
                      'm9_before': {k: before[k] for k in ('node', 'boot', 'rx', 'tx', 'firmware')}}

            # Game 1: the page challenges M9, plays White and mates.
            gid = page.run(f'chess invite {m9_prefix} w').split()[2]
            wait('challenge ACK on the page', lambda: page.game(gid)['out_status'] == 3)
            wait('M9 received the challenge', lambda: (g := m9_game(gid)) and g['state'] == 'invited')
            ok(f'chess accept {gid}')
            wait('the page saw the acceptance', lambda: page.game(gid)['state'] == 'playing')
            positions = []
            for ply, move in enumerate(MATE, 1):
                if ply % 2:
                    page.run(f'chess move {gid} {move}')
                    wait(f'ply {ply}: ACK on the page', lambda: page.game(gid)['out_status'] == 3)
                    wait(f'ply {ply} on M9', lambda: m9_game(gid)['plies'] == ply)
                else:
                    ok(f'chess move {gid} {move}')
                    wait(f'ply {ply}: ACK on M9', lambda: m9_game(gid)['out_status'] == 3)
                    wait(f'ply {ply} on the page', lambda: page.game(gid)['plies'] == ply)
                fens = m9_game(gid)['fen'], page.game(gid)['fen']
                assert fens[0] == fens[1], f'positions differ after {move}: {fens}'
                positions.append(fens[0])
                if ply in (1, 2):
                    # Typed by hand on the companion: a repeat of ply 1, then an illegal ply 3.
                    text = f'{TAG}{gid} 1 e2e4' if ply == 1 else f'{TAG}{gid} 3 e1e3'
                    page.ask('raw', peer=m9_prefix, text=text)
                    time.sleep(12)
                    assert m9_game(gid)['plies'] == ply and m9_game(gid)['fen'] == positions[-1], 'M9 took a repeated or illegal move'
            for g in (m9_game(gid), page.game(gid)):
                assert g['state'] == 'over' and g['result'] == 'white' and g['reason'] == 'mate', g
            report['game1'] = {'id': gid, 'challenge': 'page → M9', 'moves': MATE, 'positions': positions, 'result': 'the page (White) mates',
                               'ignored_on_m9': ['repeat of ply 1', 'illegal e1e3']}

            # Game 2: M9 challenges the page, M9 offers a draw, the page agrees.
            gid2 = ok(f'chess invite {m9_node} w').split()[2]
            wait('M9 challenge ACK', lambda: m9_game(gid2)['out_status'] == 3)
            wait('the page received the challenge', lambda: (g := page.game(gid2)) and g['state'] == 'invited')
            page.run(f'chess accept {gid2}')
            wait('M9 saw the acceptance', lambda: m9_game(gid2)['state'] == 'playing')
            ok(f'chess move {gid2} d2d4')
            wait('d4 on the page', lambda: page.game(gid2)['plies'] == 1)
            page.run(f'chess move {gid2} d7d5')
            wait('d5 on M9', lambda: m9_game(gid2)['plies'] == 2)
            ok(f'chess draw {gid2}')
            wait('draw offer on the page', lambda: page.game(gid2)['draw_offer'] == 'theirs')
            page.run(f'chess draw {gid2}')
            wait('draw on M9', lambda: m9_game(gid2)['state'] == 'over')
            for g in (m9_game(gid2), page.game(gid2)):
                assert g['result'] == 'draw' and g['reason'] == 'agreed', g
            assert m9_game(gid2)['fen'] == page.game(gid2)['fen']
            report['game2'] = {'id': gid2, 'challenge': 'M9 → page', 'moves': ['d2d4', 'd7d5'], 'result': 'draw agreed (offer on M9)'}

            # A plain message is not chess: it is kept for the page's "other messages".
            text = f'MeshMesh chess check {int(time.time())}'
            ok(f'send {m9_node} {text}')
            wait('plain message on the page', lambda: any(o['text'] == text for o in page.ask('self')['others']))
            others = [o['text'] for o in page.ask('self')['others']]
            assert others.count(text) == 1, 'one copy of the plain message'
            assert not any(TAG in t for t in others), 'chess commands among other messages'

            after = read('status')
            assert after['boot'] == before['boot'], 'M9 restarted'
            assert after['rx'] > before['rx'] and after['tx'] > before['tx'], 'no radio traffic on M9'
            assert chess_texts() == texts, 'chess commands reached the M9 chat'
            for g in (gid, gid2):
                ok(f'chess remove {g}')
                page.run(f'chess remove {g}')
            report.update({'result': 'passed', 'plain_message': 'one copy among the page\'s other messages', 'm9_chat_chess_copies': 0,
                           'm9_after': {k: after[k] for k in ('node', 'boot', 'rx', 'tx')}})
    finally:
        if page:
            page.close()
        bridge.terminate()
    a.output.parent.mkdir(exist_ok=True)
    a.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(f'PASS companion chess: {gid} page mates M9, {gid2} draw agreed, repeat/illegal ignored, plain message kept')


if __name__ == '__main__':
    main()
