#!/usr/bin/env python3
"""Compatibility entry point; the check now supports every board (tools/power_check.py)."""
from power_check import main, private_json, summary
if __name__ == '__main__':
    main()
