#!/usr/bin/env python3
"""Interface languages of the firmware: strings, translations, tables and fonts.

English and Russian live in the code: t("English","Русский"), tr(...) and
count(n,"one","many","ру1","ру2","ру5"). The other languages are in
i18n/firmware/<lang>.json, keyed by a hash of the English and Russian text, so a
changed source string drops its old translation.

tools/i18n.py sync       add new strings to every language file, drop removed ones
tools/i18n.py check      every string translated, same edges and placeholders, fits, glyphs exist
tools/i18n.py generate   include/I18nTable.h (strings) and include/I18nFonts.h (glyph subsets)
tools/i18n.py todo LANG  the untranslated strings of one language, with context

The fonts are subsets of BDF fonts (upstream/fonts, see FONT_SOURCES) made with the
u8g2 bdfconv tool; generate needs them, check does not.
"""
import json
import re
import subprocess
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIR = ROOT/'i18n'/'firmware'

# Order matters: the index is the stored language number (Config::lang, I18n.h).
LANGS = [
    ('en', 'English'), ('ru', 'Русский'), ('uk', 'Українська'), ('es', 'Español'), ('pt', 'Português'),
    ('fr', 'Français'), ('de', 'Deutsch'), ('it', 'Italiano'), ('pl', 'Polski'), ('tr', 'Türkçe'),
    ('zh', '中文'), ('ja', '日本語'), ('ko', '한국어'), ('ar', 'العربية'), ('id', 'Bahasa Indonesia'),
]
CODES = [c for c, _ in LANGS]
TRANSLATED = CODES[2:]  # en and ru are in the code
CATEGORIES = ['zero', 'one', 'two', 'few', 'many', 'other']
PLURALS = {
    'en': ['one', 'other'], 'ru': ['one', 'few', 'many'], 'uk': ['one', 'few', 'many'], 'es': ['one', 'other'],
    'pt': ['one', 'other'], 'fr': ['one', 'other'], 'de': ['one', 'other'], 'it': ['one', 'other'],
    'pl': ['one', 'few', 'many'], 'tr': ['one', 'other'], 'zh': ['other'], 'ja': ['other'], 'ko': ['other'],
    'ar': ['zero', 'one', 'two', 'few', 'many', 'other'], 'id': ['other'],
}
# Interface families: the 320x240 keyboard interface and the 128x64 one; shared files go to both.
FULL = ['src/Ui.cpp', 'src/UiServer.inc', 'src/UiSolitaire.inc', 'src/UiChess.inc', 'src/UiTour.inc', 'src/UiChannels.inc', 'src/UiPet.inc', 'src/UiDice.inc', 'src/UiRemote.inc']
COMPACT = ['src/UiHeltec.cpp', 'src/UiCompose.inc', 'src/UiChessCompact.inc', 'src/UiPetCompact.inc', 'src/UiDiceCompact.inc', 'src/UiHires.inc']
SHARED = ['src/ChessNet.cpp', 'src/ChessTour.cpp', 'src/ChessSync.cpp', 'src/MeshRadio.cpp', 'src/Internet.cpp', 'src/MeshServer.cpp', 'src/App.cpp', 'src/Pet.cpp', 'src/Dice.cpp']

LIT = r'"(?:[^"\\\n]|\\.)*"'
TEXT = re.compile(r'(?<![A-Za-z0-9_.>])(?:t|tr)\(\s*(' + LIT + r')\s*,\s*(' + LIT + r')\s*\)')
COUNT = re.compile(r'(?<![A-Za-z0-9_.>])(?:count|plural)\((?:[^;"]|\["[^"]*"\])*?,\s*(' + LIT + r')\s*,\s*(' + LIT + r')\s*,\s*(' + LIT + r')\s*,\s*(' + LIT + r')\s*,\s*(' + LIT + r')\s*\)')
PLACEHOLDER = re.compile(r'%[-+ 0#]*\d*(?:\.\d+)?[a-z]|\{[a-z]+\}')
# {"English","Русский"} pairs in tables read with tr(pair[0],pair[1]) (Ui.cpp netError)
PAIR = re.compile(r'\{\s*(' + LIT + r')\s*,\s*(' + LIT + r')\s*\}')


