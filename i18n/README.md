# Translations

MeshMesh speaks 15 languages: English (`en`), Russian (`ru`), Ukrainian (`uk`), Spanish (`es`),
Portuguese (`pt`, Brazilian), French (`fr`), German (`de`), Italian (`it`), Polish (`pl`), Turkish (`tr`),
Chinese (`zh`, Simplified), Japanese (`ja`), Korean (`ko`), Arabic (`ar`) and Indonesian (`id`).

| What | Source | Translations |
|---|---|---|
| Device screens (firmware) | `t("English","Русский")`, `tr(...)`, `count(n,"one","many","ру1","ру2","ру5")` in `src/` | `i18n/firmware/<lang>.json` |
| Website | `site/i18n/en.json` (and `ru.json`) | `site/i18n/<lang>.json` |
| Overview | `README.md` | `README.<lang>.md` (`README.ru.md` is the full Russian guide) |
| Detailed documents | `docs/*.md` (Russian) | `docs/en/*.md` (English) |

The device web page (`web/index.html`) and the Android app are in Russian only.

```sh
python3 tools/i18n.py sync        # after changing strings in src/: new ones appear empty, removed ones go
python3 tools/i18n.py todo de     # what German still lacks, with the code line it comes from
python3 tools/i18n.py check       # everything below; the build of the site fails without it
.venv/bin/python tools/i18n.py generate   # include/I18nTable.h and the glyph subsets in include/I18nFonts.h
```

`generate` needs the BDF fonts and u8g2 `bdfconv` in `upstream/fonts` (see `tools/i18n_fonts.py`).
The glyph subsets come from Fusion Pixel Font (SIL OFL 1.1, licenses in `i18n/fonts/fusion-pixel/`),
ClearlyU 12 (`i18n/fonts/clearlyu-cu12.txt`) and the public-domain X11 misc-fixed fonts.
Run it after every translation change, then build the boards and look at the screens:
`.venv/bin/python tools/site_shots.py OUTDIR de` renders them on a computer.

## Firmware strings

Each entry of `i18n/firmware/<lang>.json` carries the English and Russian source and the translation:

```json
"5b0c2a7e": {"en": "Settings", "ru": "Настройки", "text": "Einstellungen"}
```

The key is a hash of the source, so editing the English or Russian text in the code makes a new entry
(and drops the old translation). `i18n/firmware/source.json` tells where each string is used and its
width: `cells` is the longer of the English and Russian text in 6-pixel columns (a CJK character takes two).

- **Fit the screen.** The M9 screen is 320×240, the Heltec and GAT562 OLED 128×64 (21 columns).
  Keep a translation within `cells`; abbreviate as the Russian does («Отпр.», «удерж.») when needed.
  `check` reports translations more than 30% longer.
- **Keep the edges.** Leading and trailing spaces, line breaks and placeholders (`%d`, `%s`,
  `{weekday}`, `{month}`, `{day}`) stay exactly as in English: the code glues these strings together.
- **Keep the names.** MeshMesh, MeshCore, LoRa, Wi-Fi, Bluetooth, BLE, GPS, USB, SD, SNR, RSSI, dBm, ACK, UTC,
  PIN, NTP, CSI, ESP-NOW, OSM, board names, and the device keys written on them (OK, BACK, DEL, HOME, MSG,
  MAP, CTRL, ADV, MIC, Enter, Sym, `@`) are not translated. `RU/EN` names the keyboard layouts and stays.
- **Plurals.** `forms` has the categories of the language (CLDR): `one`/`other`; `one`/`few`/`many`
  (Ukrainian, Polish); `other` only (Chinese, Japanese, Korean, Indonesian); `zero`/`one`/`two`/`few`/`many`/`other`
  (Arabic). Write `{n}` where the number goes: `"{n} Partien"`, `"{n}局"`. An Arabic form may leave the
  number out: `"رسالتان"`.
- **Dates.** `{weekday}, {month} {day}` is the order of the date line; month names are in the form that
  order needs (Russian uses the genitive «января»).
- **Scripts.** Chinese is Simplified; Japanese mixes kana and kanji as usual UIs do; Arabic is Modern
  Standard Arabic without harakat or tatweel, with Western digits; the screens draw Arabic right to left
  inside the usual left-to-right layout. The firmware keeps only the glyphs the translations use, so
  `generate` must run after every change.
- **Tone.** Short, neutral interface language: infinitives or imperatives for actions, no exclamation marks.
  Portuguese is Brazilian (você); Spanish and Italian use tú/tu where a person is addressed; German and
  French address the user formally only when a sentence needs it.

## Website

`site/i18n/<lang>.json` has the keys of `en.json`. Values may hold `<b>`, `<em>` and entities; keep the
same tags. The steps of the installer name buttons of the page (`btn.install`, `nrf.continue`), so use
the same words; the “Erase device” checkbox belongs to ESP Web Tools, which is English only, so keep its
English name. The page sets `dir="rtl"` for Arabic.

## Overview (README)

`README.<lang>.md` translates `README.md`: the same sections, tables, commands and links. The third
line lists every language, the current one in bold. Links to detailed documents go to `docs/en/`.
Code blocks, commands, file names and URLs are not translated.
