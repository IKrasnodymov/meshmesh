#!/usr/bin/env python3
"""Check upgrade ordering and prevent publishing APK metadata under the wrong public version."""
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from version import ANDROID_VERSION, android_code, check
from app_release import publish


class VersionChecks(unittest.TestCase):
    def test_upgrade_order(self):
        self.assertLess(52, android_code(53, 1))  # migration from the old run-number-only scheme
        self.assertLess(android_code(53, 1), android_code(53, 2))
        self.assertLess(android_code(53, 99), android_code(54, 1))
        self.assertLess(android_code(0, 0), android_code(1, 1))
        with self.assertRaises(ValueError):
            android_code(1, 100)

    def test_metadata_must_match_apk_version(self):
        with tempfile.TemporaryDirectory() as folder, patch.dict(os.environ, {'GITHUB_ACTIONS': 'false'}):
            out = Path(folder)
            (out / 'meshmesh.apk').write_bytes(b'fixture for metadata/hash validation')
            metadata = out / 'output-metadata.json'
            metadata.write_text(json.dumps({'elements': [{'versionCode': 1005301, 'versionName': 'wrong'}]}))
            with self.assertRaises(ValueError):
                publish(out, metadata)
            self.assertFalse((out / 'version.json').exists())
            metadata.write_text(json.dumps({'elements': [{'versionCode': 1005301, 'versionName': f'{ANDROID_VERSION}+1005301'}]}))
            result = publish(out, metadata)
            self.assertEqual(result['code'], 1005301)
            self.assertEqual((out / result['file']).read_bytes(), (out / 'meshmesh.apk').read_bytes())


if __name__ == '__main__':
    check()
    unittest.main()
