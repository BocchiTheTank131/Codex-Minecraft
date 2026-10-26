"""Append original 16px automation textures; preserve every historical atlas texel."""
from pathlib import Path
import struct,random
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parents[1];assets=root/'assets'
raw=(assets/'crafting_blocks.rgba').read_bytes();w,h=struct.unpack('<ii',raw[:8]);base=Image.frombytes('RGBA',(w,h),raw[8:]).crop((0,0,(187-45)*16,16))
world=Image.new('RGBA',((217-45)*16,16));world.paste(base)
names=['Wire','Lever','Repeater','AND','OR','NOT','Lamp','Hopper','Piston','Sticky','Daylight','Observer','CropSensor','Harvester','Head']
for i,name in enumerate(names):
 for on in range(2):
  rng=random.Random('VoxelFrontier/Automation/'+name);tile=Image.new('RGBA',(16,16));d=ImageDraw.Draw(tile)
  color=(105,110,119) if i not in (0,1,6,8,9,10,14) else [(155,83,45),(110,101,88),(0,0,0),(0,0,0),(0,0,0),(0,0,0),(114,85,41),(0,0,0),(134,100,61),(121,110,63),(93,116,133),(0,0,0),(0,0,0),(0,0,0),(144,110,64)][i]
  for y in range(16):
   for x in range(16):
    delta=rng.randrange(-13,14);tile.putpixel((x,y),tuple(max(0,min(255,c+delta)) for c in color)+(255,))
  copper=(255,182,63) if on else (140,66,35);dark=(40,45,51)
  d.rectangle((0,0,15,15),outline=dark)
  if i==0:
   for y in range(1,16,4):d.line((0,y,15,y),fill=copper)
  elif i==1:d.rectangle((5,2,10,13),fill=(104,66,37),outline=(192,154,96));d.rectangle((6,3 if on else 9,9,6 if on else 12),fill=copper)
  elif i in (2,3,4,5,12):
   d.line((3,13,3,7,8,7,8,2),fill=copper,width=2);d.line((12,13,12,7,8,7),fill=copper,width=2);d.polygon([(5,5),(8,1),(11,5)],fill=copper)
   if i==3:d.rectangle((5,8,10,12),outline=(237,225,166))
   if i==4:d.line((5,12,8,8,11,12),fill=(237,225,166),width=2)
   if i==5:d.ellipse((6,8,10,12),outline=(237,225,166))
   if i==12:d.line((7,12,7,5),fill=(57,165,63),width=2);d.rectangle((3,5,11,7),fill=(202,170,51))
  elif i==6:
   d.rectangle((2,2,13,13),fill=(255,220,113) if on else (100,83,52),outline=(181,139,59));d.line((3,3,12,12),fill=(255,248,200) if on else (133,113,73));d.line((3,12,12,3),fill=copper)
  elif i==7:d.rectangle((2,2,13,13),fill=(30,35,41),outline=(151,158,167));d.rectangle((4,4,11,11),outline=(68,74,83))
  elif i in (8,9,14):
   for y in (3,7,11):d.line((2,y,13,y),fill=(89,59,34));d.rectangle((0,5,2,10),fill=(165,173,179));d.rectangle((13,5,15,10),fill=(165,173,179))
   if i==9:d.rectangle((4,3,11,12),fill=(96,136,48),outline=(157,179,70))
  elif i==10:
   for y in range(2,14,4):
    for x in range(2,14,4):d.rectangle((x,y,x+2,y+2),fill=(100,163,207) if on else (64,85,113))
  elif i==11:d.rectangle((3,4,6,7),fill=(25,30,35));d.rectangle((10,4,13,7),fill=(25,30,35));d.rectangle((5,10,11,12),fill=copper)
  elif i==13:
   d.rectangle((2,2,13,13),outline=(177,187,192));d.line((3,12,12,3),fill=(208,215,219),width=2);d.line((3,3,12,12),fill=copper,width=2)
  world.paste(tile,((187-45+i*2+on)*16,0))
(assets/'crafting_blocks.rgba').write_bytes(struct.pack('<ii',*world.size)+world.tobytes());world.save(assets/'crafting_blocks.png')
print('30 original automation tiles appended; 187 historical tiles unchanged')
