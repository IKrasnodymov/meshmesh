#!/usr/bin/env python3
"""Publish Android update metadata from Gradle output-metadata.json, never a guessed version.

Usage: tools/app_release.py SITE_APP OUTPUT_METADATA_JSON
SITE_APP contains meshmesh.apk. The APK and metadata must come from the same Gradle artifact.
"""
import hashlib
import json
import os
import shutil
import sys
from pathlib import Path
from version import VERSION, ANDROID_VERSION, android_code, revision


def publish(out, metadata):
    elements = json.loads(metadata.read_text())['elements']
    if len(elements) != 1:
        raise ValueError('Exactly one universal APK is required')
    code, name = elements[0]['versionCode'], elements[0]['versionName']
    if not isinstance(code, int) or not 0 < code <= 2100000000:
        raise ValueError('Invalid APK versionCode')
    if name not in (f'{ANDROID_VERSION}+{code}', f'{ANDROID_VERSION}+local'):
        raise ValueError('APK and canonical Android versions differ')
    if os.environ.get('GITHUB_ACTIONS') == 'true' and (code != android_code() or name.endswith('+local')):
        raise ValueError('APK build code does not match this workflow run/attempt')
    apk = out / 'meshmesh.apk'
    data = apk.read_bytes()
    filename = f'meshmesh-{ANDROID_VERSION}-{code}.apk'
    shutil.copyfile(apk, out / filename)
    info = {'code': code, 'name': name, 'version': ANDROID_VERSION, 'firmware': VERSION,
            'revision': revision(), 'file': filename, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    (out / 'version.json').write_text(json.dumps(info, ensure_ascii=False) + '\n')
    return info


if __name__ == '__main__':
    print(json.dumps(publish(Path(sys.argv[1]), Path(sys.argv[2]))))
