"""Check canonical versions and embed the source revision in status (only App.cpp recompiles)."""
import json
import os
import sys
from pathlib import Path

Import('env')
sys.path.insert(0, os.path.join(env.subst('$PROJECT_DIR'), 'tools'))
from version import VERSION, check, revision
check()
rev = revision()
folder = Path(env.subst('$BUILD_DIR'))
folder.mkdir(parents=True, exist_ok=True)
(folder / 'release.json').write_text(json.dumps({'version': VERSION, 'revision': rev}) + '\n')


def identify(env, node):
    if not hasattr(node, 'name') or node.name != 'App.cpp':
        return node
    return env.Object(node, CPPDEFINES=list(env['CPPDEFINES']) + [('MESHMM_REVISION', '\\"' + rev + '\\"')])


env.AddBuildMiddleware(identify)
