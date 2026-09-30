#!/usr/bin/env python3
"""Build offline street maps from OSM vector data and preload MeshMesh over USB."""
import argparse,base64,json,math,struct,time,urllib.request,urllib.parse,zlib
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from device import connect,command
from ports import M9_PORT, HELTEC_PORT
MAGIC=b'MMAP1\n'
ATTRIBUTION='© OpenStreetMap contributors · ODbL 1.0 · openstreetmap.org/copyright'

def pixel(lat,lon,z):
 lat=max(-85.05112878,min(85.05112878,lat));n=256*(1<<z)
 return (lon+180)/360*n,(1-math.asinh(math.tan(math.radians(lat)))/math.pi)/2*n

def coordinates(x,y,z):
 n=256*(1<<z);return math.degrees(math.atan(math.sinh(math.pi*(1-2*y/n)))),x/n*360-180

def encode(image):
 pixels=[]
 for r,g,b in image.convert('RGB').getdata():pixels.append((r>>3)<<11|(g>>2)<<5|(b>>3))
 output=bytearray();previous=pixels[0];count=0
 for p in pixels:
  if p!=previous or count==65535:output+=struct.pack('<HH',count,previous);previous=p;count=0
  count+=1
 output+=struct.pack('<HH',count,previous)
 return bytes(output)

def decode(data):
 if not len(data) or len(data)%4:raise ValueError('Invalid RLE size')
 pixels=[]
 for count,color in struct.iter_unpack('<HH',data):
  if count==0 or len(pixels)+count>65536:raise ValueError('Invalid RLE run')
  pixels.extend([color]*count)
 if len(pixels)!=65536:raise ValueError('Incomplete tile')
 return pixels

def pack_write(path,manifest,records):
 header=json.dumps(manifest,ensure_ascii=False,separators=(',',':')).encode();out=bytearray(MAGIC+struct.pack('<I',len(header))+header)
 for z,x,y,data in records:out+=struct.pack('<BIIII',z,x,y,len(data),zlib.crc32(data))+data
 path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(out);return len(out)

def pack_read(path):
 raw=path.read_bytes()
 if raw[:6]!=MAGIC or len(raw)<10:raise ValueError('Not a MeshMesh map package')
 n=struct.unpack_from('<I',raw,6)[0]
 if n>2048 or len(raw)<10+n:raise ValueError('Invalid manifest size')
 manifest=json.loads(raw[10:10+n]);records=[];offset=10+n
 while offset<len(raw):
  if offset+17>len(raw):raise ValueError('Truncated tile header')
  z,x,y,n,crc=struct.unpack_from('<BIIII',raw,offset);offset+=17
  if not 10<=z<=17 or x>=1<<z or y>=1<<z or n>262144 or offset+n>len(raw):raise ValueError('Invalid tile coordinates or length')
  data=raw[offset:offset+n];offset+=n
  if zlib.crc32(data)!=crc:raise ValueError('Tile checksum mismatch')
  decode(data);records.append((z,x,y,data))
 if len(records)!=manifest.get('tiles'):raise ValueError('Tile count mismatch')
 return manifest,records

def download(bounds,path,endpoint,buildings=False):
 s,w,n,e=bounds
 bbox=','.join(f'{v:.7f}' for v in bounds)
 query=f'[out:json][timeout:180];(way[highway]({bbox});way[waterway]({bbox});way[natural=water]({bbox});way[landuse~"forest|grass|meadow|recreation_ground"]({bbox});way[leisure=park]({bbox});way[railway=rail]({bbox});node[place]({bbox}););out geom;'
 if buildings:query=query.replace('node[place]',f'way[building]({bbox});node[place]')
 request=urllib.request.Request(endpoint,data=urllib.parse.urlencode({'data':query}).encode(),headers={'User-Agent':'MeshMesh/0.2 offline maps (personal bounded-area download)'})
 with urllib.request.urlopen(request,timeout=240) as response:data=response.read(120_000_001)
 if len(data)>120_000_000:raise ValueError('Area too large; maximum 120 MB of OSM data')
 parsed=json.loads(data)
 if parsed.get('remark'):raise ValueError('Overpass did not complete: '+parsed['remark'])
 path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);return parsed

