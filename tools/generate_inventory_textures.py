"""Original 16px inventory art. Requires Pillow and a C++17 compiler (default: g++).

Reads authoritative item/cell registrations and samples actual world block tiles.
No runtime dependency: exports the existing PNG/RGBA atlas and native artwork.
Usage: python tools/generate_inventory_textures.py [--compiler g++]
"""
from pathlib import Path
import argparse
import hashlib
import re
import struct
import subprocess
import tempfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUT = ASSETS / "inventory"
OUT.mkdir(exist_ok=True)
ATLAS_TILES=int(re.search(r"BlockAtlasTiles = (\d+)",(ROOT / "include/Definitions.h").read_text()).group(1))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", default="g++")
args = parser.parse_args()

def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

# Compile only the renderer's CPU pixel authoring code, without OpenGL/game code.
# This keeps icon colors/material patterns in sync with the placed-block source.
source = (ROOT / "src/Renderer.cpp").read_text()
body = function(source, "GLuint Renderer::createAtlasTexture()")
body = body[body.index("{")+1:body.index("#ifdef VOXEL_STANDALONE")]
helpers = function(source, "std::uint8_t hashPixel(") + "\n" + function(source, "void putPixel(")
cpp = "#include <algorithm>\n#include <cmath>\n#include <cstdint>\n#include <vector>\n#include <iostream>\n"
cpp += "constexpr int BlockAtlasTiles=" + str(ATLAS_TILES) + ";\n" + helpers
cpp += "\nint main(){" + body + "std::cout.write(reinterpret_cast<const char*>(p.data()),p.size());}\n"
with tempfile.TemporaryDirectory(prefix="voxel-icon-art-") as temp:
    temp = Path(temp)
    (temp / "tiles.cpp").write_text(cpp)
    exe = temp / "tiles.exe"
    subprocess.run([args.compiler, "-std=c++17", "-O0", str(temp / "tiles.cpp"), "-o", str(exe)], check=True, capture_output=True)
    raw = subprocess.check_output([str(exe)])
world = Image.frombytes("RGBA", (ATLAS_TILES*16,16), raw)
extra = Image.open(ASSETS / "crafting_blocks.png").convert("RGBA")
world.paste(extra, (45*16,0))
tiles = [world.crop((i*16,0,(i+1)*16,16)) for i in range(ATLAS_TILES)]

definitions = (ROOT / "src/Definitions.cpp").read_text()
items = [(name,int(index)) for name,index in re.findall(r'put\(Item::(\w+),\s*\d+,\s*(-?\d+),\s*"[^"]+"\)',definitions) if name!="None"]
placements = dict(re.findall(r'\{Item::(\w+), Block::(\w+)\}',definitions.split('placements[] = {')[1].split('};')[0]))
blocks = {name:(int(top),int(side),int(bottom)) for name,top,side,bottom in re.findall(r'put\(Block::(\w+),\s*\d+,\s*(\d+),\s*(\d+),\s*(\d+),', definitions)}

def canvas():
    image=Image.new("RGBA",(16,16))
    return image,ImageDraw.Draw(image)

def color(rgb, gain):
    return tuple(max(0,min(255,round(c*gain))) for c in rgb[:3])+(255,)

def clean(image):
    # Binary alpha; zero hidden RGB. No accidental disconnected one-pixel specks.
    occupied={(x,y) for y in range(16) for x in range(16) if image.getpixel((x,y))[3]}
    remaining=set(occupied)
    while remaining:
        seed=next(iter(remaining)); remaining.remove(seed); group={seed}; queue=[seed]
        while queue:
            x,y=queue.pop()
            for nx,ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
                if (nx,ny) in remaining:
                    remaining.remove((nx,ny)); group.add((nx,ny)); queue.append((nx,ny))
        if len(group)==1: occupied-=group
    for y in range(16):
        for x in range(16):
            rgba=image.getpixel((x,y))
            image.putpixel((x,y),rgba[:3]+(255,) if (x,y) in occupied else (0,0,0,0))
    return image

