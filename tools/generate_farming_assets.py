"""Append original 16px farming art. Historical atlas rectangles stay identical.

This extends the repository's established deterministic pixel-art authoring
pipeline; Pillow and ffmpeg are build-time tools, never game dependencies.
"""
from pathlib import Path
from PIL import Image, ImageDraw
import re, struct, random, wave, math, subprocess, tempfile
root=Path(__file__).resolve().parents[1];assets=root/'assets'
def names(file,enum):
 s=(root/file).read_text().split('enum class '+enum)[1].split('};')[0]
 return [x.strip().split('=')[0].strip() for x in re.sub(r'//[^\n]*','',s).split('{',1)[1].split(',') if x.strip() and x.strip()!='Count']
items=names('include/Survival.h','Item');blocks=names('include/Block.h','Block')
old=Image.open(assets/'inventory_atlas_16.png').convert('RGBA')
atlas=Image.new('RGBA',(160,((len(items)+9)//10)*16));atlas.paste(old,(0,0))
def image():
 im=Image.new('RGBA',(16,16));return im,ImageDraw.Draw(im)
def outline(color):return tuple(int(v*.40) for v in color)
def produce(name):
 im,d=image()
 if name=='Carrot':
  d.polygon([(4,5),(9,4),(12,7),(4,14),(2,14)],fill=(224,113,28),outline=(100,58,22));d.line([(5,7),(7,8)],fill=(255,172,51));d.line([(7,10),(8,11)],fill=(166,71,22));d.line([(9,4),(8,1),(11,3),(13,1),(12,5)],fill=(82,147,43),width=2)
 elif name in ('Potato','BakedPotato'):
  c=(184,132,64) if name=='Potato' else (165,91,40)
  d.polygon([(4,3),(10,2),(13,5),(13,11),(9,14),(3,12),(1,8)],fill=c,outline=outline(c));d.line([(4,5),(9,4),(11,6)],fill=(225,182,104),width=2)
  for p in [(4,9),(10,10),(7,6)]:d.point(p,fill=(96,62,31))
  if name=='BakedPotato':d.polygon([(5,5),(10,6),(11,9),(5,10),(3,8)],fill=(245,211,136));d.line([(6,7),(9,8)],fill=(255,237,181))
 elif name=='Beetroot':
  d.polygon([(4,5),(10,4),(13,7),(10,12),(7,14),(6,12),(2,9)],fill=(152,43,65),outline=(67,25,37));d.line([(4,7),(7,6),(10,7)],fill=(206,79,98),width=2);d.line([(6,4),(4,1),(8,3),(10,1),(10,4)],fill=(66,137,61),width=2)
 elif name.endswith('Seeds'):
  c=(180,150,77) if name=='PumpkinSeeds' else (96,58,36) if name=='MelonSeeds' else (121,68,65)
  for x,y in [(3,5),(8,3),(10,9),(4,11)]:d.polygon([(x,y),(x+2,y-1),(x+3,y+1),(x+1,y+3),(x-1,y+1)],fill=c,outline=outline(c));d.point((x+1,y),fill=(231,201,136))
 elif name in ('Pumpkin','Melon','Composter'):
  c=(202,112,29) if name=='Pumpkin' else (91,151,48) if name=='Melon' else (135,88,45)
  d.polygon([(2,5),(8,2),(14,5),(14,12),(8,15),(2,12)],fill=c,outline=outline(c));d.polygon([(2,5),(8,2),(14,5),(8,8)],fill=tuple(min(255,v+30) for v in c));d.line([(8,8),(8,14)],fill=outline(c))
  for x in [4,6,10,12]:d.line([(x,7),(x,12)],fill=tuple(int(v*.78) for v in c))
  if name=='Composter':d.polygon([(4,5),(8,3),(12,5),(8,7)],fill=(57,48,30));d.line([(3,10),(8,13),(13,10)],fill=(190,138,76))
  elif name=='Pumpkin':d.rectangle((7,0,9,3),fill=(91,111,43))
 elif name=='MelonSlice':
  d.polygon([(1,5),(13,3),(13,7),(10,12),(5,13),(1,9)],fill=(70,129,43),outline=(29,64,30));d.polygon([(2,5),(12,4),(11,8),(8,11),(4,10)],fill=(225,83,76));d.line([(2,9),(5,12),(9,11),(12,7)],fill=(161,193,82));d.point((5,7),fill=(66,43,35));d.point((9,6),fill=(66,43,35))
 elif name=='SweetBerries':
  d.line([(7,5),(9,1),(11,2)],fill=(72,132,43),width=2)
  for x,y in [(3,4),(8,5),(5,9)]:d.rectangle((x,y,x+4,y+4),fill=(120,33,55));d.rectangle((x+1,y,x+3,y+2),fill=(209,67,84));d.point((x+1,y),fill=(255,130,126))
 elif name.endswith('Hoe'):
  c={'Wood':(163,105,52),'Stone':(137,144,146),'Iron':(195,203,201),'Gold':(239,190,53),'Diamond':(67,211,199)}[name[:-3]]
  d.line([(3,14),(10,5)],fill=(66,43,27),width=3);d.line([(3,13),(10,4)],fill=(165,109,57));d.polygon([(4,2),(12,2),(13,4),(12,8),(10,8),(10,5),(4,5)],fill=c,outline=outline(c));d.line([(5,3),(11,3)],fill=tuple(min(255,v+35) for v in c))
 elif name=='Shears':
  d.line([(4,11),(12,2),(11,8)],fill=(192,205,209),width=2);d.line([(11,11),(3,2),(4,8)],fill=(152,170,180),width=2);d.ellipse((1,10,6,15),outline=(87,62,42),width=2);d.ellipse((9,10,14,15),outline=(87,62,42),width=2);d.point((7,8),fill=(248,227,164))
 elif name in ('Bucket','MilkBucket'):
  d.arc((2,0,13,13),180,360,fill=(181,194,195),width=2);d.polygon([(2,5),(13,5),(12,14),(4,14)],fill=(136,155,162),outline=(45,61,67));d.line([(4,6),(5,12)],fill=(210,225,225),width=2);d.line([(10,6),(10,12)],fill=(91,112,122))
  if name=='MilkBucket':d.rectangle((3,5,12,7),fill=(239,238,219));d.point((4,5),fill=(255,255,243))
 elif name in ('BoneMeal','Sugar'):
  c=(223,216,180) if name=='BoneMeal' else (241,239,224)
  for x,y in [(3,4),(9,3),(5,8),(10,10),(2,11)]:d.polygon([(x,y),(x+2,y-1),(x+3,y+2),(x,y+3)],fill=c,outline=(141,141,124))
 elif name=='Egg':
  d.polygon([(7,1),(10,3),(13,9),(12,13),(9,15),(5,14),(3,11),(3,8),(5,3)],fill=(225,216,185),outline=(116,101,78));d.line([(6,4),(5,7),(5,10)],fill=(255,246,214),width=2)
 elif name in ('RawChicken','CookedChicken'):
  c=(228,155,141) if name=='RawChicken' else (162,87,38)
  d.polygon([(4,4),(9,2),(13,5),(13,10),(10,12),(6,10),(4,13),(2,12),(4,8)],fill=c,outline=outline(c));d.line([(6,5),(10,4),(11,7)],fill=tuple(min(255,v+40) for v in c),width=2);d.line([(4,11),(2,13),(3,14)],fill=(234,222,186),width=2)
 elif name=='Feather':
  d.polygon([(3,12),(4,7),(10,1),(13,1),(13,6),(8,11)],fill=(223,229,213),outline=(102,121,118));d.line([(2,14),(11,4)],fill=(152,166,157));d.line([(6,8),(6,5)],fill=(184,198,187));d.line([(8,8),(11,8)],fill=(184,198,187))
 elif name in ('Bowl','BeetrootSoup','MushroomStew','VegetableStew'):
  d.polygon([(1,7),(14,7),(13,11),(10,14),(5,14),(2,11)],fill=(127,78,40),outline=(57,38,28));d.line([(4,11),(7,12),(11,11)],fill=(178,116,61));d.ellipse((1,4,14,9),fill=(68,44,28),outline=(177,119,62))
  if name!='Bowl':
   c=(160,47,66) if name=='BeetrootSoup' else (143,112,65) if name=='MushroomStew' else (158,140,48);d.ellipse((3,5,12,8),fill=c)
   for x,y in [(4,6),(8,5),(10,7)]:d.point((x,y),fill=(226,177,94) if name=='MushroomStew' else (240,156,52));d.point((x+1,y),fill=(81,139,57))
 elif name=='PumpkinPie':
  d.polygon([(2,6),(8,2),(14,6),(13,12),(3,12)],fill=(173,99,36),outline=(91,55,28));d.polygon([(2,6),(8,2),(14,6),(9,10)],fill=(234,159,54));d.line([(2,6),(8,10),(14,6)],fill=(250,199,106),width=2);d.line([(4,8),(5,10)],fill=(102,58,25))
 elif name=='Cookie':
  d.polygon([(5,2),(11,3),(14,7),(12,12),(8,14),(3,12),(1,7)],fill=(192,135,61),outline=(100,65,30));d.line([(4,4),(8,3),(11,5)],fill=(239,187,94));
  for x,y in [(5,6),(10,7),(7,11),(3,10)]:d.rectangle((x,y,x+1,y+1),fill=(115,44,48))
 else:raise ValueError(name)
 return im
for name in items[174:]:
 im=produce(name);i=items.index(name);atlas.paste(im,(i%10*16,i//10*16))
 im.save(assets/'inventory'/(re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png'))
for i in range(174):
 box=(i%10*16,i//10*16,i%10*16+16,i//10*16+16);assert atlas.crop(box).tobytes()==old.crop(box).tobytes()
atlas.save(assets/'inventory_atlas_16.png');expanded=atlas.resize((atlas.width*4,atlas.height*4),Image.Resampling.NEAREST)
expanded.save(assets/'item_icons_expansion.png');(assets/'item_icons_expansion.rgba').write_bytes(struct.pack('<ii',*expanded.size)+expanded.tobytes())
path=assets/'crafting_blocks.rgba';raw=path.read_bytes();w,h=struct.unpack('<ii',raw[:8]);oldBlocks=Image.frombytes('RGBA',(w,h),raw[8:]);world=Image.new('RGBA',((143-45)*16,16));world.paste(oldBlocks,(0,0))
for i,name in enumerate(blocks[167:]):
 im,d=image();rng=random.Random(name)
 if 'Stem' in name or name.startswith(('Carrot','Potato','Beetroot','BerryBush')):
  stage=int(name[-1]);c=(87,141,48) if stage<3 else (113,154,46);height=5+stage*3
  if 'BerryBush' in name:
   for x,y in [(3,9),(7,6),(10,9),(5,11)]:d.rectangle((x,max(1,y-stage),x+3,y+3),fill=(48+stage*5,108+stage*9,43))
   if stage>=2:
    for x,y in [(4,8),(10,9),(7,11)]:d.rectangle((x,y,x+1,y+1),fill=(204,55,73))
  else:
   for x in [4,8,12]:
    d.line([(x,15),(x,16-height)],fill=c,width=1);d.line([(x,13),(x-3,10),(x-2,8)],fill=(62,111,42),width=2);d.line([(x,12),(x+2,9)],fill=(113,170,57),width=2)
   if stage==3 and name.startswith(('Carrot','Beetroot','Potato')):
    c=(221,111,29) if name.startswith('Carrot') else (167,48,69) if name.startswith('Beet') else (187,151,75)
    for x in [3,7,11]:d.rectangle((x,12,x+2,14),fill=c)
 else:
  c=(195,110,29) if name=='Pumpkin' else (93,148,46) if name=='Melon' else (125,82,43)
  for y in range(16):
   for x in range(16):
    n=rng.randint(-6,6)+(-20 if x%4==0 else 0);im.putpixel((x,y),tuple(max(0,min(255,v+n)) for v in c)+(255,))
  if name.startswith('Composter'):
   stage=int(name[-1]);d.rectangle((0,0,15,2),fill=(173,119,64));d.rectangle((2,3,13,13),fill=(50,41,27));d.rectangle((0,12,15,14),fill=(156,105,55))
   if stage:
    d.rectangle((3,12-stage*2,12,12),fill=(79,91,43));
    for x in range(4,12,3):d.point((x,13-stage*2),fill=(154,132,54))
   if stage==4:d.rectangle((5,6,10,9),fill=(218,210,173))
 world.paste(im,((106+i-45)*16,0));(assets/'farming').mkdir(exist_ok=True);im.save(assets/'farming'/(name+'.png'))
for stage in range(4):
 im,d=image()
 for x in [3,7,11]:
  d.line([(x,15),(x,4)],fill=(106,147,48) if stage<3 else (177,153,59))
  d.line([(x,13),(x-2,10)],fill=(79,130,39));d.line([(x,12),(x+2,9)],fill=(104,156,48))
  if stage>=2:
   for y in [4,6,8]:d.line([(x-1,y),(x+1,y+1)],fill=(186,172,71) if stage==2 else (230,197,95))
 world.paste(im,((137+stage-45)*16,0));im.save(assets/'farming'/('Wheat'+str(stage)+'.png'))
for wet in range(2):
 im,d=image();rng=random.Random('tilled')
 for y in range(16):
  for x in range(16):
   c=(83,55,34) if wet else (135,94,57);n=rng.randint(-7,7)+(-19 if y%4==0 else 6 if y%4==1 else 0)
   im.putpixel((x,y),tuple(max(0,min(255,v+n)) for v in c)+(255,))
 world.paste(im,((141+wet-45)*16,0));im.save(assets/'farming'/('WetSoil.png' if wet else 'DrySoil.png'))
assert world.crop((0,0,(106-45)*16,16)).tobytes()==oldBlocks.crop((0,0,(106-45)*16,16)).tobytes()
path.write_bytes(struct.pack('<ii',*world.size)+world.tobytes());world.save(assets/'crafting_blocks.png');world.save(assets/'farming/block_atlas.png')
# Short original synthesized clucks; not sampled from another game's assets.
out=root/'sounds/chicken';out.mkdir(exist_ok=True)
for name,base,duration in [('say1',670,.27),('hurt1',890,.22),('death1',520,.40),('step1',180,.055)]:
 rate=22050;data=[];rng=random.Random(name)
 for k in range(int(rate*duration)):
  t=k/rate;p=t/duration;envelope=math.sin(math.pi*p)**2
  pitch=base*(1-.35*p)+90*math.sin(t*70)
  pulse=.45+.55*math.sin(t*math.pi*19)**2
  sample=(math.sin(2*math.pi*pitch*t)+.3*math.sin(2*math.pi*pitch*2.1*t)+rng.uniform(-.1,.1))*envelope*pulse*.19
  data.append(max(-32768,min(32767,int(sample*32767))))
 with tempfile.TemporaryDirectory() as tmp:
  wav=Path(tmp)/'original.wav'
  with wave.open(str(wav),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(struct.pack('<'+'h'*len(data),*data))
  subprocess.run(['ffmpeg','-v','error','-y','-i',str(wav),'-c:a','libvorbis','-q:a','4',str(out/(name+'.ogg'))],check=True)
print('32 original icons, 37 original materials, 4 original clucks; historical atlas cells unchanged')