def render(osm,bounds,zooms):
 s,w,n,e=bounds
 font_paths=['/System/Library/Fonts/Supplemental/Arial.ttf','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']
 font=next((ImageFont.truetype(p,11) for p in font_paths if Path(p).exists()),ImageFont.load_default())
 for z in zooms:
  lx,by=pixel(s,w,z);rx,ty=pixel(n,e,z);x0,x1=int(lx//256),int(rx//256);y0,y1=int(ty//256),int(by//256)
  buckets={(x,y):[] for y in range(y0,y1+1) for x in range(x0,x1+1)}
  for element in osm['elements']:
   tags=element.get('tags',{});geometry=element.get('geometry')
   if not geometry:
    if 'lat' not in element or 'place' not in tags:continue
    geometry=[{'lat':element['lat'],'lon':element['lon']}]
   pts=[pixel(g['lat'],g['lon'],z) for g in geometry if 'lat' in g]
   if not pts:continue
   a,b,c,d=min(p[0] for p in pts),min(p[1] for p in pts),max(p[0] for p in pts),max(p[1] for p in pts)
   for y in range(max(y0,int(b//256)),min(y1,int(d//256))+1):
    for x in range(max(x0,int(a//256)),min(x1,int(c//256))+1):buckets[x,y].append((tags,pts))
  for (x,y),features in buckets.items():
   image=Image.new('RGB',(256,256),'#f3efdf');draw=ImageDraw.Draw(image)
   for tags,pts in features:
    local=[(p[0]-x*256,p[1]-y*256) for p in pts]
    if len(local)>2 and pts[0]==pts[-1] and ('natural' in tags or 'landuse' in tags or 'leisure' in tags or (z>=15 and 'building' in tags)):draw.polygon(local,fill='#a4cbd6' if tags.get('natural')=='water' else '#d8d0bf' if 'building' in tags else '#c4d7ad')
   for tags,pts in features:
    local=[(p[0]-x*256,p[1]-y*256) for p in pts]
    if len(local)<2:continue
    road=tags.get('highway')
    if road:
     major=road in ['motorway','trunk','primary','secondary'];width=(4 if major else 2) if z>=14 else (3 if major else 1)
     draw.line(local,fill='#c3baaa',width=width+2);draw.line(local,fill='#ffd591' if major else '#ffffff',width=width)
    elif 'waterway' in tags:draw.line(local,fill='#86bbd0',width=2)
    elif 'railway' in tags:draw.line(local,fill='#a79b8b',width=1)
   occupied=[]
   for tags,pts in features:
    name=tags.get('name:ru',tags.get('name',''))
    if not name or ('place' not in tags and (z<14 or tags.get('highway') not in ['primary','secondary','tertiary','residential'])):continue
    p=pts[len(pts)//2];a,b=p[0]-x*256,p[1]-y*256
    if not 8<=a<248 or not 8<=b<248:continue
    name=name[:32];box=draw.textbbox((a,b),name,font=font);box=(box[0]-2,box[1]-2,box[2]+2,box[3]+2)
    if any(not(box[2]<q[0] or box[0]>q[2] or box[3]<q[1] or box[1]>q[3]) for q in occupied):continue
    occupied.append(box);draw.rectangle(box,fill='#f3efdf');draw.text((a,b),name,font=font,fill='#405047')
   yield z,x,y,encode(image)

def upload(path,port):
 manifest,records=pack_read(path);started=time.monotonic()
 with connect(port) as device:
  info=json.loads(command(device,'map info'))
  if not info['available']:raise RuntimeError('M9 SD unavailable')
  command(device,'map cancel')
  fast=False
  if 'wchusbserial' in port:
   reply=command(device,'baud 921600')
   if reply.startswith('OK'):
    device.baudrate=921600;time.sleep(.05);fast=True
    assert json.loads(command(device,'status'))['radio']
  try:
   for i,(z,x,y,data) in enumerate(records):
    def ok(text):
     reply=command(device,text,timeout=20)
     if not reply.startswith('OK'):raise RuntimeError(reply)
    exists=command(device,f'map has {z} {x} {y} {len(data)} {zlib.crc32(data)}')
    if exists=='OK map tile exists':continue
    packed=zlib.compress(data,6) if info.get('compressed_upload') else data
    options=dict(z=z,x=x,y=y,size=len(data),crc=zlib.crc32(data))
    if info.get('compressed_upload'):options['packed_size']=len(packed)
    ok(('map zbegin ' if info.get('compressed_upload') else 'map begin ')+json.dumps(options,separators=(',',':')))
    for n in range(0,len(packed),512):ok('map chunk '+base64.b64encode(packed[n:n+512]).decode())
    ok('map finish')
    if i%10==0 or i+1==len(records):print(f'Uploaded {i+1}/{len(records)} tiles ({time.monotonic()-started:.0f}s)',flush=True)
   ok('map manifest '+json.dumps(manifest,ensure_ascii=False,separators=(',',':')))
  except BaseException:
   command(device,'map cancel');raise
  finally:
   if fast:
    command(device,'baud 115200');device.baudrate=115200;time.sleep(.05)
 print('PASS map package uploaded and committed to SD',flush=True)

def main():
 p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
 build=sub.add_parser('build');build.add_argument('--latitude',type=float,required=True);build.add_argument('--longitude',type=float,required=True);build.add_argument('--radius',type=float,default=2);build.add_argument('--bbox',type=float,nargs=4);build.add_argument('--zooms',default='13,14,15');build.add_argument('--name',default='Offline area');build.add_argument('--osm',type=Path,default=Path('artifacts/maps/osm.json'));build.add_argument('--reuse',action='store_true');build.add_argument('--endpoint',default='https://overpass-api.de/api/interpreter');build.add_argument('--output',type=Path,default=Path('artifacts/maps/area.mmmap'))
 up=sub.add_parser('upload');up.add_argument('package',type=Path);up.add_argument('--port',default=M9_PORT)
 check=sub.add_parser('check');check.add_argument('package',type=Path)
 a=p.parse_args()
 if a.action=='upload':upload(a.package,a.port);return
 if a.action=='check':m,r=pack_read(a.package);print(f'PASS {len(r)} tiles, checksums and complete RGB565 expansion: {m["name"]}');return
 if not -85<a.latitude<85 or not -180<a.longitude<180 or not 0<a.radius<=30:raise ValueError('Invalid center or radius (maximum 30 km)')
 dy=a.radius/111.32;dx=dy/math.cos(math.radians(a.latitude));bounds=a.bbox or [a.latitude-dy,a.longitude-dx,a.latitude+dy,a.longitude+dx];s,w,n,e=bounds
 if not -85<s<n<85 or not -180<w<e<180 or (n-s)*(e-w)*12392*math.cos(math.radians(a.latitude))>4000:raise ValueError('Invalid or oversized area (max 4000 km²)')
 zooms=sorted(set(int(v) for v in a.zooms.split(',')))
 if not zooms or min(zooms)<10 or max(zooms)>17:raise ValueError('Zooms 10..17')
 estimate=sum((int(pixel(n,e,z)[0]//256)-int(pixel(s,w,z)[0]//256)+1)*(int(pixel(s,w,z)[1]//256)-int(pixel(n,e,z)[1]//256)+1) for z in zooms)
 if estimate>1500:raise ValueError(f'{estimate} tiles exceeds limit 1500; reduce detail or area')
 print(f'Preparing {estimate} tiles; vector download is cached at {a.osm}',flush=True)
 osm=json.loads(a.osm.read_text()) if a.reuse else download(bounds,a.osm,a.endpoint,max(zooms)>=15)
 print(f'OSM objects: {len(osm["elements"])}',flush=True);records=list(render(osm,bounds,zooms));manifest={'version':1,'name':a.name,'latitude':a.latitude,'longitude':a.longitude,'bounds':bounds,'zooms':zooms,'default_zoom':max(zooms),'tiles':len(records),'attribution':ATTRIBUTION,'created':int(time.time())}
 size=pack_write(a.output,manifest,records);print(f'Saved {a.output}: {size:,} bytes, {len(records)} tiles',flush=True)
if __name__=='__main__':main()