def c_string(literal):
    """The bytes of a C string literal, decoded as the compiler does."""
    body, out, i = literal[1:-1], bytearray(), 0
    while i < len(body):
        c = body[i]
        if c != '\\':
            out += c.encode()
            i += 1
            continue
        n = body[i + 1]
        if n == 'x':
            j = i + 2
            while j < len(body) and body[j] in '0123456789abcdefABCDEF':
                j += 1
            out.append(int(body[i + 2:j], 16) & 0xff)
            i = j
        elif n in '01234567':
            j = i + 1
            while j < len(body) and j < i + 4 and body[j] in '01234567':
                j += 1
            out.append(int(body[i + 1:j], 8) & 0xff)
            i = j
        else:
            out.append({'n': 10, 't': 9, 'r': 13, '0': 0, '"': 34, "'": 39, '\\': 92, '?': 63}[n])
            i += 2
    return bytes(out).decode('utf-8')


def key(*parts):
    """FNV-1a over the UTF-8 parts joined by 0x1f; the firmware computes the same (I18n.cpp)."""
    h = 0x811c9dc5
    for i, part in enumerate(parts):
        for b in (b'\x1f' if i else b'') + part.encode():
            h = ((h ^ b) * 0x01000193) & 0xffffffff
    return f'{h:08x}'


def cells(s):
    """Approximate width in 6 px cells of the body font: wide (CJK) glyphs take two."""
    return sum(2 if unicodedata.east_asian_width(c) in 'WF' else 0 if unicodedata.combining(c) else 1 for c in s)


def extract():
    """Every translatable string in the sources: {key: entry}."""
    entries = {}
    for family, files in (('full', FULL), ('compact', COMPACT), ('shared', SHARED)):
        for name in files:
            path = ROOT/name
            if not path.exists():
                continue
            lines = path.read_text().split('\n')
            for n, line in enumerate(lines, 1):
                found = [('text', m) for m in TEXT.finditer(line)] + [('plural', m) for m in COUNT.finditer(line)]
                found += [('text', m) for m in PAIR.finditer(line) if re.search('[А-Яа-яЁё]', m.group(2)) and not re.search('[А-Яа-яЁё]', m.group(1))]
                for kind, m in found:
                    parts = [c_string(g) for g in m.groups()]
                    k = key(*parts)
                    e = entries.setdefault(k, {'kind': kind, 'families': set(), 'where': []})
                    if kind == 'text':
                        e['en'], e['ru'] = parts
                    else:
                        e['en'], e['ru'] = {'one': parts[0], 'other': parts[1]}, {'one': parts[2], 'few': parts[3], 'many': parts[4]}
                    e['families'].add(family)
                    if len(e['where']) < 3:
                        e['where'].append(f'{name}:{n}: {line.strip()[:220]}')
    for e in entries.values():
        e['families'] = sorted(e['families'])
    return entries


def load(code):
    path = DIR/f'{code}.json'
    return json.loads(path.read_text()) if path.exists() else {}


def save(code, data):
    DIR.mkdir(parents=True, exist_ok=True)
    (DIR/f'{code}.json').write_text(json.dumps(data, ensure_ascii=False, indent=1) + '\n')


def sync():
    entries = extract()
    for code in TRANSLATED:
        old = load(code)
        new = {}
        for k, e in sorted(entries.items(), key=lambda kv: (kv[1]['where'][0])):
            prev = old.get(k, {})
            item = {'en': e['en'], 'ru': e['ru']}
            if e['kind'] == 'text':
                item['text'] = prev.get('text', '')
            else:
                forms = prev.get('forms', {})
                item['forms'] = {c: forms.get(c, '') for c in PLURALS[code]}
            new[k] = item
        save(code, new)
    source = {k: {'where': e['where'], 'families': e['families'], 'cells': max(cells(e['en'] if e['kind'] == 'text' else e['en']['other']), cells(e['ru'] if e['kind'] == 'text' else e['ru']['many']))} for k, e in entries.items()}
    (DIR/'source.json').write_text(json.dumps(source, ensure_ascii=False, indent=1, sort_keys=True) + '\n')
    print(len(entries), 'strings;', ', '.join(f'{c}: {missing(c)} untranslated' for c in TRANSLATED))


def missing(code):
    data = load(code)
    return sum(1 for v in data.values() if ('text' in v and not v['text']) or ('forms' in v and not all(v['forms'].values())))


