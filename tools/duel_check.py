#!/usr/bin/env python3
"""Checkers and sea battle between two copies of the real src/Duel.cpp (no radio).

tools/ui_preview/build.sh with KEEP=PATH builds the host program; "PATH --duel" serves the games on stdin/stdout.
This check starts two of them, A and B, carries every message one sends to the other (each sees the other as
the same contact), plays whole games with random legal moves and shots, loses some messages on the way
(their sender gets a failed delivery and resends) and compares both sides after every step.

  tools/duel_check.py PROGRAM [--games N] [--seed S]
"""
import argparse
import json
import random
import subprocess
import sys

PEER = '1F00B2C3D4E5F600'  # the first contact of the preview (a chat node)


class Side:
    def __init__(self, program, name):
        self.name = name
        self.p = subprocess.Popen([program, '--duel', 'en'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def ask(self, line):
        self.p.stdin.write(line + '\n')
        self.p.stdin.flush()
        out = self.p.stdout.readline().strip()
        if not out:
            raise SystemExit(f'{self.name}: no answer to {line!r}')
        return out

    def js(self, line):
        out = self.ask(line)
        try:
            return json.loads(out)
        except ValueError:
            raise SystemExit(f'{self.name}: {line!r} -> {out!r}')

    def game(self, gid):
        return self.js(f'duel show {gid}')


def fail(text):
    print('FAIL', text)
    sys.exit(1)


def carry(src, dst, rng, loss):
    """Messages src sent: each is delivered (ACK) or lost (failed delivery, nothing arrives)."""
    n = 0
    for m in src.js('sent'):
        if rng.random() < loss:
            src.ask(f"fail {m['id']}")
            continue
        r = dst.ask(f"recv {PEER} {m['text']}")
        if r != 'OK game':
            fail(f'{dst.name} took {m["text"]!r} as {r}')
        src.ask(f"ack {m['id']}")
        n += 1
    return n


def settle(a, b, rng, loss):
    """Both directions until quiet; lost messages come back through the automatic resending."""
    for _ in range(40):
        moved = carry(a, b, rng, loss) + carry(b, a, rng, loss)
        if not moved:
            a.ask('wait 130')
            b.ask('wait 130')
            moved = carry(a, b, rng, 0) + carry(b, a, rng, 0)
            if not moved:
                return


def checkers_game(a, b, rng, loss):
    r = a.ask(f'duel invite {PEER} checkers w')
    if not r.startswith('OK game '):
        fail(r)
    gid = r.split()[2]
    settle(a, b, rng, loss)
    if b.ask(f'duel accept {gid}') != 'OK accept':
        fail('accept')
    settle(a, b, rng, loss)
    plies = 0
    while True:
        ga, gb = a.game(gid), b.game(gid)
        if ga['board'] != gb['board'] or ga['plies'] != gb['plies']:
            fail(f'boards differ after {plies}: {ga["board"]} / {gb["board"]}')
        if ga['state'] == 'over' or gb['state'] == 'over':
            break
        side, g = (a, ga) if ga['my_turn'] else (b, gb) if gb['my_turn'] else (None, None)
        if not side:
            fail(f'nobody to move at ply {ga["plies"]}: {ga} {gb}')
        if not g['legal']:
            fail('no legal move listed')
        mv = rng.choice(g['legal'])
        if side.ask(f'duel move {gid} {mv}') != 'OK move':
            fail(f'move {mv}')
        plies += 1
        settle(a, b, rng, loss)
        if plies > 400:  # random players: a draw by agreement ends it
            a.ask(f'duel draw {gid}')
            settle(a, b, rng, loss)
            b.ask(f'duel draw {gid}')
            settle(a, b, rng, loss)
    ga, gb = a.game(gid), b.game(gid)
    pair = {(ga['result'], gb['result'])}
    if pair not in ({('won', 'lost')}, {('lost', 'won')}, {('draw', 'draw')}):
        fail(f'results {ga["result"]} / {gb["result"]} ({ga["reason"]} / {gb["reason"]})')
    print(f'checkers {gid}: {plies} plies, A {ga["result"]} ({ga["reason"]})')
    for s in (a, b):
        s.ask(f'duel remove {gid}')


def sea_game(a, b, rng, loss):
    r = a.ask(f'duel invite {PEER} sea')
    if not r.startswith('OK game '):
        fail(r)
    gid = r.split()[2]
    settle(a, b, rng, loss)
    if b.game(gid)['can_shuffle'] is not True or b.ask(f'duel shuffle {gid}') != 'OK shuffle':
        fail('shuffle before the first shot')
    b.ask(f'duel accept {gid}')
    settle(a, b, rng, loss)
    shots = 0
    while True:
        ga, gb = a.game(gid), b.game(gid)
        if ga['state'] == 'over' and gb['state'] == 'over':
            break
        # What each side knows of the other must match the other's own field.
        for me, other in ((ga, gb), (gb, ga)):
            for c in range(100):
                t, o = me['theirs'][c], other['mine'][c]
                if t == 'o' and o != 'o' or t in 'xX' and o not in 'xX' or t == 'X' and o != 'X':
                    fail(f'cell {c}: shooter sees {t}, field has {o}')
        side, g = (a, ga) if ga['my_turn'] else (b, gb) if gb['my_turn'] else (None, None)
        if not side:
            fail(f'nobody to shoot after {shots}: {ga["shots"]}/{gb["shots"]} aim {ga["aim"]}/{gb["aim"]}')
        cells = [c for c in range(100) if g['theirs'][c] == '.']
        c = rng.choice(cells)
        name = 'abcdefghij'[c % 10] + str(c // 10 + 1)
        if side.ask(f'duel shoot {gid} {name}') != 'OK shoot':
            fail(f'shot {name}')
        if side.ask(f'duel shoot {gid} a1') == 'OK shoot':
            fail('a second shot before the answer')
        shots += 1
        settle(a, b, rng, loss)
    if {ga['result'], gb['result']} != {'won', 'lost'}:
        fail(f'results {ga["result"]} / {gb["result"]}')
    loser = ga if ga['result'] == 'lost' else gb
    settle(a, b, rng, 0)
    loser = a.game(gid) if ga['result'] == 'lost' else b.game(gid)
    if loser.get('fair') is not True:
        fail(f'the winner fleet was not shown or does not match: {loser}')
    print(f'sea {gid}: {shots} shots, A {ga["result"]}, fleet shown to the loser')
    for s in (a, b):
        s.ask(f'duel remove {gid}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('program')
    ap.add_argument('--games', type=int, default=3)
    ap.add_argument('--seed', type=int, default=1)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    a, b = Side(args.program, 'A'), Side(args.program, 'B')
    for i in range(args.games):
        loss = 0 if i == 0 else 0.25
        checkers_game(a, b, rng, loss)
        sea_game(a, b, rng, loss)
        a, b = b, a
    # A move of an unknown game and a repeated move change nothing.
    if a.ask(f'recv {PEER} \U0001F3C1BEEF 1 c3-d4') != 'OK game':
        fail('unknown game')
    print('OK')


if __name__ == '__main__':
    main()
