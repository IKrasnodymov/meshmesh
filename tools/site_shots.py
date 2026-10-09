#!/usr/bin/env python3
"""Screenshots of the screens for the site, in every interface language.

tools/site_shots.py OUTDIR [LANG...]   (all languages by default; needs .pio/libdeps/m9, so run pio run -e m9 first)
Writes OUTDIR/<name>-<lang>.png as site/img has them: the 320x240 screens at 2x, the 128x64 ones at 4x,
the T114's 240x135 colour screen at 2x.
tools/pages.py copies them into the site; the ones in site/img stay as the fallback.
"""
import subprocess
import sys
import tempfile
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from i18n import CODES  # noqa: E402

# Names the site shows (site/index.html HERO, OLED, GAT, FEATURES); the M9 preview writes them without "m9-".
M9 = ['radio-regions', 'channel-region', 'network-ble', 'dice-shapes', 'dice-grid', 'dice-counters', 'dice-saved', 'pet-happy', 'pet-hungry', 'pet-dead', 'threads-channels', 'channel-private', 'channel-add', 'chat-invite', 'role-boot', 'repeater', 'room', 'room-post', 'threads', 'chat', 'chat-public', 'map', 'library', 'nodes', 'node', 'radar',
      'homing', 'motion', 'chess-italian', 'chess-promotion', 'locked-chess', 'chess-rating', 'chess-pick-rated', 'chess-rated-result', 'tour-final', 'tour-new-players', 'tour-invite', 'solitaire-played', 'sensors', 'home', 'layout', 'locked', 'settings',
      'radio', 'diagnostics']
HELTEC = ['radio-region', 'ble-on', 'dice-grid', 'pet-stats', 'invite-menu', 'role', 'repeater', 'room', 'messages', 'popup', 'nodes', 'radar', 'homing', 'gps', 'settings', 'home', 'messages-menu', 'wifi-on', 'chess-piece', 'chess-move', 'chess-gamemenu', 'chess-list']
GAT562 = ['home', 'keyboard', 'chess', 'chess-list', 'radar', 'node-menu']
T114 = ['radio-find', 'ble-on', 'dice-saved', 'pet-happy', 'home', 'chess-targets', 'chess-list', 'radar', 'popup', 'nodes']


def main():
    out, langs = Path(sys.argv[1]), sys.argv[2:] or CODES
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for board, names, prefix, scale in (('m9', M9, '', 2), ('heltec', HELTEC, 'heltec-', 4), ('gat562', GAT562, 'gat562-', 4), ('t114', T114, 't114-', 2)):
            subprocess.run(['sh', str(ROOT/'tools/ui_preview/build.sh'), board, f'{tmp}/{board}', ' '.join(langs) + ' '], check=True, stdout=subprocess.DEVNULL)
            for lang in langs:
                for name in names:
                    im = Image.open(f'{tmp}/{board}/{lang}/{prefix}{name}.ppm')
                    im = im.resize((im.width*scale, im.height*scale), Image.NEAREST)
                    if board not in ('m9', 't114'):
                        im = im.convert('L')
                    site = f'{prefix or "m9-"}{name}' if prefix else f'm9-{name}'
                    im.save(out/f'{site}-{lang}.png', optimize=True)
    print(out, len(list(out.glob('*.png'))), 'screenshots')


if __name__ == '__main__':
    main()
