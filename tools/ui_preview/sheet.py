#!/usr/bin/env python3
"""Tile rendered PPM frames into one PNG contact sheet: sheet.py DIR OUT.png [COLS] [SCALE]"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw
src=Path(sys.argv[1]);out=sys.argv[2];cols=int(sys.argv[3]) if len(sys.argv)>3 else 3;scale=int(sys.argv[4]) if len(sys.argv)>4 else 2
names=sys.argv[5].split(',') if len(sys.argv)>5 else None
files=[src/f'{n}.ppm' for n in names] if names else sorted(src.glob('*.ppm'),key=lambda p:p.stat().st_mtime)
ims=[Image.open(f).convert('RGB') for f in files]
w,h=ims[0].size;rows=(len(ims)+cols-1)//cols;pad=18
sheet=Image.new('RGB',(cols*(w*scale+8),rows*(h*scale+pad+8)),(40,40,40));d=ImageDraw.Draw(sheet)
for i,(f,im) in enumerate(zip(files,ims)):
    x=(i%cols)*(w*scale+8);y=(i//cols)*(h*scale+pad+8)
    d.text((x+4,y+3),f.stem,fill=(230,230,230));sheet.paste(im.resize((w*scale,h*scale),Image.NEAREST),(x,y+pad))
sheet.save(out)
