"""Append original 16px equipment art without changing existing atlas pixels."""
from pathlib import Path
import re, struct
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parents[1]
assets=root/'assets'
enum=(root/'include/Survival.h').read_text().split('enum class Item')[1].split('};')[0]
enum=re.sub(r'//[^\n]*','',enum)
names=[n.strip().split('=')[0].strip() for n in enum.split('{',1)[1].split(',') if n.strip()]
names.remove('Count')
old=Image.open(assets/'inventory_atlas_16.png').convert('RGBA')
atlas=Image.new('RGBA',(160,((len(names)+9)//10)*16));atlas.paste(old,(0,0))
materials={'Leather':(155,91,49),'Gold':(237,180,42),'Iron':(177,188,196),'Diamond':(60,204,202)}
shapes={
 'Helmet':[(3,4),(5,2),(11,2),(13,4),(13,11),(10,11),(10,7),(6,7),(6,11),(3,11)],
 'Chestplate':[(1,4),(5,2),(6,5),(10,5),(11,2),(15,4),(13,8),(12,7),(12,14),(4,14),(4,7),(3,8)],
 'Leggings':[(4,2),(12,2),(12,14),(9,14),(9,8),(7,8),(7,14),(4,14)],
 'Boots':[(3,4),(6,4),(6,10),(7,11),(7,14),(1,14),(1,11),(3,10)]}
for material,color in materials.items():
 for piece,shape in shapes.items():
  im=Image.new('RGBA',(16,16));draw=ImageDraw.Draw(im)
  dark=tuple(int(c*.36) for c in color);light=tuple(min(255,int(c*1.18)+12) for c in color)
  draw.polygon(shape,fill=color,outline=dark)
  if piece=='Boots':draw.polygon([(x+8,y) for x,y in shape],fill=color,outline=dark)
  mask=im.copy()
  for y in range(16):
   for x in range(16):
    if mask.getpixel((x,y))[:3]==color:
     if x>8:im.putpixel((x,y),tuple(int(c*.8) for c in color)+(255,))
     elif y%4==0:im.putpixel((x,y),light+(255,))
  if material=='Leather':
   draw.line([(4,5),(4,9)],fill=(215,162,95));draw.point((11,6),fill=(245,193,102))
  name=material+piece;index=names.index(name)
  atlas.paste(im,((index%10)*16,(index//10)*16))
  im.save(assets/'inventory'/(re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png'))
im=Image.new('RGBA',(16,16));d=ImageDraw.Draw(im)
d.polygon([(2,1),(13,1),(13,9),(11,12),(7,15),(4,12),(2,9)],fill=(177,184,184),outline=(40,47,51))
d.polygon([(4,3),(11,3),(11,9),(7,12),(4,9)],fill=(132,81,42),outline=(74,46,32))
d.line([(7,3),(7,11)],fill=(188,124,66));d.line([(4,6),(11,6)],fill=(208,214,205))
d.point((3,2),fill=(236,239,225));d.point((12,2),fill=(236,239,225))
i=names.index('Shield');atlas.paste(im,((i%10)*16,(i//10)*16));im.save(assets/'inventory/shield.png')
for i in range(names.index('LeatherHelmet')):
 box=((i%10)*16,(i//10)*16,(i%10+1)*16,(i//10+1)*16)
 assert atlas.crop(box).tobytes()==old.crop(box).tobytes(), 'Historical atlas changed'
atlas.save(assets/'inventory_atlas_16.png')
expanded=atlas.resize((atlas.width*4,atlas.height*4),Image.Resampling.NEAREST)
expanded.save(assets/'item_icons_expansion.png')
(assets/'item_icons_expansion.rgba').write_bytes(struct.pack('<ii',*expanded.size)+expanded.tobytes())
print('17 equipment icons appended; all historical pixels identical')