def cube(block, height=1):
    image,_=canvas()
    top,side,_=blocks[block]
    # One camera/projection for every cube; partial blocks only change height.
    rise=7*height
    top_y=8-rise
    faces=[((8,top_y),(6,3),(-6,3),tiles[top],1.00),
           ((2,top_y+3),(6,3),(0,rise),tiles[side],.82),
           ((8,top_y+6),(6,-3),(0,rise),tiles[side],.64)]
    for origin,a,b,texture,gain in faces:
        det=a[0]*b[1]-a[1]*b[0]
        for y in range(1,15):
            for x in range(1,15):
                px,py=x+.5-origin[0],y+.5-origin[1]
                u=(px*b[1]-py*b[0])/det;v=(a[0]*py-a[1]*px)/det
                if not (0<=u<1 and 0<=v<1): continue
                rgba=texture.getpixel((min(15,int(u*16)),min(15,int(v*16))))
                if rgba[3]==0 or (block=='Glass' and rgba[3]<150): continue
                edge=.91 if min(u,v,1-u,1-v)<.06 else 1
                image.putpixel((x,y),color(rgba,gain*edge))
    if block=='Glass':
        # A connected original frame keeps transparent glass legible on either
        # UI background; the clear face interiors retain binary transparency.
        d=ImageDraw.Draw(image)
        d.line([(8,1),(2,4),(2,11),(8,14),(14,11),(14,4),(8,1)],fill=(109,167,183),width=1)
        d.line([(2,4),(8,7),(14,4)],fill=(177,224,232),width=1)
        d.line([(8,7),(8,14)],fill=(125,186,198),width=1)
        d.line([(8,1),(14,4)],fill=(206,239,242),width=1)
    return clean(image)

def shape(points,base):
    image,d=canvas();d.polygon(points,fill=base+(255,))
    mask=image.copy()
    for y in range(16):
        for x in range(16):
            if mask.getpixel((x,y))[3]:
                boundary=any(not (0<=nx<16 and 0<=ny<16) or not mask.getpixel((nx,ny))[3] for nx,ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)))
                gain=.60 if boundary else 1.12 if x+y<14 else .88 if x+y>21 else 1
                image.putpixel((x,y),color(base,gain))
    return image

materials={'Wood':(157,103,53),'Stone':(136,142,146),'Iron':(205,215,220),'Gold':(236,183,54),'Diamond':(62,200,191)}
heads={
    'Pickaxe':[(3,4),(5,2),(10,2),(13,4),(14,7),(12,7),(11,5),(7,4),(5,5)],
    'Axe':[(7,2),(11,2),(13,4),(12,7),(8,8),(7,6)],
    'Shovel':[(10,2),(13,3),(14,5),(11,8),(8,7),(7,5)],
    'Sword':[(12,1),(14,2),(13,5),(7,11),(5,9)]}

def tool(material,kind):
    image,d=canvas()
    d.line([(3,13),(10,6)],fill=(64,41,25,255),width=3)
    d.line([(3,12),(9,6)],fill=(159,111,61,255),width=1)
    head=shape(heads[kind],materials[material]);image.alpha_composite(head)
    if kind=='Sword':
        d=ImageDraw.Draw(image)
        d.line([(4,7),(9,12)],fill=color(materials[material],.65),width=2)
        d.line([(4,7),(8,11)],fill=color(materials[material],1.1),width=1)
    return clean(image)

