#!/usr/bin/env python3
"""Build the site's chess page for a stock MeshCore companion: OUTDIR/chess/.

tools/chess_site.py OUTDIR   (tools/pages.py calls it for GitHub Pages)
The page is web/index.html, the boards' own page, with web/chess-companion.js and the tab icon
web/chess-icon.svg: the rules, the games
and the companion link run in the browser (docs/chess.md). Web Serial and Web Bluetooth need HTTPS,
so the page works from the site, not from a board's access point.
The page speaks the site's 15 languages: web/chess-i18n.js translates the Russian text of the page as it
is drawn, with i18n/chess/<lang>.json (the Russian text is the key; en.json lists all of them).
"""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LANGS = ['en', 'ru', 'uk', 'es', 'pt', 'fr', 'de', 'it', 'pl', 'tr', 'zh', 'ja', 'ko', 'ar', 'id']
PLACEHOLDER = re.compile(r'\{(?:\d|r|san)\}')
FORMS = {'one', 'two', 'few', 'many', 'other', 'zero'}


def dictionaries():
    """i18n/chess/<lang>.json, checked against en.json: the same keys, placeholders and plural forms."""
    base = ROOT/'i18n/chess'
    en = json.loads((base/'en.json').read_text())
    out, errors = {}, []
    for lang in LANGS:
        if lang == 'ru':
            continue
        d = json.loads((base/f'{lang}.json').read_text())
        if list(d) != list(en):
            errors.append(f'{lang}: keys differ from en.json (missing {sorted(set(en)-set(d))[:3]}, extra {sorted(set(d)-set(en))[:3]})')
        for key, value in d.items():
            want = sorted(PLACEHOLDER.findall(key))
            if key == '_pieces':
                if not isinstance(value, str) or len(value.split(' ')) != 5:
                    errors.append(f'{lang}: _pieces needs five letters K Q R B N')
                continue
            texts = list(value.values()) if isinstance(value, dict) else [value]
            if isinstance(value, dict) and ('other' not in value or set(value) - FORMS):
                errors.append(f'{lang}: {key!r} plural forms {sorted(value)}')
            for text in texts:
                if not isinstance(text, str) or not text.strip():
                    errors.append(f'{lang}: {key!r} is empty')
                elif sorted(PLACEHOLDER.findall(text)) != want and not (isinstance(value, dict) and lang == 'ar'):
                    errors.append(f'{lang}: {key!r} placeholders {PLACEHOLDER.findall(text)}, expected {want}')
        out[lang] = d
    if errors:
        raise SystemExit('i18n/chess:\n  ' + '\n  '.join(errors))
    return out


def build(out):
    target = Path(out)/'chess'
    target.mkdir(parents=True, exist_ok=True)
    page = (ROOT/'web/index.html').read_text()
    for marker in ('<title>MeshMesh</title>', '</script></body>', 'let companion=null;'):
        if page.count(marker) != 1:
            raise SystemExit(f'web/index.html: expected one {marker!r}')
    page = page.replace('<title>MeshMesh</title>', '<title>MeshMesh Chess</title><link rel="icon" href="icon.svg" type="image/svg+xml">')
    page = page.replace('</script></body>', '</script><script src="i18n.js"></script><script src="companion.js"></script></body>')
    (target/'index.html').write_text(page)
    dicts = json.dumps(dictionaries(), ensure_ascii=False, separators=(',', ':'))
    (target/'i18n.js').write_text(f'globalThis.MeshMeshChessDicts={dicts};\n' + (ROOT/'web/chess-i18n.js').read_text())
    shutil.copyfile(ROOT/'web/chess-companion.js', target/'companion.js')
    shutil.copyfile(ROOT/'web/chess-icon.svg', target/'icon.svg')
    return target


if __name__ == '__main__':
    print(build(sys.argv[1] if len(sys.argv) > 1 else 'artifacts/site'))
