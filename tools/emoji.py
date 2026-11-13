#!/usr/bin/env python3
"""Emoji on the screens: Twemoji pictures as 12x12 pixel art -> include/EmojiData.h.

.venv/bin/python tools/emoji.py                 writes the header (the pictures are cached in upstream/twemoji)
.venv/bin/python tools/emoji.py --sheet OUT.png also draws every emoji, enlarged and at its own size

Pictures: Twemoji 15.1 (https://github.com/jdecked/twemoji), graphics by Twitter, Inc and other contributors,
CC-BY 4.0. Each 72x72 picture becomes 12x12: every 6x6 block takes the most frequent palette colour, colours
far from the picture's main colour weigh more, so eyes, mouths and marks stay. One palette of 31 colours
(Twemoji's own) serves every emoji; 0 is transparent.

Packing (Emoji.cpp decodes it), MSB first, each emoji from a whole byte: 4 bits the number of local colours k,
k x 5 bits those palette values, then runs until 144 pixels: a code of ceil(log2(k+1)) bits (k = "as the
pixel above") and the run length as an Elias gamma code. About 30 bytes an emoji.
"""
import argparse
import math
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / 'upstream' / 'twemoji'
URL = 'https://cdn.jsdelivr.net/gh/jdecked/twemoji@15.1.0/assets/72x72/{}.png'
N = 12

# Twemoji's colours (5 bits: 1..31).
PALETTE = [0xFFCC4D, 0xFFAC33, 0xF4900C, 0xFFD983, 0xFFE8B6, 0xDD2E44, 0xBE1931, 0xA0041E, 0xEA596E, 0xF4ABBA,
           0xC1694F, 0xD99E82, 0x662113, 0x664500, 0x55ACEE, 0x3B88C3, 0xBDDDF4, 0x226699, 0x77B255, 0x5C913B,
           0x3E721D, 0xAA8ED6, 0x744EAA, 0xFFFFFF, 0xE1E8ED, 0xCCD6DD, 0x99AAB5, 0x66757F, 0x31373D, 0x14171A,
           0xFA743E]

# The set: faces, hearts, hands, nature, animals, food, things, travel, signs (single code points only;
# skin tones and variation selectors are drawn as nothing, so a toned hand shows the yellow one).
EMOJI = """
1f600 1f603 1f604 1f601 1f606 1f605 1f923 1f602 1f642 1f643 1f609 1f60a 1f607 1f970 1f60d 1f929 1f618 1f617 1f61a
1f60b 1f61b 1f61c 1f92a 1f61d 1f911 1f917 1f92d 1f92b 1f914 1f910 1f928 1f610 1f611 1f636 1f60f 1f612 1f644 1f62c
1f925 1f60c 1f614 1f62a 1f924 1f634 1f637 1f912 1f915 1f922 1f92e 1f927 1f975 1f976 1f974 1f635 1f92f 1f920 1f973
1f60e 1f913 1f615 1f61f 1f641 2639 1f62e 1f62f 1f632 1f633 1f97a 1f626 1f627 1f628 1f630 1f625 1f622 1f62d 1f631
1f616 1f623 1f61e 1f613 1f629 1f62b 1f971 1f624 1f621 1f620 1f92c 1f608 1f47f 1f480 1f4a9 1f921 1f47b 1f47d 1f916
1f63a 1f639 1f63b 1f648 1f649 1f64a
2764 1f9e1 1f49b 1f49a 1f499 1f49c 1f5a4 1f90d 1f494 1f495 1f496 1f498 1f48b 1f4af 1f4a2 1f4a5 1f4a6 1f4a8 1f4ac
1f4a4
1f44b 270b 1f44c 270c 1f91e 1f918 1f919 1f448 1f449 1f446 1f447 261d 1f44d 1f44e 270a 1f44a 1f44f 1f64c 1f450
1f91d 1f64f 1f4aa 1f440 1f937 1f926
2600 1f31e 1f319 2b50 1f31f 2728 26a1 1f525 2744 2601 1f327 26c4 1f308 2614 1f30a 1f332 1f333 1f335 1f340 1f339
1f338 1f33b 1f342 26f0 1f3d4
1f436 1f431 1f42d 1f430 1f98a 1f43b 1f43c 1f428 1f42f 1f981 1f42e 1f437 1f438 1f435 1f414 1f427 1f426 1f986 1f989
1f41d 1f41b 1f98b 1f40c 1f41f 1f419 1f422 1f40d 1f984
2615 1f37a 1f37b 1f377 1f378 1f355 1f354 1f35f 1f32d 1f370 1f382 1f36a 1f34e 1f34c 1f353 1f352 1f349 1f347 1f344
1f95a 1f35e
1f389 1f38a 1f381 1f388 1f3c6 1f947 26bd 1f3b2 265f 1f3ae 1f3b5 1f3b6 1f3b8 1f4f7 1f4f1 1f4bb 1f4e1 1f4f6 1f50b
1f50c 1f4a1 1f526 1f4cd 1f4cc 1f5fa 1f9ed 23f0 231b 1f4c5 1f4dd 1f4e2 1f514 1f511 1f512 1f513 1f6e0 1f527 1f528
1f4b0 1f48e 1f451 1f453 1f392
1f697 1f695 1f68c 1f6b2 1f3cd 2708 1f681 1f680 26f5 1f6a2 1f3e0 1f3e1 1f3e2 1f3e5 26fa 1f6a8 26fd
2705 274c 274e 2753 2757 203c 2049 26a0 26d4 1f6ab 1f198 1f197 1f195 1f199 1f534 1f7e0 1f7e1 1f7e2 1f535 1f7e3
26ab 26aa 2622 2620 1f6a9 1f3c1
""".split()