def glyphs_available():
    """Codepoints the firmware can draw: the source fonts when they are here (upstream/fonts), else the
    glyph list of include/I18nFonts.h (what generate kept), with Latin-1 and Cyrillic of the u8g2 fonts."""
    import i18n_fonts
    if i18n_fonts.BDFCONV.exists():
        have = set(range(0x20, 0x100)) | set(range(0x400, 0x460)) | {0x490, 0x491} | i18n_fonts.arabic_forms()
        for name, bdf, kind in i18n_fonts.FONTS:
            if name in ('mmFontExt13', 'mmFontZh12', 'mmFontJa12', 'mmFontKo12', 'mmFontAr12'):
                have |= i18n_fonts.bdf_glyphs(i18n_fonts.SRC/bdf)
        # Arabic letters are drawn as presentation forms (src/I18n.cpp shapes them)
        return have | set(range(0x621, 0x64b))
    header = ROOT/'include'/'I18nFonts.h'
    if not header.exists():
        return None
    m = re.search(r'// glyphs: (.*)', header.read_text())
    return set(int(x, 16) for x in m.group(1).split()) if m else None


def check(strict=True):
    entries = extract()
    errors, warnings = [], []
    can_draw = glyphs_available()
    for code in TRANSLATED:
        data = load(code)
        if set(data) != set(entries):
            errors.append(f'{code}: out of date with the sources; run tools/i18n.py sync')
        for k, e in entries.items():
            item = data.get(k)
            if not item:
                continue
            where = e['where'][0].split(': ')[0]
            values = [item.get('text', '')] if e['kind'] == 'text' else [item['forms'].get(c, '') for c in PLURALS[code]]
            sources = [e['en'], e['ru']] if e['kind'] == 'text' else list(e['en'].values()) + list(e['ru'].values())
            for v in values:
                if not v:
                    errors.append(f'{code} {k} {where}: not translated: {sources[0]!r}')
                    continue
                if e['kind'] == 'text':
                    lead = lambda s: len(s) - len(s.lstrip(' '))
                    trail = lambda s: len(s) - len(s.rstrip(' '))
                    if lead(v) != lead(e['en']) or trail(v) != trail(e['en']):
                        errors.append(f'{code} {k} {where}: spaces at the edges differ from {e["en"]!r}: {v!r}')
                    if v.count('\n') != e['en'].count('\n'):
                        errors.append(f'{code} {k} {where}: line breaks differ from {e["en"]!r}: {v!r}')
                    if sorted(PLACEHOLDER.findall(v)) != sorted(PLACEHOLDER.findall(e['en'])):
                        errors.append(f'{code} {k} {where}: placeholders differ from {e["en"]!r}: {v!r}')
                elif PLACEHOLDER.findall(v) != ['{n}'] and not (code == 'ar' and PLACEHOLDER.findall(v) == []):
                    errors.append(f'{code} {k} {where}: a plural form needs one {{n}} where the number goes (Arabic may leave it out): {v!r}')
                if re.search(r'[Ѐ-ӿ]', v) and code != 'uk':
                    errors.append(f'{code} {k} {where}: Cyrillic left in {v!r}')
                if can_draw is not None:
                    lost = sorted(set(c for c in v if ord(c) not in can_draw and ord(c) >= 0x20 and not any(c in s for s in sources)))
                    if lost:
                        errors.append(f'{code} {k} {where}: no glyph for {"".join(lost)!r} in {v!r}')
                limit = max(cells(s) for s in sources)
                if cells(v.replace('{n}', '')) > limit * 1.3 + 2:
                    warnings.append(f'{code} {k} {where}: {cells(v)} cells, sources {limit}: {v!r} ({sources[0]!r})')
    errors += check_site()
    if (ROOT/'include'/'I18nTable.h').read_text() != table_header(entries):
        errors.append('include/I18nTable.h is older than the translations; run tools/i18n.py generate')
    for w in warnings:
        print('long:', w)
    for e in errors:
        print('ERROR', e)
    print(f'{len(entries)} strings, {len(TRANSLATED)} languages: {len(errors)} errors, {len(warnings)} long')
    return not errors


