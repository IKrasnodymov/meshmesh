#!/usr/bin/env python3
"""Publishes the Android app's update description: SITE_APP/version.json and a numbered APK copy.

Usage: tools/app_release.py SITE_APP CODE  (SITE_APP holds meshmesh.apk; CODE is the APK's versionCode,
the workflow run number given to Gradle as MM_VERSION_CODE). Read by android/.../Updater.kt.
"""
import hashlib, json, re, shutil, sys
from pathlib import Path

out, code = Path(sys.argv[1]), int(sys.argv[2])
apk = out / 'meshmesh.apk'
firmware = re.search(r'MESHMM_VERSION "([^"]+)"', Path('include/Version.h').read_text()).group(1)
name = f'meshmesh-{code}.apk'
shutil.copyfile(apk, out / name)
data = apk.read_bytes()
info = {'code': code, 'name': f'{firmware}-app{code}', 'file': name, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
(out / 'version.json').write_text(json.dumps(info, ensure_ascii=False) + '\n')
print(json.dumps(info))
