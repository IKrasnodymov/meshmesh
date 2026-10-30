# PlatformIO extra script: the device page for the firmware's web server, made at build time as PortalWeb.h
# in the build folder: web/index.html, the page's dictionaries (i18n/web/<lang>.json) and the networks catalogue
# (site/networks.json), each compressed with gzip and sent with Content-Encoding: gzip (src/Portal.cpp). The header
# is rewritten only when its contents change.
# The dictionaries share their keys, the Russian text, which is most of each: i18n/keys.js has them once, and
# i18n/<lang>.js the translations in their order; it loads keys.js and makes globalThis.MeshMeshWebDict as
# tools/web_i18n.cjs build does (about 300 KB less flash than whole dictionaries).
import gzip
import json
import os

try:
    Import('env')  # noqa: F821
except NameError:  # imported by a check, not run by PlatformIO
    env = None
LANGS = ['en', 'uk', 'es', 'pt', 'fr', 'de', 'it', 'pl', 'tr', 'zh', 'ja', 'ko', 'ar', 'id']


def packed(data):
    return gzip.compress(data, compresslevel=9, mtime=0)


def array(name, data):
    rows = ','.join(str(b) for b in data)
    return f'static const uint8_t {name}[] PROGMEM={{{rows}}};\n'


def js(value):
    return json.dumps(value, ensure_ascii=False, separators=(',', ':')).replace('<', '\\u003c')


def dictionaries(root):
    """i18n/keys.js and {lang: i18n/<lang>.js}; a translation left empty is 0 and stays out of the dictionary."""
    found = {}
    for code in LANGS:
        with open(os.path.join(root, 'i18n', 'web', code + '.json'), encoding='utf-8') as f:
            found[code] = {k: v for k, v in json.load(f).items() if (v if isinstance(v, str) else any(v.values()))}
    keys = sorted(set().union(*found.values()))
    files = {}
    for code, d in found.items():
        files[code] = ('globalThis.MeshMeshWebValues=' + js([d.get(k, 0) for k in keys]) + ';document.write(\'<script src="i18n/keys.js"><\\/script>'
                       '<script>{const d={},v=MeshMeshWebValues;MeshMeshWebKeys.forEach((k,i)=>{if(v[i])d[k]=v[i]});globalThis.MeshMeshWebDict=d}<\\/script>\');\n').encode()
    return ('globalThis.MeshMeshWebKeys=' + js(keys) + ';\n').encode(), files


def build(env):
    root = env.subst('$PROJECT_DIR')  # noqa: F821
    out_dir = os.path.join(env.subst('$BUILD_DIR'), 'web')  # noqa: F821
    with open(os.path.join(root, 'web', 'index.html'), 'rb') as f:
        page = f.read()
    header = ['#pragma once\n// Made by tools/pio_web.py: web/index.html and i18n/web, gzip.\n#include <stdint.h>\n#include <stddef.h>\n',
              array('portalPage', packed(page)), 'static const size_t portalPageSize=sizeof(portalPage);\n']
    langs = []
    keys, files = dictionaries(root)
    header.append(array('portalKeys', packed(keys)))
    for code in LANGS:
        header.append(array('portalLang_' + code, packed(files[code])))
        langs.append(f'{{"{code}",portalLang_{code},sizeof(portalLang_{code})}}')
    with open(os.path.join(root, 'site', 'networks.json'), encoding='utf-8') as f:
        networks = json.dumps(json.load(f), ensure_ascii=False, separators=(',', ':')).encode()
    header.append(array('portalNetworks', packed(networks)))
    header.append('struct PortalLang{const char* code;const uint8_t* data;size_t size;};\n')
    header.append('static const PortalLang portalLangs[]={' + ','.join(langs) + '};\n')
    text = ''.join(header)

    os.makedirs(out_dir, exist_ok=True)
    target = os.path.join(out_dir, 'PortalWeb.h')
    if not os.path.exists(target) or open(target).read() != text:
        with open(target, 'w') as f:
            f.write(text)
    env.Append(CPPPATH=[out_dir])  # noqa: F821


if env is not None:
    build(env)
