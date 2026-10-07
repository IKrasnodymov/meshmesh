# PlatformIO extra script of the nRF52 boards: MM_LANG=<code> builds the image with that screen language:
# English and that language, without the Russian strings (MM_NO_RU in every file: tr() in I18n.h leaves them
# out). Only I18n.cpp gets the language number (MM_LANG_EXTRA), so after the first language build in the
# language build folder (tools/nrf52.py package) each next one recompiles one file and links. Without
# MM_LANG: English and Russian.
import os
import sys

Import('env')  # noqa: F821
sys.path.insert(0, os.path.join(env.subst('$PROJECT_DIR'), 'tools'))  # noqa: F821
from i18n import CODES  # noqa: E402

code = os.environ.get('MM_LANG', '')
if code and code not in CODES:
    sys.exit(f'MM_LANG={code}: not one of {" ".join(CODES)}')


if code and code not in ('en', 'ru'):
    env.Append(CPPDEFINES=[('MM_NO_RU', 1)])  # noqa: F821


def language(env, node):
    if not hasattr(node, 'name') or node.name != 'I18n.cpp' or code in ('', 'en', 'ru'):
        return node
    return env.Object(node, CPPDEFINES=list(env['CPPDEFINES']) + [('MM_LANG_EXTRA', CODES.index(code))])


env.AddBuildMiddleware(language)  # noqa: F821
