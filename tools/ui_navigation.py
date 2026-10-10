"""Navigate compact production UI without relying on an old app list."""
import json,time
from device import command

def read(d,name):return json.loads(command(d,name,timeout=25))
def key(d,value):
 assert command(d,f'uikey {value}',timeout=25).startswith('OK')
 time.sleep(.1)

def compact_home(d):
 # BACK wakes a dark panel without rolling dice or changing a manual counter.
 key(d,0x86)
 ui=read(d,'ui')
 if ui.get('menu'):
  n=ui['menu_count']
  for _ in range((n-1-ui['menu_index'])%n):key(d,13)
  key(d,0xa3)
 key(d,0x86);key(d,0x86)
 assert read(d,'ui')['page']=='home'

def compact_open(d,page,app):
 original=read(d,'config')['apps']
 order=app+' '+' '.join(t for t in original.split() if t.lstrip('-')!=app)
 try:
  assert command(d,'set '+json.dumps({'apps':order}),timeout=25).startswith('OK')
  compact_home(d);key(d,13)
  assert read(d,'ui')['page']==page
 finally:
  assert command(d,'set '+json.dumps({'apps':original}),timeout=25).startswith('OK')
  assert read(d,'config')['apps']==original
