#!/usr/bin/env python3
"""The networks catalogue of the site, the app and the device page: site/networks.json.

A copy of catalog.json of VoxMesh/meshcore-local-networks (radio profiles of local MeshCore networks by city).
`update` fetches the current one, checks it and writes the copy; `check` checks the copy (CI). The pages keep only
profiles they can apply (web/index.html netFits, site/netpick.js), but a malformed catalogue fails here.
"""
import argparse
import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / 'site' / 'networks.json'
URL = 'https://raw.githubusercontent.com/VoxMesh/meshcore-local-networks/main/catalog.json'


def problems(c):
    out = []
    if not isinstance(c, dict) or c.get('schema_version') != 2 or not isinstance(c.get('locations'), list):
        return ['schema_version 2 with a list of locations expected']
    if not isinstance(c.get('updated'), str):
        out.append('updated: date text expected')
    places, nets = set(), set()
    for l in c['locations']:
        lid = l.get('id')
        if not isinstance(lid, str) or lid in places:
            out.append(f'location id {lid!r}: missing or repeated')
        places.add(lid)
        if not isinstance(l.get('name'), str) or not all(isinstance(l.get(k), (int, float)) for k in ('lat', 'lon')):
            out.append(f'{lid}: name, lat and lon expected')
        for n in l.get('networks') or []:
            nid, r = n.get('id'), n.get('radio') or {}
            if not isinstance(nid, str) or nid in nets:
                out.append(f'network id {nid!r}: missing or repeated')
            nets.add(nid)
            if not (isinstance(r.get('frequency_hz'), int) and 150_000_000 <= r['frequency_hz'] <= 2_500_000_000
                    and isinstance(r.get('bandwidth_hz'), int) and 7_000 <= r['bandwidth_hz'] <= 500_000
                    and r.get('sf') in range(5, 13) and r.get('cr') in range(5, 9)):
                out.append(f'{nid}: radio out of range {r}')
            url = n.get('community_url')
            if url is not None and not (isinstance(url, str) and url.startswith(('https://', 'http://'))):
                out.append(f'{nid}: community_url must be http(s)')
            if n.get('path_hash_bytes') not in (None, 1, 2, 3):
                out.append(f"{nid}: path_hash_bytes must be 1, 2 or 3, not {n['path_hash_bytes']!r}")
    for l in c['locations']:
        if l.get('parent_id') is not None and l['parent_id'] not in places:
            out.append(f"{l.get('id')}: unknown parent_id {l['parent_id']}")
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('action', choices=['update', 'check'])
    a = p.parse_args()
    if a.action == 'update':
        with urllib.request.urlopen(URL, timeout=30) as r:
            c = json.load(r)
    else:
        c = json.loads(TARGET.read_text())
    bad = problems(c)
    if bad:
        print('\n'.join(bad), file=sys.stderr)
        return 1
    if a.action == 'update':
        TARGET.write_text(json.dumps(c, ensure_ascii=False, indent=1) + '\n')
    n = sum(len(l.get('networks') or []) for l in c['locations'])
    print(f"{TARGET.relative_to(ROOT)}: {c['updated']}, {len(c['locations'])} locations, {n} networks")
    return 0


if __name__ == '__main__':
    sys.exit(main())
