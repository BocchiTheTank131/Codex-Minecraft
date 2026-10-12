"""Label native one-block river survey windows exported by the V18 audit."""
import argparse,csv
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);args=p.parse_args();root=args.directory
dtype=np.dtype([('biome','u1'),('water_y','u1'),('height','<i2'),('temperature','<f4'),('humidity','<f4'),('continent','<f4'),('river','<f4'),('macro_t','<f4'),('macro_h','<f4')])
font=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',17)
for site in root.glob('river-window-*.csv'):
 row=next(csv.DictReader(site.open()));seed=row['seed']
 panel=Image.new('RGB',(1080,600),(24,28,34));draw=ImageDraw.Draw(panel)
 draw.text((18,12),f"Seed {seed} | 512 x 512 blocks | one sample/block | centre {row['x']}, {row['z']}",font=font,fill='white')
 for j,v in enumerate((17,18)):
  a=np.fromfile(root/f'river-window-{seed}-v{v}.bin',dtype=dtype).reshape(512,512)
  h=a['height'].astype(float);shade=np.clip(.75+(h-np.roll(h,1,axis=1))*.06+(h-np.roll(h,1,axis=0))*.04,.40,1.15)
  colors=np.empty((512,512,3));colors[:]=(113,167,75)
  colors[np.isin(a['biome'],(2,10,16,29))]=(213,195,136)
  colors[np.isin(a['biome'],(3,7,20,23))]=(128,132,130)
  colors[np.isin(a['biome'],(4,6,8,12,13))]=(209,228,225)
  colors*=shade[...,None]
  water=h<a['water_y'];colors[water]=(57,119,172)
  colors[water&np.isin(a['biome'],(4,6,8,12,13,14,15,16))]=(179,217,237)
  panel.paste(Image.fromarray(np.clip(colors,0,255).astype('u1')),(18+j*530,60))
  draw.text((18+j*530,36),f'V{v} SURFACE / NATIVE WATER LEVEL',font=font,fill='white')
 draw.text((18,578),'Surface-column preview; caves, vegetation and structures are checked in rendered captures.',font=font,fill='white')
 panel.save(root/f'river-window-{seed}-comparison.png')