def sprite(name):
    image,d=canvas()
    if name in ('TallGrass','RedFlower','YellowFlower','SugarCane','Vine'):
        block=placements[name];tile=blocks.get(block, (58,58,58))[1]
        image.paste(tiles[tile].crop((1,1,15,15)),(1,1));return clean(image)
    if name=='Torch':
        d.rectangle((7,6,8,14),fill=(111,71,34));d.line((7,7,7,13),fill=(179,126,62))
        d.polygon([(6,6),(6,3),(7,1),(9,3),(10,5),(9,7)],fill=(238,125,30));d.rectangle((7,3,8,5),fill=(255,222,100))
    elif name=='Ladder':
        d.rectangle((3,2,4,14),fill=(122,77,34));d.rectangle((11,2,12,14),fill=(122,77,34))
        for y in (4,7,10,13):d.rectangle((4,y,11,y+1),fill=(184,127,62))
    elif name=='WoodenDoor':
        image=shape([(4,1),(11,1),(11,14),(4,14)],(162,105,45));d=ImageDraw.Draw(image)
        d.rectangle((6,3,9,6),fill=(80,54,28));d.rectangle((6,9,9,12),fill=(128,80,37));d.point((10,8),fill=(235,189,62))
    elif name=='Stick':
        d.line([(3,13),(12,3)],fill=(75,46,26),width=3);d.line([(3,12),(11,3)],fill=(186,133,72),width=1)
    elif name=='Seeds':
        for x,y in [(4,9),(7,6),(10,10)]:
            d.polygon([(x,y-2),(x+2,y-1),(x+1,y+2),(x-1,y+1)],fill=(98,150,49));d.point((x,y),fill=(180,196,85))
    elif name=='Wheat':
        d.line((7,14,8,3),fill=(121,100,36),width=2)
        for y in (4,7,10):
            d.polygon([(7,y),(4,y-2),(3,y),(6,y+2)],fill=(208,157,43));d.polygon([(8,y),(11,y-2),(12,y),(9,y+2)],fill=(240,194,63))
    elif name in ('Coal','Charcoal'):
        points=[(3,6),(6,3),(11,4),(13,8),(11,12),(5,13),(2,10)] if name=='Coal' else [(4,4),(10,2),(13,6),(11,13),(5,13),(2,9)]
        image=shape(points,(63,66,71) if name=='Coal' else (62,56,51));d=ImageDraw.Draw(image)
        d.line((5,6,8,4),fill=(105,108,112));d.line((5,10,9,11),fill=(28,30,34))
    elif name in ('IronIngot','GoldIngot','CopperIngot','Brick'):
        base={'IronIngot':(198,208,213),'GoldIngot':(230,176,48),'CopperIngot':(193,110,66),'Brick':(176,79,48)}[name]
        image=shape([(2,7),(9,4),(13,6),(13,10),(6,13),(2,10)],base);d=ImageDraw.Draw(image)
        d.polygon([(3,7),(9,5),(12,6),(6,9)],fill=color(base,1.15));d.line((6,9,6,12),fill=color(base,.70))
    elif name=='Diamond':
        image=shape([(5,3),(11,3),(14,7),(8,14),(2,7)],(52,186,184));d=ImageDraw.Draw(image)
        d.polygon([(5,4),(8,4),(6,7),(3,7)],fill=(167,240,226));d.polygon([(7,8),(10,8),(8,12)],fill=(90,215,203))
    elif name in ('ClayBall','Snowball','Wool'):
        base={'ClayBall':(134,150,170),'Snowball':(232,242,246),'Wool':(214,210,191)}[name]
        image=shape([(5,3),(10,3),(13,6),(13,10),(10,13),(5,13),(2,10),(2,6)],base);d=ImageDraw.Draw(image)
        d.line((5,5,9,4),fill=color(base,1.1));d.line((4,6,4,8),fill=color(base,1.1))
        if name=='Wool':
            for x,y in [(6,7),(9,6),(8,10)]:d.line((x,y,x+2,y+1),fill=color(base,.84))
    elif name=='Paper':
        image=shape([(3,2),(11,2),(13,4),(13,13),(3,13)],(229,220,187));d=ImageDraw.Draw(image)
        d.polygon([(10,3),(12,5),(10,5)],fill=(164,157,132));d.line((5,7,10,7),fill=(182,174,146));d.line((5,10,9,10),fill=(182,174,146))
    elif name=='Book':
        image=shape([(3,2),(10,2),(13,5),(13,13),(4,14),(2,11)],(139,75,43));d=ImageDraw.Draw(image)
        d.rectangle((5,4,10,10),fill=(180,112,60));d.line((4,3,4,12),fill=(221,165,93));d.polygon([(5,12),(11,11),(12,12),(5,13)],fill=(230,218,180))
    elif name=='Leather':
        image=shape([(3,2),(6,3),(9,2),(13,4),(11,7),(13,11),(10,14),(7,12),(3,13),(2,9),(4,6)],(158,93,47))
    elif name=='Apple':
        image=shape([(4,5),(7,4),(9,5),(12,4),(14,7),(13,11),(10,14),(7,13),(4,14),(2,10),(2,7)],(192,51,44));d=ImageDraw.Draw(image)
        d.line((8,5,8,2),fill=(95,63,30),width=2);d.line((9,3,12,2),fill=(84,145,52),width=2);d.line((4,7,4,9),fill=(240,106,78))
    elif name=='Bread':
        image=shape([(2,8),(4,5),(8,3),(12,4),(14,7),(13,11),(9,13),(4,13),(2,11)],(196,135,57));d=ImageDraw.Draw(image)
        for x,y in [(5,6),(8,5),(11,6)]:d.line((x,y,x-1,y+2),fill=(237,187,100),width=2)
    elif name in ('RawMeat','RawBeef','RawPork','RawMutton','CookedBeef','CookedPork','CookedMutton'):
        pork='Pork' in name;mutton='Mutton' in name;cooked=name.startswith('Cooked')
        points=[(3,6),(5,3),(10,3),(13,6),(12,11),(8,13),(3,11)]
        if mutton:points=[(6,3),(11,3),(13,6),(11,9),(8,11),(6,14),(3,13),(5,9),(3,6)]
        if name=='RawMeat':points=[(3,5),(9,3),(13,6),(12,12),(6,13),(2,9)]
        if pork:points=[(4,3),(10,3),(13,5),(14,9),(11,12),(5,13),(2,10),(2,6)]
        base=((183,116,66) if pork else (136,80,43)) if cooked else (224,137,132) if pork else (173,61,63) if mutton else (181,64,61)
        image=shape(points,base);d=ImageDraw.Draw(image)
        d.line([(5,6),(7,5),(10,6)],fill=(198,143,83) if cooked else (240,190,173),width=2)
        if mutton:d.line((4,12,5,10),fill=(225,207,171),width=2)
    else:raise ValueError('Missing original sprite: '+name)
    return clean(image)

