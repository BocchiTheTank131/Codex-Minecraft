"""Original construction pixel art appended to existing atlases; old rectangles are immutable."""
from pathlib import Path
from PIL import Image,ImageDraw
import struct,re,random
root=Path(__file__).resolve().parents[1];assets=root/'assets';out=assets/'building';out.mkdir(exist_ok=True)
raw=(assets/'crafting_blocks.rgba').read_bytes();w,h=struct.unpack('<ii',raw[:8]);old=Image.frombytes('RGBA',(w,h),raw[8:]).crop((0,0,(143-45)*16,16));w,h=old.size
world=Image.new('RGBA',((187-45)*16,16));world.paste(old)
colors={'Oak':(165,118,65),'Birch':(213,193,137),'Spruce':(100,72,42),'Jungle':(170,119,83),'Acacia':(186,88,48),'DarkOak':(72,49,30)}
names=['OakStrippedLog','BirchStrippedLog','SpruceStrippedLog','JungleStrippedLog','AcaciaStrippedLog','DarkOakStrippedLog','ChiseledStoneBricks','SmoothStone','SmoothSandstone','CutSandstone','PolishedDeepslate','DeepslateBricks','CrackedDeepslateBricks','RoofShingles','StonePaving','GlassPane','RoseGlassPane','AmberGlassPane','SmokedGlassPane','Lantern','Chain','Candle0','Candle1','Candle2','Candle3','CampfireLit','CampfireUnlit','Bed0','Bed1','Bed2','Bed3','Barrel','Painting0','Painting1','Painting2','Painting3','ItemFrame','FlowerPot','Carpet0','Carpet1','Carpet2','Carpet3','DirtPath','Scaffold']
for index,name in enumerate(names):
 rng=random.Random('VoxelFrontier/'+name);im=Image.new('RGBA',(16,16));d=ImageDraw.Draw(im)
 color=next((v for k,v in colors.items() if name.startswith(k)),(128,132,131))
 if 'Sandstone' in name:color=(216,195,137)
 if 'Deepslate' in name:color=(65,70,77)
 if 'Roof' in name:color=(132,67,48)
 if 'Carpet' in name or name.startswith('Bed') or name.startswith('Candle'):color=[(175,55,58),(218,175,58),(221,215,194),(62,109,155)][int(name[-1])]
 if name=='Lantern':color=(228,174,66)
 if name=='Chain':color=(129,140,147)
 if name=='Barrel':color=(135,91,49)
 if name=='FlowerPot':color=(159,83,57)
 if name=='Scaffold':color=(165,151,80)
 if name=='DirtPath':color=(145,118,73)
 for y in range(16):
  for x in range(16):
   n=rng.randrange(-7,8);im.putpixel((x,y),tuple(max(0,min(255,c+n)) for c in color)+(255,))
 if name.endswith('StrippedLog'):
  for x in [1,5,10,14]:d.line((x,0,x,15),fill=tuple(int(c*.80) for c in color));d.line((x+1,2,x+1,12),fill=tuple(min(255,int(c*1.12)) for c in color))
 if 'Bricks' in name or 'Shingles' in name or name=='StonePaving':
  for y in range(0,16,4):d.line((0,y,15,y),fill=tuple(int(c*.55) for c in color));[d.line((x,y,x,y+4),fill=tuple(int(c*.6) for c in color)) for x in range((y//4%2)*4,16,8)]
 if name=='ChiseledStoneBricks':d.rectangle((2,2,13,13),outline=(75,78,78));d.rectangle((5,5,10,10),outline=(164,168,165));d.rectangle((7,6,8,9),fill=(72,76,77))
 if 'Cracked' in name:d.line([(2,0),(6,5),(4,8),(10,10),(8,15)],fill=(31,34,39))
 if 'Pane' in name:
  c={'GlassPane':(184,220,226),'RoseGlassPane':(214,138,161),'AmberGlassPane':(223,182,95),'SmokedGlassPane':(113,127,142)}[name];im=Image.new('RGBA',(16,16),c+(90,));d=ImageDraw.Draw(im);d.rectangle((0,0,15,15),outline=c+(210,));d.line((3,11,10,4),fill=(235,246,248,150));d.line((8,13,13,8),fill=(235,246,248,120))
 if name=='Lantern':
  d.rectangle((0,0,15,2),fill=(65,58,45));d.rectangle((0,13,15,15),fill=(65,58,45));d.rectangle((1,2,2,13),fill=(78,68,48));d.rectangle((13,2,14,13),fill=(78,68,48));d.rectangle((5,4,10,11),fill=(255,226,130))
 if name=='Chain':d.rectangle((3,0,12,15),outline=(59,66,74),width=3);d.line((5,1,10,1),fill=(198,204,207))
 if name.startswith('Candle'):d.rectangle((6,0,9,2),fill=(68,52,35));d.line((3,4,3,12),fill=(245,218,160))
 if name.startswith('Campfire'):
  d.rectangle((0,0,15,15),fill=(65,48,35));d.line((0,4,15,10),fill=(136,93,49),width=4);d.line((0,12,15,3),fill=(172,117,53),width=4)
  if name.endswith('Lit'):d.polygon([(3,10),(5,4),(8,8),(11,1),(13,11)],fill=(238,132,34));d.polygon([(6,10),(8,6),(10,11)],fill=(255,221,106))
 if name.startswith('Bed'):d.rectangle((0,0,15,4),fill=(234,224,202));d.line((0,5,15,5),fill=(81,67,59));d.line((1,7,1,14),fill=tuple(min(255,c+30) for c in color))
 if name=='Barrel':
  for x in [1,5,10,14]:d.line((x,0,x,15),fill=(85,58,35))
  for y in [2,12]:d.rectangle((0,y,15,y+1),fill=(69,73,75));d.point((3,y),fill=(160,166,164));d.point((12,y),fill=(160,166,164))
 if name.startswith('Painting'):
  pattern=int(name[-1]);d.rectangle((0,0,15,15),fill=(91,56,33));d.rectangle((1,1,14,14),fill=[(89,138,170),(53,71,100),(179,140,91),(80,128,98)][pattern]);d.ellipse((9,2,12,5),fill=(238,211,145));d.polygon([(2,12),(5,5+pattern),(9,12),(12,7),(14,13)],fill=(57,84,75));d.rectangle((2,12,13,13),fill=(77,116,73));d.line((2,14,14,14),fill=(204,154,76))
 if name=='ItemFrame':d.rectangle((0,0,15,15),fill=(106,65,35));d.rectangle((1,1,14,14),outline=(207,158,89));d.rectangle((3,3,12,12),fill=(129,97,59))
 if name=='Scaffold':
  d.rectangle((0,0,15,15),fill=(147,135,76))
  for k in [0,5,10,15]:d.line((k,0,k,15),fill=(82,84,44));d.line((0,k,15,k),fill=(82,84,44))
 world.paste(im,((143+index-45)*16,0));im.save(out/(name+'.png'))
assert world.crop((0,0,w,h)).tobytes()==old.tobytes()
(assets/'crafting_blocks.rgba').write_bytes(struct.pack('<ii',*world.size)+world.tobytes());world.save(assets/'crafting_blocks.png')
# Consistent isometric icons use the same original material images and shape silhouettes.
entries=re.findall(r'BUILDING\((\w+), (\w+), Block::(\w+), Item::(\w+)\)',(root/'include/BuildingContent.inc').read_text())
oldIcons=Image.open(assets/'inventory_atlas_16.png').convert('RGBA');atlas=Image.new('RGBA',(160,((348+9)//10)*16));atlas.paste(oldIcons)
for k,(name,shape,material,ingredient) in enumerate(entries):
 c=next((v for family,v in colors.items() if name.startswith(family)),(135,139,140));
 if 'Sandstone' in name:c=(219,197,138)
 if 'Deepslate' in name:c=(75,81,88)
 if name=='Lantern':c=(231,178,70)
 if 'Glass' in name:c=(165,213,219)
 if name in ['Bed','Carpet']:c=(175,55,58)
 if name=='FlowerPot':c=(164,87,58)
 if name in names:
  tex=Image.open(out/(name+'.png'));pix=list(tex.getdata());c=tuple(sum(p[j] for p in pix)//len(pix) for j in range(3))
 im=Image.new('RGBA',(32,32));d=ImageDraw.Draw(im)
 def box(x,y,z,X,Y,Z):
  def project(a,b,c):return (16+5*(a-c),16-7*b+2.5*(a+c))
  top=[project(x,Y,z),project(X,Y,z),project(X,Y,Z),project(x,Y,Z)];front=[project(x,y,Z),project(X,y,Z),project(X,Y,Z),project(x,Y,Z)];side=[project(X,y,z),project(X,y,Z),project(X,Y,Z),project(X,Y,z)]
  for points,factor in [(front,.76),(side,.54),(top,1.12)]:d.polygon(points,fill=tuple(min(255,int(v*factor)) for v in c)+(255,),outline=tuple(int(v*.35) for v in c)+(255,))
 if shape=='Stairs':box(0,0,0,1,.5,1);box(0,.5,.5,1,1,1)
 elif shape in ['Slab','Carpet','Path','PressurePlate']:box(0,0,0,1,.12 if shape in ['Carpet','PressurePlate'] else .5,1)
 elif shape in ['Fence','Wall']:box(.35,0,.35,.65,1,.65);box(0,.4,.4,1,.6,.6);box(0,.8,.4,1,.95,.6)
 elif shape in ['Door','Trapdoor','Pane','Sign','Painting','Frame','Shelf']:box(0,0,.4,1,1,.6)
 elif shape in ['Table','Chair','Bench']:box(.05,0,.1,.2,.75,.25);box(.8,0,.75,.95,.75,.9);box(0,.65,0,1,.85,1);(box(0,.85,.8,1,1.25,1) if shape=='Chair' else None)
 elif shape in ['Lantern','Pot','Candle','Button','Chain']:box(.25,0,.25,.75,.7,.75)
 elif shape=='Bed':box(0,.1,0,1,.5,1)
 else:box(0,0,0,1,1,1)
 # Render complete geometry before centering; the old projection clipped at y=17.
 glyph=im.crop(im.getbbox())
 if max(glyph.size)>14:
  factor=14/max(glyph.size);glyph=glyph.resize(tuple(max(1,round(v*factor)) for v in glyph.size),Image.Resampling.NEAREST)
 im=Image.new('RGBA',(16,16));im.paste(glyph,((16-glyph.width)//2,(16-glyph.height)//2))
 bounds=im.getbbox();assert bounds and min(bounds[:2])>=1 and max(bounds[2:])<=15
 assert abs(bounds[0]+bounds[2]-16)<=1 and abs(bounds[1]+bounds[3]-16)<=1
 item=206+k;atlas.paste(im,((item%10)*16,(item//10)*16));im.save(assets/'inventory'/(re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png'))
for i in range(206):
 box=((i%10)*16,(i//10)*16,(i%10+1)*16,(i//10+1)*16);assert atlas.crop(box).tobytes()==oldIcons.crop(box).tobytes()
atlas.save(assets/'inventory_atlas_16.png');large=atlas.resize((640,atlas.height*4),Image.Resampling.NEAREST);large.save(assets/'item_icons_expansion.png');(assets/'item_icons_expansion.rgba').write_bytes(struct.pack('<ii',*large.size)+large.tobytes())
print('44 original texture tiles and 142 isometric icons; historical pixels unchanged')
