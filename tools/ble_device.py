#!/usr/bin/env python3
"""MeshMesh BLE client; macOS asks for the PIN displayed on the device."""
import argparse
import asyncio
import json
from pathlib import Path
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

SERVICE = '7a9e0001-98bd-4d56-89a8-c4eab4179010'
RX = '7a9e0002-98bd-4d56-89a8-c4eab4179010'
TX = '7a9e0003-98bd-4d56-89a8-c4eab4179010'

async def run(args):
    # macOS may cache the name of the previous firmware; match advertisement data.
    def match(device, advertisement):
        return SERVICE in advertisement.service_uuids and args.node.upper() in (advertisement.local_name or '').upper()
    device = await BleakScanner.find_device_by_filter(match, timeout=15, service_uuids=[SERVICE])
    if device is None:
        raise RuntimeError('MeshMesh BLE advertisement not found; enable BLE on the device')
    response = asyncio.get_running_loop().create_future()
    data = bytearray()
    def received(_, fragment):
        data.extend(fragment)
        if b'\n' in data and not response.done():
            response.set_result(bytes(data).split(b'\n', 1)[0].decode('utf-8'))
    async with BleakClient(device, timeout=30) as client:
        print('Connected; enter the device PIN if macOS asks to pair.', flush=True)
        # CoreBluetooth initiates pairing on a protected read. A write can fail
        # immediately while the security procedure from subscription is pending.
        deadline = asyncio.get_running_loop().time() + 100
        while True:
            try:
                await client.read_gatt_char(TX)
                break
            except (TimeoutError, BleakError) as error:
                if isinstance(error, BleakError) and 'Encryption is insufficient' not in str(error):
                    raise
                if asyncio.get_running_loop().time() >= deadline:
                    raise TimeoutError('Pairing did not finish; enter the device PIN and retry') from error
                await asyncio.sleep(1)
        await client.start_notify(TX, received)
        encoded = args.command.encode('utf-8')
        if len(encoded) > 255:
            raise ValueError('BLE command exceeds 255 UTF-8 bytes')
        await asyncio.wait_for(client.write_gatt_char(RX, encoded, response=True), 100)
        value = await asyncio.wait_for(response, 45)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.touch(mode=0o600, exist_ok=True)
            args.output.chmod(0o600)
            args.output.write_text(value + '\n')
            print(f'Saved {args.output}')
        else:
            try:
                print(json.dumps(json.loads(value), ensure_ascii=False, indent=2))
            except json.JSONDecodeError:
                print(value)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--node', default='AD4F7C', help='Six trailing hex digits displayed in the BLE name')
    parser.add_argument('--output', type=Path)
    parser.add_argument('command', nargs='?', default='status')
    try:
        asyncio.run(run(parser.parse_args()))
    except (BleakError, TimeoutError, RuntimeError) as error:
        raise SystemExit('BLE: ' + str(error)) from None

if __name__ == '__main__':
    main()
