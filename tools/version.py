"""Firmware version from include/Version.h, shared by packaging and hardware checks."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = re.search(r'MESHMM_VERSION "([^"]+)"', (ROOT / 'include/Version.h').read_text())[1]
FIRMWARE = 'MeshMesh ' + VERSION
