#!/usr/bin/env python3
"""Play chess between M9 and Heltec over physical LoRa (MeshCore direct messages with ACK).

Heltec challenges M9, M9 accepts on its production screen (USB key events), the boards play
a short game to checkmate and must agree on every position. A repeated and an illegal move sent
as plain text must be ignored, and no chess command may appear in either chat history.
The test game is removed from both boards afterwards; other games are left untouched.
"""
import argparse
import json
import time
from pathlib import Path
from device import connect, command
from ports import M9_PORT, HELTEC_PORT

TAG = '♟'
MOVES = ['e2e4', 'e7e5', 'f1c4', 'b8c6', 'd1h5', 'g8f6', 'h5f7']  # White (Heltec) mates on move 4


def read(device, name):
    return json.loads(command(device, name))


def game(device, gid):
    found = [g for g in read(device, 'chess') if g['id'] == gid]
    return found[0] if found else None


def wait(what, check, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = check()
        if value:
            return value
        time.sleep(1)
    raise TimeoutError(what)


def ok(device, line):
    answer = command(device, line)
    if not answer.startswith('OK'):
        raise RuntimeError(f'{line}: {answer}')
    return answer


def delivered(device, gid):
    return wait(f'{gid}: ACK', lambda: (g := game(device, gid)) and g['out_status'] == 3 and g)


def chess_texts(device):
    return sum(TAG in m['text'] for m in read(device, 'messages'))


def accept_on_screen(m9, gid):
    """Open the challenge from the home tile and accept it with OK, as a person would."""
    if read(m9, 'ui')['locked']:
        ok(m9, 'uikey 0xa3')
    for key in [0x82, 0x82, 0xb6, 0xb6, 0xb6, 13]:  # a dark screen takes the first key to wake
        ok(m9, f'uikey {key}')
    for _ in range(8):
        ok(m9, 'uikey 0xb6')
        ok(m9, 'uikey 13')
        if read(m9, 'ui').get('chess_game') == gid:
            break
        ok(m9, 'uikey 0x86')
    else:
        raise AssertionError('The challenge is not in the M9 list of games')
    ok(m9, 'uikey 13')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--m9', default=M9_PORT)
    p.add_argument('--heltec', default=HELTEC_PORT)
    p.add_argument('--output', type=Path, default=Path('artifacts/chess-check.json'))
    a = p.parse_args()
    with connect(a.m9) as m9, connect(a.heltec) as heltec:
        before = [read(d, 'status') for d in (m9, heltec)]
        m9_id, heltec_id = before[0]['node'], before[1]['node']
        texts = [chess_texts(d) for d in (m9, heltec)]
        gid = ok(heltec, f'chess invite {m9_id} w').split()[2]
        delivered(heltec, gid)
        wait('M9 received the challenge', lambda: (g := game(m9, gid)) and g['state'] == 'invited')
        accept_on_screen(m9, gid)
        assert game(m9, gid)['state'] == 'playing'
        wait('Heltec saw the acceptance', lambda: game(heltec, gid)['state'] == 'playing')
        delivered(m9, gid)
        positions = []
        for ply, move in enumerate(MOVES, 1):
            sender, receiver = (heltec, m9) if ply % 2 else (m9, heltec)
            ok(sender, f'chess move {gid} {move}')
            delivered(sender, gid)
            wait(f'ply {ply} arrived', lambda: game(receiver, gid)['plies'] == ply)
            a_fen, b_fen = game(m9, gid)['fen'], game(heltec, gid)['fen']
            assert a_fen == b_fen, f'positions differ after {move}: {a_fen} / {b_fen}'
            positions.append(a_fen)
            if ply in (1, 2):
                # Typed as plain text: a repeat of ply 1 (as after a lost ACK), then an illegal ply 3.
                text = f'{TAG}{gid} 1 e2e4' if ply == 1 else f'{TAG}{gid} 3 e1e3'
                ok(heltec, f'send {m9_id} {text}')
                wait(f'{text}: ACK', lambda: [m for m in read(heltec, 'messages') if m['outgoing'] and m['text'] == text and m['status'] == 3])
                assert game(m9, gid)['plies'] == ply and game(m9, gid)['fen'] == positions[-1], 'M9 took a repeated or illegal move'
        ends = [game(d, gid) for d in (m9, heltec)]
        for g in ends:
            assert g['state'] == 'over' and g['result'] == 'white' and g['reason'] == 'mate', g
        # The two plain-text probes are Heltec's own chat messages; chess commands never are.
        assert chess_texts(m9) == texts[0], 'chess commands reached the M9 chat'
        assert chess_texts(heltec) == texts[1] + 2, 'chess commands reached the Heltec chat'
        for d in (m9, heltec):
            ok(d, f'chess remove {gid}')
        after = [read(d, 'status') for d in (m9, heltec)]
        for b, c in zip(before, after):
            assert b['boot'] == c['boot'], 'a board restarted during the check'
            assert c['rx'] > b['rx'] and c['tx'] > b['tx'], 'no radio traffic counted'
        report = {'result': 'passed', 'game': gid, 'moves': MOVES, 'positions': positions,
                  'result_on_both': 'white mates', 'ignored': ['repeat of ply 1', 'illegal e1e3'],
                  'accepted_on_m9_screen': True, 'chat_copies': 0,
                  'before': [{'node': s['node'], 'boot': s['boot'], 'rx': s['rx'], 'tx': s['tx']} for s in before],
                  'after': [{'node': s['node'], 'boot': s['boot'], 'rx': s['rx'], 'tx': s['tx']} for s in after]}
    a.output.parent.mkdir(exist_ok=True)
    a.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(f'PASS chess {gid}: challenge, screen accept, {len(MOVES)} plies to mate, repeat/illegal ignored')


if __name__ == '__main__':
    main()
