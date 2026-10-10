"""Append original native 16px biome art; preserve every existing atlas cell."""
from pathlib import Path
import random, re, struct
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets'
entries=re.findall(r'\{Block::(\w+),Item::\w+,"\w+",(\d+),(\d+),(\d+),BiomeMaterial::(\w+)\}',(ROOT/'include/BiomeContent.h').read_text())
world=Image.new('RGBA',(51*16,16))
source=(ASSETS/'crafting_blocks.rgba').read_bytes()
old=Image.frombytes('RGBA',struct.unpack('<ii',source[:8]),source[8:])
world.paste(old.crop((0,0,240,16)),(0,0))
palette={
 'Spruce':(100,69,39), 'Jungle':(159,111,65),'Acacia':(185,105,58),'DarkOak':(77,48,29),
 'Podzol':(115,78,45),'Mycelium':(143,119,149),'CoarseDirt':(128,93,59),
 'PackedIce':(133,185,221),'BlueIce':(79,149,207),'Terracotta':(158,98,76),
 'TerracottaRed':(160,66,52),'TerracottaOrange':(196,107,59),'TerracottaYellow':(206,162,73),
 'TerracottaWhite':(212,184,161),'TerracottaBrown':(109,76,59),
 'MushroomStem':(202,191,157),'RedMushroomBlock':(174,49,45),'BrownMushroomBlock':(142,109,79)}
tiles={}
def grain(name,tile):
    rng=random.Random(tile*7319)
    base=palette.get(name,next((v for k,v in palette.items() if name.startswith(k)),(91,139,52)))
    im=Image.new('RGBA',(16,16));d=ImageDraw.Draw(im)
    for y in range(16):
        for x in range(16):
            n=rng.randrange(-10,11)
            if name.endswith('Log'):n+=-15 if x%4==0 else 8 if x%4==2 else 0
            if name.endswith('Planks'):n+=-24 if y%4==3 else 0
            if 'Ice' in name:n+=10 if (x+y)%13==0 else 0
            if name.endswith('Leaves'):base=(54,109,54) if name.startswith('Spruce') else (72,129,47) if name.startswith('Jungle') else (91,123,44) if name.startswith('Acacia') else (48,96,41)
            im.putpixel((x,y),tuple(max(0,min(255,c+n)) for c in base)+(255,))
    if name.endswith('LogTop'):
        for r in (2,5,7):d.rectangle((r,r,15-r,15-r),outline=tuple(max(0,c-26) for c in base)+(255,))
    if name in ('RedMushroomBlock','BrownMushroomBlock'):
        for x,y in ((2,2),(10,6),(4,12)):d.rectangle((x,y,x+2,y+1),fill=(224,203,167,255))
    if name=='Mycelium':
        for x,y in ((1,4),(7,9),(12,2)):d.line((x,y,x+2,y),fill=(184,159,193,255))
    return im
def plant(name):
    im=Image.new('RGBA',(16,16));d=ImageDraw.Draw(im)
    if name=='Bamboo':
        d.rectangle((7,0,9,15),fill=(118,159,52,255))
        for y in (3,9,14):d.line((7,y,9,y),fill=(64,105,36,255))
        d.line((8,5,3,2),fill=(68,126,44,255),width=2);d.line((9,10,13,7),fill=(82,143,49,255),width=2)
    elif name in ('Fern','DeadBush'):
        color=(69,128,59,255) if name=='Fern' else (130,97,51,255)
        d.line((8,3,8,15),fill=color)
        for y in (5,8,11):
            for side in (-1,1):d.line((8,y+2,8+side*(y//2),y),fill=color,width=2)
    elif name=='LilyPad':
        d.polygon([(2,4),(7,1),(12,3),(14,9),(10,13),(3,12),(1,8)],fill=(65,118,53,255))
        d.polygon([(7,7),(13,3),(15,6)],fill=(0,0,0,0));d.line((3,8,8,6),fill=(104,150,69,255))
    else:
        d.rectangle((7,8,9,14),fill=(205,185,146,255))
        d.polygon([(2,8),(4,4),(7,2),(11,3),(14,8)],fill=(183,54,45,255) if name=='RedMushroom' else (140,107,73,255))
        d.rectangle((5,5,6,6),fill=(232,216,181,255));d.rectangle((10,6,11,7),fill=(232,216,181,255))
    return im
for name,side,top,bottom,material in entries:
    for tile,variant in ((int(side),name),(int(top),name+'Top' if top!=side else name)):
        if tile in tiles:continue
        tiles[tile]=plant(name) if material=='Plant' else grain(variant,tile)
        world.paste(tiles[tile],((tile-45)*16,0))
world.save(ASSETS/'crafting_blocks.png')
(ASSETS/'crafting_blocks.rgba').write_bytes(struct.pack('<ii',*world.size)+world.tobytes())
old_native=Image.open(ASSETS/'inventory_atlas_16.png').convert('RGBA')
native=Image.new('RGBA',(160,224));native.paste(old_native,(0,0))
enum=(ROOT/'include/Survival.h').read_text().split('enum class Item')[1].split('};')[0]
enum=re.sub(r'//[^\n]*','',enum)
names=[v.strip().split('=')[0].strip() for v in enum.split('{',1)[1].split(',') if v.strip()]
def cube(side,top):
    im=Image.new('RGBA',(16,16))
    for origin,a,b,tex,gain in [((8,1),(6,3),(-6,3),tiles[top],1),((2,4),(6,3),(0,7),tiles[side],.82),((8,7),(6,-3),(0,7),tiles[side],.64)]:
        det=a[0]*b[1]-a[1]*b[0]
        for y in range(1,15):
            for x in range(1,15):
                px,py=x+.5-origin[0],y+.5-origin[1]
                u=(px*b[1]-py*b[0])/det;v=(a[0]*py-a[1]*px)/det
                if 0<=u<1 and 0<=v<1:
                    c=tex.getpixel((min(15,int(u*16)),min(15,int(v*16))))
                    edge=.91 if min(u,v,1-u,1-v)<.06 else 1
                    im.putpixel((x,y),tuple(round(ch*gain*edge) for ch in c[:3])+(255,))
    return im
for name,side,top,bottom,material in entries:
    index=names.index(name)
    if material=='Plant':
        icon=Image.new('RGBA',(16,16));icon.paste(tiles[int(side)].resize((12,12),Image.Resampling.NEAREST),(2,2))
    else:icon=cube(int(side),int(top))
    native.paste(icon,((index%10)*16,(index//10)*16))
    filename=re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png'
    icon.save(ASSETS/'inventory'/filename)
for index in range(names.index('SpruceLog')):
    box=((index%10)*16,(index//10)*16,(index%10+1)*16,(index//10+1)*16)
    assert native.crop(box).tobytes()==old_native.crop(box).tobytes(),index
native.save(ASSETS/'inventory_atlas_16.png')
atlas=native.resize((640,896),Image.Resampling.NEAREST)
atlas.save(ASSETS/'item_icons_expansion.png')
(ASSETS/'item_icons_expansion.rgba').write_bytes(struct.pack('<ii',*atlas.size)+atlas.tobytes())
print('Appended 36 world tiles and 32 icons; existing cells unchanged.')
