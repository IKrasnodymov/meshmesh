#!/usr/bin/env python3
"""Glyph subsets for the interface languages: include/I18nFonts.h (tools/i18n.py generate).

The screens draw with u8g2 fonts that hold Latin-1 and Cyrillic. The other letters come from
fallback fonts (src/I18n.cpp): Latin Extended from the same X11 misc-fixed faces, Chinese,
Japanese and Korean from Fusion Pixel Font (8 and 12 px), Arabic from ClearlyU 12 (cu12).
Only the glyphs the translations use are kept, so the fonts fit the nRF52 flash; text
received over the radio in these scripts shows only the glyphs that are in the subset.

Sources, downloaded once into upstream/fonts (not in Git):
  u8g2 tools/font/bdf/{6x13,6x13B,5x8,4x6,10x20,6x12,cu12}.bdf and tools/font/bdfconv (built with cc)
  https://github.com/TakWolf/fusion-pixel-font/releases 2026.09.25, 8px and 12px proportional BDF (OFL)
"""
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT/'upstream'/'fonts'
BDFCONV = SRC/'bdfconv'/'bdfconv'
FUSION = 'v2026.09.25'

# name, BDF, which characters: 'ext' Latin Extended and symbols, a language code for its script, 'ar' Arabic.
FONTS = [
    ('mmFontExt13', '6x13.bdf', 'ext'), ('mmFontExt13B', '6x13B.bdf', 'ext'), ('mmFontExt8', '5x8.bdf', 'ext'),
    ('mmFontExt6', '4x6.bdf', 'ext'), ('mmFontExt20', '10x20.bdf', 'ext'), ('mmFontExt12', '6x12.bdf', 'ext'),
    ('mmFontZh12', 'fusion12/fusion-pixel-12px-proportional-zh_hans.bdf', 'zh'),
    ('mmFontJa12', 'fusion12/fusion-pixel-12px-proportional-ja.bdf', 'ja'),
    ('mmFontKo12', 'fusion12/fusion-pixel-12px-proportional-ko.bdf', 'ko'),
    ('mmFontZh8', 'fusion8/fusion-pixel-8px-proportional-zh_hans.bdf', 'zh'),
    ('mmFontJa8', 'fusion8/fusion-pixel-8px-proportional-ja.bdf', 'ja'),
    ('mmFontKo8', 'fusion8/fusion-pixel-8px-proportional-ko.bdf', 'ko'),
    ('mmFontAr12', 'cu12.bdf', 'ar'),
]
NATIVE = {'zh': '中文', 'ja': '日本語', 'ko': '한국어', 'ar': 'العربية'}


def latin1_or_cyrillic(cp):
    return cp < 0x100 or 0x400 <= cp <= 0x45f or cp in (0x490, 0x491)


def script(cp):
    if 0x600 <= cp <= 0x6ff or 0xfb50 <= cp <= 0xfdff or 0xfe70 <= cp <= 0xfeff:
        return 'ar'
    if 0x1100 <= cp <= 0x11ff or 0x3130 <= cp <= 0x318f or 0xac00 <= cp <= 0xd7a3:
        return 'cjk'
    if 0x2e80 <= cp <= 0x9fff or 0xf900 <= cp <= 0xfaff or 0xfe30 <= cp <= 0xfe4f or 0xff00 <= cp <= 0xffef:
        return 'cjk'
    return 'ext'


def used():
    """Characters of every translation, per language."""
    out = {}
    for path in sorted((ROOT/'i18n'/'firmware').glob('*.json')):
        if path.stem == 'source':
            continue
        text = ''
        for item in json.loads(path.read_text()).values():
            text += item.get('text', '') + ''.join(item.get('forms', {}).values())
        out[path.stem] = set(ord(c) for c in text)
    return out


def arabic_forms():
    """Presentation forms B and the punctuation the shaper (src/I18n.cpp) can produce."""
    return set(range(0xfe70, 0xfeff)) | {0x60c, 0x61b, 0x61f, 0x640, 0x66a, 0x66b, 0x66c}


