"""Append original transportation/navigation pixel art; preserve historical atlas cells."""
from pathlib import Path
from PIL import Image, ImageDraw
import re,struct
root=Path(__file__).resolve().parents[1];assets=root/'assets'
e=(root/'include/Survival.h').read_text().split('enum class Item')[1].split('};')[0];e=re.sub(r'//[^\n]*','',e)
names=[n.strip().split('=')[0].strip() for n in e.split('{',1)[1].split(',') if n.strip()];names.remove('Count')
old=Image.open(assets/'inventory_atlas_16.png').convert('RGBA');atlas=Image.new('RGBA',(160,((len(names)+9)//10)*16));atlas.paste(old,(0,0))
colors=[(155,103,52),(205,191,129),(91,61,34),(172,111,70),(194,91,38),(66,43,28)]
for name in names[names.index('OakBoat'):]:
 im=Image.new('RGBA',(16,16));d=ImageDraw.Draw(im)
 if name.endswith('Boat'):
  c=colors[names.index(name)-names.index('OakBoat')];dark=tuple(int(v*.48) for v in c);light=tuple(min(255,int(v*1.3)) for v in c)
  d.polygon([(0,7),(4,4),(12,4),(15,7),(12,12),(4,12)],fill=c,outline=dark)
  d.polygon([(3,7),(5,5),(11,5),(13,7),(11,9),(5,9)],fill=dark)
  d.line([(1,8),(5,11),(11,11),(14,8)],fill=light);d.line([(7,5),(8,9)],fill=c)
  d.line([(1,2),(13,13)],fill=(200,155,85),width=1);d.rectangle((12,12,14,14),fill=c)
 elif name in ('EmptyMap','FilledMap'):
  d.polygon([(2,2),(7,1),(12,2),(14,1),(14,13),(10,14),(6,13),(1,14)],fill=(216,191,135),outline=(91,67,41))
  d.line([(5,3),(5,12)],fill=(174,147,95));d.line([(10,3),(10,12)],fill=(236,218,172))
  if name=='FilledMap':
   d.polygon([(3,4),(7,3),(11,5),(9,10),(3,11)],fill=(82,135,66));d.line([(12,3),(8,6),(8,9),(5,12)],fill=(58,116,184),width=2);d.line([(9,6),(11,8)],fill=(240,242,218))
 elif name=='Compass':
  d.ellipse((1,1,14,14),fill=(186,170,116),outline=(46,45,45));d.ellipse((3,3,12,12),fill=(48,57,68));d.polygon([(8,3),(6,9),(8,8),(10,8)],fill=(226,60,42));d.polygon([(8,12),(6,9),(8,8),(10,8)],fill=(212,224,220));d.point((4,4),fill=(244,237,198))
 i=names.index(name);atlas.paste(im,((i%10)*16,(i//10)*16));im.save(assets/'inventory'/(re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png'))
for i in range(names.index('OakBoat')):
 box=(i%10*16,i//10*16,i%10*16+16,i//10*16+16);assert atlas.crop(box).tobytes()==old.crop(box).tobytes()
atlas.save(assets/'inventory_atlas_16.png');expanded=atlas.resize((atlas.width*4,atlas.height*4),Image.Resampling.NEAREST);expanded.save(assets/'item_icons_expansion.png');(assets/'item_icons_expansion.rgba').write_bytes(struct.pack('<ii',*expanded.size)+expanded.tobytes())
print('9 original icons appended; historical item pixels unchanged')