def picture(code):
    from PIL import Image
    path = CACHE / f'{code}.png'
    if not path.exists():
        CACHE.mkdir(parents=True, exist_ok=True)
        with urllib.request.urlopen(URL.format(code), timeout=60) as r:
            path.write_bytes(r.read())
    return Image.open(path).convert('RGBA')


def distance(a, b):
    return 2 * (a[0] - b[0]) ** 2 + 4 * (a[1] - b[1]) ** 2 + 3 * (a[2] - b[2]) ** 2


RGB = [((c >> 16) & 255, (c >> 8) & 255, c & 255) for c in PALETTE]
_near = {}


def nearest(c):
    if c not in _near:
        _near[c] = min(range(len(RGB)), key=lambda i: distance(c, RGB[i])) + 1
    return _near[c]


def pixels(image):
    """12x12 palette values, row by row (0 transparent)."""
    image = image.resize((72, 72)) if image.size != (72, 72) else image
    px = image.load()
    counts = {}
    for y in range(72):
        for x in range(72):
            r, g, b, a = px[x, y]
            if a >= 128:
                k = nearest((r, g, b))
                counts[k] = counts.get(k, 0) + 1
    main = max(counts, key=counts.get) if counts else 1
    out = []
    for by in range(N):
        for bx in range(N):
            block, clear = {}, 0
            for y in range(by * 6, by * 6 + 6):
                for x in range(bx * 6, bx * 6 + 6):
                    r, g, b, a = px[x, y]
                    if a < 128:
                        clear += 1
                        continue
                    k = nearest((r, g, b))
                    block[k] = block.get(k, 0) + 1
            if clear * 2 > 36 or not block:
                out.append(0)
                continue
            out.append(max(block, key=lambda k: block[k] * (1 + 2 * min(1, distance(RGB[k - 1], RGB[main - 1]) / 60000))))
    return out


class Bits:
    def __init__(self):
        self.bits = []

    def put(self, value, count):
        self.bits += [(value >> (count - 1 - i)) & 1 for i in range(count)]

    def gamma(self, n):
        length = n.bit_length()
        self.bits += [0] * (length - 1)
        self.put(n, length)

    def bytes(self):
        b = self.bits + [0] * (-len(self.bits) % 8)
        return bytes(int(''.join(map(str, b[i:i + 8])), 2) for i in range(0, len(b), 8))


def encode(px):
    colours = sorted(set(px))
    assert 1 <= len(colours) <= 15
    width = max(1, math.ceil(math.log2(len(colours) + 1)))
    out = Bits()
    out.put(len(colours), 4)
    for c in colours:
        out.put(c, 5)
    i = 0
    while i < len(px):
        j = i
        while j < len(px) and px[j] == px[i]:
            j += 1
        k = i
        if i >= N:
            while k < len(px) and px[k] == px[k - N]:
                k += 1
        if k > j:
            out.put(len(colours), width)
            run = k - i
        else:
            out.put(colours.index(px[i]), width)
            run = j - i
        out.gamma(run)
        i += run
    return out.bytes()


