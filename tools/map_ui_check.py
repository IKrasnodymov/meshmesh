#!/usr/bin/env python3
"""Exercise stored maps and M9 user flows on hardware, including radio while locked."""
import base64,json,time,zlib,struct
from contextlib import ExitStack
from pathlib import Path
from device import connect,command,screenshot
from radio_check import apply,delivery

def read(d,name):return json.loads(command(d,name))
def ok(d,text):
 r=command(d,text)
 assert r.startswith('OK'),r
 return r

def key(d,n):ok(d,f'uikey {n}')
def wake(d):
 if read(d,'ui')['locked']:key(d,0xa3)
 key(d,0x82);key(d,0x82)
def capture(d,name):
 time.sleep(.2)
 ok(d,'baud 921600');d.baudrate=921600
 try:screenshot(d,f'artifacts/{name}.ppm')
 finally:ok(d,'baud 115200');d.baudrate=115200

def main():
 with ExitStack() as stack:
  m9=stack.enter_context(connect('/dev/cu.wchusbserial10'))
  heltec=stack.enter_context(connect('/dev/cu.usbmodem1101'))
  config=read(m9,'config');before=[read(d,'status') for d in (m9,heltec)]
  nav=read(m9,'navigation');areas=read(m9,'map areas');checks=[]
  assert nav['calibrated'] and not nav['calibrating'],'Saved physical compass calibration missing'
  assert sorted(a['tiles'] for a in areas)==[218,1022],areas
  city=next(a for a in areas if a['tiles']==1022)
  try:
   apply(m9,{'auto_lock':0,'dim_after':0})
   wake(m9);capture(m9,'m9-home-0.3')
   ok(m9,'map select '+city['id']);key(m9,0x85)
   ok(m9,'map view '+json.dumps({'latitude':city['latitude'],'longitude':city['longitude'],'zoom':14,'follow':False}))
   deadline=time.monotonic()+30
   while time.monotonic()<deadline:
    info=read(m9,'map info')
    if info['cached']:break
    time.sleep(.3)
   else:raise TimeoutError('Stored city map not rendered from SD')
   time.sleep(2);capture(m9,'m9-map-city')
   old=read(m9,'map info');key(m9,0xb7);new=read(m9,'map info')
   assert new['longitude']>old['longitude'] and not new['follow']
   key(m9,ord('+'));assert read(m9,'map info')['zoom']==15
   key(m9,ord('-'));assert read(m9,'map info')['zoom']==14
   key(m9,ord('L'));assert read(m9,'ui')['page']=='library';capture(m9,'m9-map-library')
   key(m9,13);assert read(m9,'ui')['page']=='map'
   key(m9,13);assert read(m9,'map info')['follow']
   ok(m9,'map select '+city['id'])
   old=read(m9,'map info')
   for value in ({'latitude':10,'longitude':20,'zoom':99},{'latitude':10},{'follow':1},{'zoom':14.5},{'foo':0}):
    assert command(m9,'map view '+json.dumps(value)).startswith('ERR')
    new=read(m9,'map info')
    assert all(old[k]==new[k] for k in ('latitude','longitude','zoom','follow'))
   checks+=['SD map render','pan and zoom','saved-area library','GPS follow','atomic invalid map-view rejection']
   # A world-edge fixture is outside every saved area. Corrupt replacements
   # must preserve its complete prior tile and must not create a catalog entry.
   data=struct.pack('<HHHH',65535,0,1,0);crc=zlib.crc32(data)
   tile={'z':17,'x':1,'y':1,'size':len(data),'crc':crc}
   ok(m9,'map begin '+json.dumps(tile));ok(m9,'map chunk '+base64.b64encode(data).decode());ok(m9,'map finish')
   has=f'map has 17 1 1 {len(data)} {crc}'
   assert command(m9,has)=='OK map tile exists'
   for mode in ('crc','incomplete','compressed','rle'):
    if mode=='compressed':
     ok(m9,'map zbegin '+json.dumps({**tile,'packed_size':4}));chunk=b'abcd'
    else:
     ok(m9,'map begin '+json.dumps({**tile,'crc':crc^1 if mode=='crc' else crc}));chunk=data[:4] if mode=='incomplete' else b'\x00\x00\x00\x00' if mode=='rle' else data
    result=command(m9,'map chunk '+base64.b64encode(chunk).decode())
    if mode=='rle':assert result.startswith('ERR')
    else:assert result.startswith('OK') and command(m9,'map finish').startswith('ERR')
    assert command(m9,has)=='OK map tile exists'
    assert not read(m9,'map info')['uploading']
   assert sorted(read(m9,'map areas'),key=lambda a:a['id'])==sorted(areas,key=lambda a:a['id'])
   checks+=['CRC, incomplete, malformed zlib and RLE rejected; complete prior tile retained']
   for patch in ({'brightness':100,'sf':20},{'dim_after':5},{'auto_lock':1},{'utc_offset':1},{'russian':1}):
    old=read(m9,'config');assert command(m9,'set '+json.dumps(patch)).startswith('ERR');assert read(m9,'config')==old
   checks+=['invalid settings rejected without partial changes']
   # Keyboard language is independent of the interface. Keep drafts intact.
   key(m9,0x81);key(m9,13);ui=read(m9,'ui');initial=ui['composer'];kb=ui['keyboard_language']
   if kb=='RU':key(m9,0x83)
   draft=' draft 123'
   for c in draft:key(m9,ord(c))
   assert read(m9,'ui')['composer']==initial+draft
   key(m9,0x86);key(m9,13);assert read(m9,'ui')['composer']==initial+draft
   key(m9,0x88);assert read(m9,'ui')['locked'];key(m9,ord('x'));assert read(m9,'ui')['composer']==initial+draft
   capture(m9,'m9-locked')
   radio=delivery(heltec,m9,before[1]['node'],before[0]['node'],f'LoRa: locked screen {time.time_ns()}')
   assert read(m9,'ui')['locked'];key(m9,0xa3);assert not read(m9,'ui')['locked']
   assert read(m9,'ui')['composer']==initial+draft
   for _ in draft:key(m9,8)
   if kb=='RU':key(m9,0x83)
   assert read(m9,'ui')['composer']==initial and read(m9,'config')['russian']==config['russian']
   if len(initial.encode())+14<=151:
    if kb=='EN':key(m9,0x83)
    for c in '{}:"<>~':key(m9,ord(c))
    assert read(m9,'ui')['composer']==initial+'ХЪЖЭБЮЁ'
    for _ in range(7):key(m9,8)
    if kb=='EN':key(m9,0x83)
    checks+=['RU shifted punctuation keys produce uppercase Cyrillic; UTF-8 backspace']
   checks+=['draft retained across BACK and lock','independent keyboard language','locked keyboard ignored','physical LoRa ACK while locked']
   # Real inactivity timers: first wake key restores the screen, next navigates.
   apply(m9,{'auto_lock':30,'dim_after':10});key(m9,0x81);key(m9,13)
   time.sleep(11);assert not read(m9,'ui')['locked']
   key(m9,0x82);assert read(m9,'ui')['page']=='chat'
   key(m9,0x82);assert read(m9,'ui')['page']=='home'
   deadline=time.monotonic()+33
   while time.monotonic()<deadline:
    if read(m9,'ui')['locked']:break
    time.sleep(.5)
   else:raise TimeoutError('30-second screen auto-lock did not activate')
   key(m9,0xa3);assert not read(m9,'ui')['locked']
   checks+=['10-second dim/wake behavior','30-second automatic lock and hold-OK unlock']
   # Open each settings section without touching saved values or calibration.
   for index,name in enumerate(('radio','display','sensors','network','diagnostics','help')):
    key(m9,0x90)
    for _ in range(index):key(m9,0xb6)
    key(m9,13);assert read(m9,'ui')['page']==name
    if name in ('radio','display','sensors','help'):capture(m9,'m9-'+name)
   checks+=['radio/device/sensors/network/diagnostics/help navigation']
  finally:
   apply(m9,{'auto_lock':config['auto_lock'],'dim_after':config['dim_after']})
   ok(m9,'map select '+city['id']);wake(m9);key(m9,0x85)
  after=[read(d,'status') for d in (m9,heltec)]
  assert [s['boot'] for s in before]==[s['boot'] for s in after]
  assert [s['diagnostic_rx'] for s in before]==[s['diagnostic_rx'] for s in after]
  assert read(m9,'config')==config and read(m9,'navigation')['calibrated']
  report={'input':'production UI using simulated USB key events; not physical-button automation','checks':checks,'areas':areas,'locked_radio':radio,'before':before,'after':after}
  p=Path('artifacts/map-ui-check.json');p.touch(mode=0o600,exist_ok=True);p.chmod(0o600);p.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
  print('PASS SD maps, damaged transfers, drafts, screen lock, inactivity timers, settings and physical LoRa while locked')
if __name__=='__main__':main()
