#!/usr/bin/env python3
"""Build the site's chess page for a stock MeshCore companion: OUTDIR/chess/.

tools/chess_site.py OUTDIR   (tools/pages.py calls it for GitHub Pages)
The page is web/index.html, the boards' own page, with web/chess-companion.js: the rules, the games
and the companion link run in the browser (docs/chess.md). Web Serial and Web Bluetooth need HTTPS,
so the page works from the site, not from a board's access point.
"""
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def build(out):
    target = Path(out)/'chess'
    target.mkdir(parents=True, exist_ok=True)
    page = (ROOT/'web/index.html').read_text()
    for marker in ('<title>MeshMesh</title>', '</script></body>', 'let companion=null;'):
        if page.count(marker) != 1:
            raise SystemExit(f'web/index.html: expected one {marker!r}')
    page = page.replace('<title>MeshMesh</title>', '<title>MeshMesh Chess</title>')
    page = page.replace('</script></body>', '</script><script src="companion.js"></script></body>')
    (target/'index.html').write_text(page)
    shutil.copyfile(ROOT/'web/chess-companion.js', target/'companion.js')
    return target


if __name__ == '__main__':
    print(build(sys.argv[1] if len(sys.argv) > 1 else 'artifacts/site'))
