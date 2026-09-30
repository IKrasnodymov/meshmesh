"""USB ports of the two boards. Names change after reconnection, so find each
board by its USB bridge: exactly one CH340 (M9) and one ESP32-S3 native USB
(Heltec). MESHMESH_M9 / MESHMESH_HELTEC override; otherwise the last known name
is kept. flash.py still verifies chip model and MAC before writing."""
import os
from serial.tools.list_ports import comports


def _find(variable, vid, pid, fallback):
    if os.environ.get(variable):
        return os.environ[variable]
    found = [p.device for p in comports() if p.vid == vid and p.pid == pid and p.device.startswith('/dev/cu.')]
    return found[0] if len(found) == 1 else fallback


M9_PORT = _find('MESHMESH_M9', 0x1a86, 0x7522, '/dev/cu.wchusbserial10')
HELTEC_PORT = _find('MESHMESH_HELTEC', 0x303a, 0x1001, '/dev/cu.usbmodem1101')
