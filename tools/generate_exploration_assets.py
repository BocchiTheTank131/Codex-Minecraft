"""Original 16px exploration art. Append cells; preserve all historical pixels."""
from pathlib import Path
import re, struct, random
from PIL import Image, ImageDraw

root=Path(__file__).resolve().parents[1]
a=root/'assets'
source=(a/'crafting_blocks.rgba').read_bytes()
old=Image.frombytes('RGBA',struct.unpack('<ii',source[:8]),source[8:])
world=Image.new('RGBA',((106-45)*16,16));world.paste(old,(0,0))
web=Image.new('RGBA',(16,16));d=ImageDraw.Draw(web)
for end in [(0,0),(15,0),(0,15),(15,15),(7,0),(0,7),(15,7),(7,15)]:
    d.line((7,7,*end),fill=(220,223,225,235))
for r in [3,6]:
    d.line([(7-r,7),(7-r//2,7-r//2),(7,7-r),(7+r//2,7-r//2),
            (7+r,7),(7+r//2,7+r//2),(7,7+r),(7-r//2,7+r//2),(7-r,7)],fill=(180,191,197,215))
stone=Image.new('RGBA',(16,16));d=ImageDraw.Draw(stone);rng=random.Random(2120)
for y in range(16):
    for x in range(16):
        n=rng.randrange(-8,9);joint=y%5==0 or (x+(8 if (y//5)%2 else 0))%16==0
        v=65 if joint else 121+n;d.point((x,y),fill=(v,v+2,v+3,255))
for path in [[(4,1),(5,3),(4,5),(7,7),(6,10)],[(13,6),(11,8),(12,11),(10,14)]]:
    d.line(path,fill=(49,53,55,255));d.point(path[0],fill=(178,180,174,255))
icons={'Cobweb':web,'CrackedStoneBricks':stone}
token=Image.new('RGBA',(16,16));d=ImageDraw.Draw(token)
d.polygon([(5,2),(10,2),(13,5),(13,10),(10,13),(5,13),(2,10),(2,5)],fill=(114,69,34),outline=(45,34,28))
d.polygon([(5,3),(10,3),(12,5),(12,10),(10,12),(5,12),(3,10),(3,5)],fill=(198,142,65))
d.line([(7,4),(7,10),(4,7),(10,7)],fill=(78,103,97));d.point((5,4),fill=(255,221,125))
icons['SurveyorToken']=token
shard=Image.new('RGBA',(16,16));d=ImageDraw.Draw(shard)
d.polygon([(8,1),(12,5),(10,13),(6,15),(3,8)],fill=(41,79,102),outline=(25,37,60))
d.polygon([(8,2),(10,5),(7,12),(4,8)],fill=(112,192,192));d.line([(8,3),(7,7),(5,9)],fill=(209,239,229))
icons['CavernShard']=shard
seal=Image.new('RGBA',(16,16));d=ImageDraw.Draw(seal)
d.polygon([(4,2),(12,2),(14,5),(12,13),(3,13),(1,9)],fill=(104,118,99),outline=(43,56,50))
d.line([(4,3),(11,3),(12,5)],fill=(183,196,157));d.rectangle((5,5,10,10),outline=(52,72,68));d.line([(6,5),(9,8),(6,10)],fill=(216,179,80))
icons['AncientSeal']=seal
for i,im in enumerate([web,stone]):world.paste(im,((104+i-45)*16,0))
assert world.crop((0,0,min(old.width,59*16),16)).tobytes()==old.crop((0,0,min(old.width,59*16),16)).tobytes()
world.save(a/'crafting_blocks.png');(a/'crafting_blocks.rgba').write_bytes(struct.pack('<ii',*world.size)+world.tobytes())
old=Image.open(a/'inventory_atlas_16.png').convert('RGBA')
s=(root/'include/Survival.h').read_text().split('enum class Item')[1].split('};')[0];s=re.sub(r'//[^\n]*','',s)
names=[v.strip().split('=')[0].strip() for v in s.split('{',1)[1].split(',') if v.strip()];names.remove('Count')
native=Image.new('RGBA',(160,((len(names)+9)//10)*16));native.paste(old,(0,0))
for name,im in icons.items():
    i=names.index(name);native.paste(im,((i%10)*16,(i//10)*16))
    im.save(a/'inventory'/(re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png'))
for i in range(names.index('Cobweb')):
    box=((i%10)*16,(i//10)*16,(i%10+1)*16,(i//10+1)*16)
    assert old.crop(box).tobytes()==native.crop(box).tobytes(),i
native.save(a/'inventory_atlas_16.png');expanded=native.resize((native.width*4,native.height*4),Image.Resampling.NEAREST)
expanded.save(a/'item_icons_expansion.png');(a/'item_icons_expansion.rgba').write_bytes(struct.pack('<ii',*expanded.size)+expanded.tobytes())
print('Five original 16x16 icons, two block tiles appended; historical pixels unchanged.')