def wanted(kind, chars):
    if kind == 'ext':
        return sorted(cp for lang, s in chars.items() for cp in s if not latin1_or_cyrillic(cp) and script(cp) == 'ext')
    if kind == 'ar':
        return sorted(arabic_forms() | set(cp for cp in chars.get('ar', ()) if script(cp) == 'ar') | set(map(ord, NATIVE['ar'])))
    return sorted(set(cp for cp in chars.get(kind, ()) if script(cp) == 'cjk') | set(map(ord, NATIVE[kind])))


def bdf_glyphs(path):
    return set(int(m) for m in re.findall(r'^ENCODING (\d+)', path.read_text(errors='replace'), re.M))


def decode(literal):
    """Bytes of the C string literals bdfconv writes."""
    out, i, s = bytearray(), 0, ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', literal))
    while i < len(s):
        if s[i] == '\\':
            n = s[i + 1]
            if n in '01234567':
                j = i + 1
                while j < len(s) and j < i + 4 and s[j] in '01234567':
                    j += 1
                out.append(int(s[i + 1:j], 8))
                i = j
                continue
            out.append({'n': 10, 't': 9, 'r': 13, '"': 34, '\\': 92, '?': 63}.get(n, ord(n)))
            i += 2
            continue
        out.append(ord(s[i]))
        i += 1
    return bytes(out)


def generate():
    if not BDFCONV.exists():
        raise SystemExit(f'{BDFCONV}: build u8g2 tools/font/bdfconv there (see this file)')
    chars = used()
    parts, drawable, report = [], set(), []
    tmp = ROOT/'.pio'/'i18n-fonts'
    tmp.mkdir(parents=True, exist_ok=True)
    for name, bdf, kind in FONTS:
        source = SRC/bdf
        want = wanted(kind, chars)
        have = bdf_glyphs(source)
        lost = [cp for cp in want if cp not in have]
        take = [cp for cp in want if cp in have] or [0x20]
        if lost and kind != 'ar':
            report.append(f'{name}: {len(lost)} missing in {bdf}: ' + ''.join(map(chr, lost[:40])))
        mapping = ','.join(f'${cp:x}' for cp in take)
        out = tmp/f'{name}.c'
        subprocess.run([str(BDFCONV), '-f', '1', '-b', '0', '-m', mapping, '-n', name, '-o', str(out), str(source)], check=True, capture_output=True)
        code = out.read_text()
        m = re.search(r'const uint8_t ' + name + r'\[(\d+)\][^=]*=\s*(.*?);\s*$', code, re.S)
        data = decode(m.group(2))
        assert len(data) == int(m.group(1)) - 1, name
        drawable |= set(take)
        parts.append(f'// {bdf}: {len(take)} glyphs, {len(data) + 1} bytes')
        parts.append(f'static const uint8_t {name}[{m.group(1)}] U8G2_FONT_SECTION("{name}") =\n  ' + m.group(2).strip() + ';')
        print(f'{name}: {len(take)} glyphs, {len(data) + 1} bytes')
    for line in report:
        print('warning:', line)
    drawable |= set(range(0x20, 0x100)) | set(range(0x400, 0x460)) | {0x490, 0x491}
    licence = ('// Fallback glyphs for the interface languages, generated by tools/i18n.py generate; do not edit.\n'
               '// Latin Extended: X11 misc-fixed fonts (public domain) via u8g2. Arabic: ClearlyU 12 (cu12) via u8g2,\n'
               '// notice in i18n/fonts/clearlyu-cu12.txt. Chinese, Japanese, Korean: Fusion Pixel Font ' + FUSION + '\n'
               '// (https://github.com/TakWolf/fusion-pixel-font), SIL Open Font License 1.1 with the licenses of the\n'
               '// fonts it builds on in i18n/fonts/fusion-pixel/; subsets, glyphs unchanged.\n'
               '// glyphs: ' + ' '.join(f'{cp:x}' for cp in sorted(drawable)) + '\n')
    (ROOT/'include'/'I18nFonts.h').write_text(licence + '#pragma once\n#include <stdint.h>\n#include <U8g2_for_Adafruit_GFX.h>\n' + '\n'.join(parts) + '\n')
    print('include/I18nFonts.h:', sum(1 for _ in FONTS), 'fonts')


if __name__ == '__main__':
    generate()