art={}
for name,index in items:
    match=re.fullmatch('(Wood|Stone|Iron|Gold|Diamond)(Pickaxe|Axe|Shovel|Sword)',name)
    if match:image=tool(*match.groups())
    elif name in placements and name not in ('Torch','WoodenDoor','Ladder','TallGrass','RedFlower','YellowFlower','SugarCane','Vine'):
        height=.4 if name.endswith('Slab') else .16 if name=='Snow' else 1
        image=cube(placements[name],height)
    else:image=sprite(name)
    art[name]=image
    filename=re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png'
    image.save(OUT/filename)

native=Image.new('RGBA',(160,176))
for name,index in items:native.paste(art[name],((index%10)*16,(index//10)*16))
native.save(ASSETS/'inventory_atlas_16.png')
atlas=native.resize((640,704),Image.Resampling.NEAREST)
atlas.save(ASSETS/'item_icons_expansion.png')
(ASSETS/'item_icons_expansion.rgba').write_bytes(struct.pack('<ii',*atlas.size)+atlas.tobytes())

# Validate registry alignment, binary alpha/transparent RGB, gutters, silhouettes,
# and uniqueness. Every tool material must share its family's alpha silhouette.
assert len(items)==101 and len({index for _,index in items})==101
assert len(art)==101
hashes={}
for name,index in items:
    image=art[name]
    assert any(image.getchannel('A').tobytes()),name
    assert all(a in (0,255) for a in image.getchannel('A').tobytes()),name
    assert all(image.getpixel((x,y))==(0,0,0,0) for y in range(16) for x in range(16) if image.getpixel((x,y))[3]==0),name
    assert all(image.getpixel((x,y))[3]==0 for x,y in [(i,0) for i in range(16)]+[(i,15) for i in range(16)]+[(0,i) for i in range(16)]+[(15,i) for i in range(16)]),name
    cell=native.crop(((index%10)*16,(index//10)*16,(index%10+1)*16,(index//10+1)*16))
    assert cell.tobytes()==image.tobytes(),name
    digest=hashlib.sha256(image.tobytes()).hexdigest()
    assert digest not in hashes,(name,hashes.get(digest))
    hashes[digest]=name
for kind in heads:
    assert len({art[material+kind].getchannel('A').tobytes() for material in materials})==1,kind
print('Regenerated/validated 101 icons: 56 block/plant icons + 45 other item sprites; 16px artwork, 640x704 atlas, 4px transparent gutters')