def check_site():
    """site/i18n/<lang>.json: the keys of en.json, nothing empty, the same HTML tags; README.<lang>.md with every language linked."""
    errors = []
    site = ROOT/'site'/'i18n'
    en = json.loads((site/'en.json').read_text())
    tags = lambda v: sorted(re.findall(r'</?[a-z]+(?: [^>]*)?>', v))
    for code in CODES:
        path = site/f'{code}.json'
        if not path.exists():
            errors.append(f'site {code}: no {path.relative_to(ROOT)}')
            continue
        d = json.loads(path.read_text())
        for k in sorted(set(en) - set(d)):
            errors.append(f'site {code}: missing {k}')
        for k in sorted(set(d) - set(en)):
            errors.append(f'site {code}: unknown {k}')
        for k, v in d.items():
            if k in en and (not v.strip() or tags(v) != tags(en[k])):
                errors.append(f'site {code} {k}: empty or HTML tags differ from English: {v!r}')
            if code not in ('ru', 'uk') and re.search('[А-Яа-яЁё]', v):
                errors.append(f'site {code} {k}: Cyrillic left: {v!r}')
    for code in CODES:
        path = ROOT/('README.md' if code == 'en' else f'README.{code}.md')
        if not path.exists():
            errors.append(f'readme {code}: no {path.name}')
            continue
        bar = path.read_text().split('\n')[2]
        for other, name in LANGS:
            target = 'README.md' if other == 'en' else f'README.{other}.md'
            if other != code and f'[{name}]({target})' not in bar:
                errors.append(f'readme {code}: the language line lacks [{name}]({target})')
            if other == code and f'**{name}**' not in bar:
                errors.append(f'readme {code}: the language line should show **{name}**')
    return errors


def c_literal(s):
    out = ''
    for b in s.encode():
        out += chr(b) if 0x20 <= b < 0x7f and b not in (0x22, 0x5c, 0x3f) else f'\\{b:03o}'
    return '"' + out + '"'


def table(entries, family):
    """C++ for one interface family: sorted keys and, per language, offsets into one string blob."""
    keys = sorted(k for k, e in entries.items() if family in e['families'] or 'shared' in e['families'])
    out = [f'constexpr unsigned count={len(keys)};', 'const uint32_t keys[count]={' + ','.join(f'0x{k}' for k in keys) + '};']
    for code in TRANSLATED:
        data = load(code)
        blob, offsets = b'', []
        for k in keys:
            item = data.get(k, {})
            if entries[k]['kind'] == 'text':
                value = item.get('text', '')
            else:  # all six categories; an absent one falls back to other at run time
                value = '\x1f'.join(item.get('forms', {}).get(c, '') for c in CATEGORIES)
            offsets.append(len(blob))
            blob += value.encode() + b'\0'
        if len(blob) >= 1 << 16:
            raise SystemExit(f'{code}: {len(blob)} bytes do not fit 16-bit offsets')
        # MM_LANG_IN (src/I18n.cpp): an nRF52 image keeps only the language it is built for.
        out.append(f'#if MM_LANG_IN({CODES.index(code)})')
        out.append(f'const uint16_t {code}Offsets[count]={{' + ','.join(map(str, offsets)) + '};')
        out.append(f'const char {code}Text[{len(blob)}]=' + c_literal(blob.decode()[:-1]) + ';')
        out.append('#else')
        out.append(f'constexpr const uint16_t* {code}Offsets=nullptr;constexpr const char* {code}Text=nullptr;')
        out.append('#endif')
    out.append('const uint16_t* const offsets[]={' + ','.join(f'{c}Offsets' for c in TRANSLATED) + '};')
    out.append('const char* const texts[]={' + ','.join(f'{c}Text' for c in TRANSLATED) + '};')
    return out


def table_header(entries):
    lines = ['// Generated by tools/i18n.py from i18n/firmware/*.json; do not edit.', '#pragma once', '#include <stdint.h>',
             f'// Languages after en and ru, in Config::lang order: {" ".join(TRANSLATED)}', 'namespace i18nTable {',
             '#if defined(MM_COMPACT)', *table(entries, 'compact'), '#else', *table(entries, 'full'), '#endif', '}']
    return '\n'.join(lines) + '\n'


def generate_table():
    entries = extract()
    (ROOT/'include'/'I18nTable.h').write_text(table_header(entries))
    print('include/I18nTable.h:', len(entries), 'strings')


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'check'
    if cmd == 'sync':
        sync()
    elif cmd == 'check':
        sys.exit(0 if check() else 1)
    elif cmd == 'generate':
        generate_table()
        import i18n_fonts  # noqa: E402
        i18n_fonts.generate()
    elif cmd == 'todo':
        code = sys.argv[2]
        source = json.loads((DIR/'source.json').read_text())
        for k, v in load(code).items():
            if ('text' in v and not v['text']) or ('forms' in v and not all(v['forms'].values())):
                print(k, json.dumps({**v, 'where': source[k]['where'][0]}, ensure_ascii=False))
    else:
        raise SystemExit(__doc__)
