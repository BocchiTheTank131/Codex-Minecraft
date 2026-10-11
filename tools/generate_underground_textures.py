"""Append original 16px underground tiles/icons without touching existing cells."""
from pathlib import Path
import struct,re,random
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1]; A=ROOT/'assets'
names=['Deepslate','DeepslateCoalOre','DeepslateIronOre','DeepslateCopperOre','DeepslateGoldOre','DeepslateDiamondOre','Bedrock','Lava']
blob=(A/'crafting_blocks.rgba').read_bytes(); old=Image.frombytes('RGBA',struct.unpack('<ii',blob[:8]),blob[8:])
world=Image.new('RGBA',((104-45)*16,16));world.paste(old,(0,0));tiles={}
palettes=[None,(28,30,35),(182,151,127),(195,112,69),(220,175,53),(59,191,189),None,None]
for n,name in enumerate(names):
 r=random.Random(7919+n*311);im=Image.new('RGBA',(16,16));d=ImageDraw.Draw(im)
 for y in range(16):
  for x in range(16):
   noise=r.randrange(-9,10); base=(67,70,79) if n<6 else (41,43,47) if n==6 else (203,69,14)
   if n<6:noise+=-12 if y%4==0 else 0
   if n==6:noise+=r.randrange(-20,20)
   if n==7:noise+=22 if (x//3+y//2)%3==0 else -10
   im.putpixel((x,y),tuple(max(0,min(255,v+noise)) for v in base)+(255,))
 if 0<n<6:
  for x,y in [(3,3),(9,4),(5,10),(12,12)]:
   c=palettes[n];d.rectangle((x,y,x+2,y+1),fill=c+(255,));d.point((x,y),fill=tuple(min(255,v+25) for v in c)+(255,))
 tiles[name]=im;world.paste(im,((96+n-45)*16,0))
world.save(A/'crafting_blocks.png');(A/'crafting_blocks.rgba').write_bytes(struct.pack('<ii',*world.size)+world.tobytes())
old=Image.open(A/'inventory_atlas_16.png').convert('RGBA')
s=(ROOT/'include/Survival.h').read_text().split('enum class Item')[1].split('};')[0];s=re.sub(r'//[^\n]*','',s)
items=[v.strip().split('=')[0].strip() for v in s.split('{',1)[1].split(',') if v.strip()];items.remove('Count')
native=Image.new('RGBA',(160,((len(items)+9)//10)*16));native.paste(old,(0,0))
for name in names:
 im=Image.new('RGBA',(16,16));tex=tiles[name]
 for origin,a,b,gain in [((8,1),(6,3),(-6,3),1),((2,4),(6,3),(0,7),.82),((8,7),(6,-3),(0,7),.64)]:
  det=a[0]*b[1]-a[1]*b[0]
  for y in range(1,15):
   for x in range(1,15):
    px,py=x+.5-origin[0],y+.5-origin[1];u=(px*b[1]-py*b[0])/det;v=(a[0]*py-a[1]*px)/det
    if 0<=u<1 and 0<=v<1:
     c=tex.getpixel((min(15,int(u*16)),min(15,int(v*16))));edge=.91 if min(u,v,1-u,1-v)<.06 else 1
     im.putpixel((x,y),tuple(round(k*gain*edge) for k in c[:3])+(255,))
 i=items.index(name);native.paste(im,((i%10)*16,(i//10)*16));im.save(A/'inventory'/ (re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png'))
for i in range(items.index('Deepslate')):
 box=((i%10)*16,(i//10)*16,(i%10+1)*16,(i//10+1)*16)
 assert native.crop(box).tobytes()==old.crop(box).tobytes(),i
native.save(A/'inventory_atlas_16.png');atlas=native.resize((native.width*4,native.height*4),Image.Resampling.NEAREST)
atlas.save(A/'item_icons_expansion.png');(A/'item_icons_expansion.rgba').write_bytes(struct.pack('<ii',*atlas.size)+atlas.tobytes())
print('8 original underground tiles/icons appended; legacy icon pixels unchanged.')