def decode(data):
    """The firmware's decoder, as a check of the encoder."""
    bits = ''.join(f'{b:08b}' for b in data)
    pos = 0

    def take(n):
        nonlocal pos
        v = int(bits[pos:pos + n], 2)
        pos += n
        return v
    k = take(4)
    colours = [take(5) for _ in range(k)]
    width = max(1, math.ceil(math.log2(k + 1)))
    px = []
    while len(px) < N * N:
        code = take(width)
        zeros = 0
        while bits[pos] == '0':
            zeros += 1
            pos += 1
        run = take(zeros + 1)
        for _ in range(run):
            px.append(px[len(px) - N] if code == k else colours[code])
    return px


def key(code):
    cp = int(code, 16)
    return cp - 0x10000 if 0x1F000 <= cp <= 0x1FFFF else cp


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--sheet')
    args = p.parse_args()
    if len(set(EMOJI)) != len(EMOJI):
        raise SystemExit('duplicate emoji')
    for code in EMOJI:
        cp = int(code, 16)
        if not (cp < 0xF000 or 0x1F000 <= cp <= 0x1FFFF):
            raise SystemExit(f'{code}: outside the keyed ranges')
    codes = sorted(EMOJI, key=key)
    art = {c: pixels(picture(c)) for c in codes}
    data = {}
    for c in codes:
        data[c] = encode(art[c])
        if decode(data[c]) != art[c] or len(data[c]) > 255:
            raise SystemExit(f'{c}: packing check failed')
    blob = b''.join(data[c] for c in codes)
    lines = ['// Generated by tools/emoji.py: emoji as 12x12 pixel art from Twemoji 15.1 (graphics (c) Twitter, Inc',
             '// and other contributors, CC-BY 4.0, https://github.com/jdecked/twemoji). Do not edit.',
             '// keys: the code point, or minus 0x10000 for U+1F000-1FFFF, ascending; sizes: bytes of each picture;',
             '// bits: the pictures (tools/emoji.py describes the packing, Emoji.cpp decodes it).',
             '#pragma once', '#include <stdint.h>', 'namespace emoji {',
             f'constexpr unsigned Count={len(codes)},Size={N},Colors={len(PALETTE)};',
             'constexpr uint32_t palette[Colors]={' + ','.join(f'0x{c:06x}' for c in PALETTE) + '};',
             '#define MM_EMOJI_PALETTE(f) ' + ','.join(f'f(0x{c:06x})' for c in PALETTE),
             '#if defined(EMOJI_DATA_DEFINE)',
             'const uint16_t keys[Count]={' + ','.join(f'0x{key(c):04x}' for c in codes) + '};',
             'const uint8_t sizes[Count]={' + ','.join(str(len(data[c])) for c in codes) + '};',
             f'const uint8_t bits[{len(blob)}]={{']
    for i in range(0, len(blob), 32):
        lines.append(' ' + ','.join(f'0x{b:02x}' for b in blob[i:i + 32]) + ',')
    lines += ['};', '#endif', '}', '']
    (ROOT / 'include' / 'EmojiData.h').write_text('\n'.join(lines))
    print(f'{len(codes)} emoji, {len(blob)} bytes of pictures, {len(blob) + 3 * len(codes)} with the index')
    if args.sheet:
        from PIL import Image
        cols, cell = 20, N * 4 + 8
        rows = (len(codes) + cols - 1) // cols
        sheet = Image.new('RGB', (cols * cell, rows * cell + rows * (N + 4)), (8, 12, 17))
        for i, c in enumerate(EMOJI):
            small = Image.new('RGB', (N, N), (8, 12, 17))
            for n, v in enumerate(art[c]):
                if v:
                    small.putpixel((n % N, n // N), RGB[v - 1])
            sheet.paste(small.resize((N * 4, N * 4), Image.NEAREST), ((i % cols) * cell, (i // cols) * cell))
            sheet.paste(small, ((i % cols) * cell, rows * cell + (i // cols) * (N + 4)))
        sheet.save(args.sheet)


if __name__ == '__main__':
    sys.exit(main())
