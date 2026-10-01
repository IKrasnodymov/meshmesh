#!/usr/bin/env python3
"""USB access to MeshMesh with board-specific control lines to avoid reset."""
import argparse
import json
import time
from pathlib import Path
import serial
from serial.tools.list_ports import comports
from ports import M9_PORT, HELTEC_PORT

def connect(port):
    s=serial.Serial(port=None,baudrate=115200,timeout=.5,exclusive=True)
    native=any(p.device==port and p.vid==0x303a and p.pid==0x1001 for p in comports())
    s.port=port;s.dtr=native;s.rts=False;s.meshmesh_native=native;s.open()
    return s

def command(s,text,timeout=8):
    s.reset_input_buffer();s.write((text+'\n').encode());s.flush()
    deadline=time.monotonic()+timeout
    pending=bytearray()
    last_line=''
    json_response=text in ('status','config','key','messages','nodes','ui','navigation','connections','clock','bleprobe','wifiprobe','map info','map areas','radar','radar web','internet','chess','chess web','role','server','server secrets') or text.startswith('chess show ')
    ping_at=time.monotonic()+.4
    while time.monotonic()<deadline:
        pending.extend(s.read(max(1,min(4096,s.in_waiting))))
        # Native USB can suspend between short replies on macOS. CR wakes the
        # link without executing another command or changing the current one.
        if getattr(s,'meshmesh_native',False) and time.monotonic()>=ping_at:
            s.write(b'\r');ping_at=time.monotonic()+.4
        while b'\n' in pending:
            raw,_,tail=pending.partition(b'\n');pending=bytearray(tail)
            line=raw.decode('utf-8','replace').strip()
            if not line:continue
            last_line=line
            if line.startswith(('ERR','Commands:')):
                if json_response:raise RuntimeError(line)
                return line
            if line.startswith('OK') and not json_response:return line
            if json_response and line.startswith(('{','[')):
                try:json.loads(line);return line
                except json.JSONDecodeError:continue
            if text=='txframe' and all(c in '0123456789abcdef' for c in line):return line
            if text.startswith('server cli '):return line # MeshCore CLI replies have no OK/ERR prefix
    # Report the operation without printing credentials or a partial JSON body.
    raise TimeoutError(f'No response from MeshMesh to {text.split()[0]} '
                       f'({len(pending)} buffered bytes; last line {last_line[:3]!r})')

def screenshot(s,target):
    s.reset_input_buffer();s.write(b'screenshot\n');s.flush()
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        line=s.readline()
        if line.startswith(b'RGB565 '):break
    else:raise TimeoutError('No framebuffer header')
    _,width,height,size=line.split();width=int(width);height=int(height);size=int(size)
    if not (1<=width<=320 and 1<=height<=320 and size==width*height*2):raise ValueError('Unexpected framebuffer dimensions')
    data=bytearray();deadline=time.monotonic()+30
    while len(data)<size and time.monotonic()<deadline:data.extend(s.read(min(4096,size-len(data))))
    if len(data)!=size:raise TimeoutError(f'Incomplete framebuffer: {len(data)}/{size}')
    rgb=bytearray()
    for i in range(0,size,2):
        value=data[i]|data[i+1]<<8
        rgb.extend(((value>>11)*255//31,((value>>5)&63)*255//63,(value&31)*255//31))
    target=Path(target);target.parent.mkdir(parents=True,exist_ok=True)
    target.touch(mode=0o600,exist_ok=True);target.chmod(0o600)
    target.write_bytes(f'P6\n{width} {height}\n255\n'.encode()+rgb)
    return str(target)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',default=M9_PORT)
    p.add_argument('--screenshot',metavar='FILE.ppm');p.add_argument('--output',type=Path);p.add_argument('command',nargs='?',default='status');a=p.parse_args()
    with connect(a.port) as s:
        if a.screenshot:print(screenshot(s,a.screenshot));return
        value=command(s,a.command)
    if a.output:
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text(value+'\n');a.output.chmod(0o600);print(f'Saved to {a.output}')
    else:
        try:print(json.dumps(json.loads(value),ensure_ascii=False,indent=2))
        except json.JSONDecodeError:print(value)

if __name__=='__main__':main()
