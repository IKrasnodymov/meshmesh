# PlatformIO extra script of the nRF52 boards: MM_LANG=<code> builds the image with that screen language
# (src/I18n.cpp keeps English, Russian and MM_LANG_EXTRA). Only I18n.cpp gets the define, so a build per
# language recompiles one file and links (tools/nrf52.py package). Without MM_LANG: English and Russian.
import os
import sys

Import('env')  # noqa: F821
sys.path.insert(0, os.path.join(env.subst('$PROJECT_DIR'), 'tools'))  # noqa: F821
from i18n import CODES  # noqa: E402

code = os.environ.get('MM_LANG', '')
if code and code not in CODES:
    sys.exit(f'MM_LANG={code}: not one of {" ".join(CODES)}')


def language(env, node):
    if node.name != 'I18n.cpp' or code in ('', 'en', 'ru'):
        return node
    return env.Object(node, CPPDEFINES=list(env['CPPDEFINES']) + [('MM_LANG_EXTRA', CODES.index(code))])


env.AddBuildMiddleware(language)  # noqa: F821
