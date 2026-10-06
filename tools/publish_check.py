#!/usr/bin/env python3
"""Reject stale/mixed site metadata and Android downgrade before publishing GitHub Pages."""
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from package import TARGETS
from version import VERSION, ANDROID_VERSION

root = Path(sys.argv[1])
release = json.loads((root / 'release.json').read_text())
app = json.loads((root / 'app/version.json').read_text())
if release['firmware'] != VERSION or app['version'] != ANDROID_VERSION:
    raise SystemExit('Site versions differ from canonical versions')
if set(release['boards']) != set(TARGETS) or release['revision'] != app['revision']:
    raise SystemExit('Missing board or mixed source revisions')
if os.environ.get('GITHUB_ACTIONS') == 'true':
    if release['revision'] != os.environ['GITHUB_SHA']:
        raise SystemExit('Site does not match this clean commit')
    request = urllib.request.Request('https://ikrasnodymov.github.io/meshmesh/app/version.json?check=' + str(time.time_ns()),
                                     headers={'Cache-Control': 'no-cache'})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            previous = json.load(response)
        if app['code'] <= previous['code']:
            raise SystemExit('Refusing an Android downgrade or repeated build code')
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise
print('PASS complete site, consistent versions/revisions, Android upgrade ordering')
