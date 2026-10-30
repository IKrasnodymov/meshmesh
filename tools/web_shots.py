#!/usr/bin/env python3
"""Screenshots of the device page for the site, in every language: tools/web_demo.py with made-up data in a
headless Chromium.

tools/web_shots.py OUTDIR [LANG...]   (all 15 languages by default)
Writes OUTDIR/web-<name>-<lang>.webp, the phone at 390x844 CSS pixels x2 and the desktop at 1280x800 x2.
Chromium: $CHROME, otherwise google-chrome / chromium on the PATH or Playwright's headless shell.
tools/pages.py copies them into the site; the ones in site/img (ru, en) stay as the fallback.
"""
import glob
import os
import shutil
import subprocess
import sys
import tempfile
import threading
from pathlib import Path
from urllib.parse import quote

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import web_demo  # noqa: E402

LANGS = ['en', 'ru'] + web_demo.LANGS[1:]
ANNA = next(m for m in web_demo.messages('en') if m['name'] == 'Anna' and m['destination'] == 'FFFFFFFFFFFFFFFF')
MINE = next(m for m in web_demo.messages('en') if m['outgoing'] and m.get('echoes'))
key = lambda m: f"{m['source']}.{m['session']}.{m['id']}"  # msgKey() of the page
# name: (route, extra query, script once the data is shown, desktop)
SHOTS = {
    'web-home': ('home', '', '', False),
    'web-chat': ('chat/B0B5A11E7C42D9E3', '', '', False),
    'web-public': ('chat/ALL', '', '', False),
    'web-channel-qr': ('chinfo/FF9C8B7A6D5E4F30', '', "const q=document.querySelector('.qrbox').closest('.card').previousElementSibling;for(let e=q.previousElementSibling;e&&e.previousElementSibling;e=e.previousElementSibling)e.style.display='none'", False),
    'web-channel-add': ('addch', '', '', False),
    'web-nodes': ('nodes', '', '', False),
    'web-map': ('map', '', '', False),
    'web-radar': ('radar', '', '', False),
    'web-homing': ('homing', 'homing=1', '', False),
    'web-motion': ('motion', 'csi=1', '', False),
    'web-board': ('board/3F2A', '', '', False),
    'web-radio': ('radio', '', '', False),
    # The networks catalogue: a network chosen in the list, then a test that runs (state of the page only).
    'web-networks': ('radio', '', "netAction('open').then(()=>{netPick.net='mow-01';renderNetList()})", False),
    'web-networks-test': ('radio', '', "config.radio_trial={frequency:config.frequency,bandwidth:config.bandwidth,sf:config.sf,cr:config.cr};Object.assign(config,{frequency:868.731,sf:7,cr:7});netPick.check={n:3};buildEditor('radio');renderNet(true)", False),
    # The network search of the board: the answers of the demo (tools/web_demo.py NETFIND).
    'web-netfind': ('radio', '', "netScanLoad().then(()=>{$('netScan').scrollIntoView({block:'start'});scrollBy(0,-120)})", False),
    'web-msg-path': ('msg/' + key(ANNA), '', '', False),
    'web-msg-echoes': ('msg/' + key(MINE), '', '', False),
    'web-alerts-feed': ('alerts', '', '', False),
    'web-alerts-settings': ('alerts', '', "alertTab='settings';renderAlerts()", False),
    'web-desktop': ('chat/ALL', '', '', True),
}


def chrome():
    for c in [os.environ.get('CHROME'), shutil.which('google-chrome'), shutil.which('chromium'), shutil.which('chromium-browser'),
              *sorted(glob.glob(os.path.expanduser('~/Library/Caches/ms-playwright/chromium_headless_shell-*/*/chrome-headless-shell'))),
              *sorted(glob.glob(os.path.expanduser('~/.cache/ms-playwright/chromium_headless_shell-*/*/chrome-headless-shell')))]:
        if c and os.path.exists(c):
            return c
    raise SystemExit('No Chromium: set CHROME')


def main():
    out, langs = Path(sys.argv[1]), sys.argv[2:] or LANGS
    out.mkdir(parents=True, exist_ok=True)
    server = web_demo.serve(0)
    port = server.server_address[1]
    threading.Thread(target=server.serve_forever, daemon=True).start()
    browser = chrome()
    with tempfile.TemporaryDirectory() as tmp:
        for lang in langs:
            for name, (route, query, run, desktop) in SHOTS.items():
                url = f'http://127.0.0.1:{port}/?lang={lang}' + (f'&{query}' if query else '')
                if run:
                    url += '&run=' + quote(run, safe='')
                png = Path(tmp)/'shot.png'
                size = '1280,800' if desktop else '390,844'
                # A fresh profile each time: nothing kept between languages (the page's language is in localStorage).
                subprocess.run([browser, '--headless', '--disable-gpu', '--no-sandbox', '--hide-scrollbars', '--no-first-run', f'--user-data-dir={tmp}/profile-{lang}-{name}',
                                f'--window-size={size}', '--force-device-scale-factor=2', '--virtual-time-budget=9000',
                                f'--screenshot={png}', url + '#' + route], check=True, capture_output=True, timeout=120)
                Image.open(png).convert('RGB').save(out/f'{name}-{lang}.webp', quality=82, method=6)
            print(lang, len(SHOTS), 'pages')
    server.shutdown()


if __name__ == '__main__':
    main()
