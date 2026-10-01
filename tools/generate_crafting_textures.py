"""Author original deterministic 16px crafting textures; Pillow is build-time only.

Existing item slots 0..83 remain byte-identical. Run from any directory.
"""
from pathlib import Path
import random
import struct
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUT = ASSETS / "crafting"
OUT.mkdir(exist_ok=True)

def canvas():
    image = Image.new("RGBA", (16, 16))
    return image, ImageDraw.Draw(image)

def material(name, color, kind="metal"):
    image, draw = canvas()
    rng = random.Random(name)
    for y in range(16):
        for x in range(16):
            noise = rng.randint(-7, 7)
            edge = -25 if x in (0, 15) or y in (0, 15) else 0
            if kind == "polished":
                edge += 8 if x == 1 or y == 1 else 0
                noise //= 2
            elif kind == "coal":
                edge += -12 if (x + y * 2) % 7 == 0 else 0
            elif kind == "sand":
                edge += -13 if y in (5, 11) and (x + y) % 4 != 0 else 0
            elif kind == "hay":
                edge += 15 if x % 3 == 1 else -7
                edge += -64 if y in (4, 5, 11, 12) else 0
            elif kind == "hay_top":
                edge += 16 if (x * 3 + y) % 5 == 0 else -4
            else:
                edge += 10 if x + y < 12 else -3
            image.putpixel((x, y), tuple(max(0, min(255, c + noise + edge)) for c in color) + (255,))
    return image

items = {}
image, d = canvas()
d.polygon([(3,2),(11,2),(13,4),(12,13),(3,13),(2,11)], fill=(222,214,183), outline=(92,85,73))
d.line([(4,4),(10,4),(11,5)], fill=(255,247,219))
d.line([(4,8),(10,8)], fill=(172,164,140))
d.line([(4,10),(8,10)], fill=(172,164,140))
items["paper"] = image
image, d = canvas()
d.polygon([(3,3),(11,2),(13,4),(13,12),(5,14),(2,11),(2,5)], fill=(108,61,39), outline=(45,29,24))
d.polygon([(5,11),(12,9),(12,12),(5,13)], fill=(227,212,172))
d.line([(4,4),(4,10)], fill=(178,109,56), width=2)
d.rectangle((7,4,10,6), fill=(181,140,68))
items["book"] = image
for name, color in [("clay_ball",(153,169,183)),("snowball",(227,240,246)),("charcoal",(49,45,44))]:
    image, d = canvas()
    d.polygon([(5,2),(10,2),(13,5),(13,10),(10,13),(4,12),(2,9),(2,5)], fill=color,
              outline=tuple(max(0,c-45) for c in color))
    d.line([(5,4),(9,4),(11,6)], fill=tuple(min(255,c+24) for c in color), width=2)
    d.line([(4,9),(6,11),(10,11)], fill=tuple(max(0,c-20) for c in color))
    items[name] = image
image,d=canvas()
d.polygon([(2,6),(10,3),(14,6),(14,10),(6,13),(2,10)], fill=(169,76,48), outline=(73,35,28))
d.polygon([(3,6),(10,4),(13,6),(6,9)], fill=(220,119,75))
d.line([(6,9),(6,12)], fill=(119,47,34))
items["brick"] = image
image,d=canvas()
for x, top in [(3,4),(7,1),(11,3)]:
    d.rectangle((x,top,x+2,15), fill=(82,127,44))
    d.line((x+1,top,x+1,15), fill=(177,200,81))
    for y in range(top+3,15,4):
        d.line((x,y,x+2,y), fill=(43,89,38))
    d.line((x+1,top+2,x+4,top), fill=(102,169,51))
items["sugar_cane"] = image
image,d=canvas()
d.line([(4,0),(6,5),(9,9),(8,15)], fill=(55,95,32), width=2)
for x,y in [(2,2),(7,3),(4,6),(10,7),(6,10),(10,12),(5,14)]:
    d.polygon([(x,y),(x+3,y-1),(x+3,y+1),(x+1,y+2)], fill=(73,139,44))
    d.point((x+1,y), fill=(132,179,66))
items["vine"] = image
specs = [
    ("coal_block",(49,52,56),"coal"), ("iron_block",(184,194,196),"metal"),
    ("gold_block",(229,178,51),"metal"), ("copper_block",(184,103,65),"metal"),
    ("diamond_block",(68,189,186),"metal"), ("hay_bale",(184,145,49),"hay"),
    ("hay_top",(200,163,64),"hay_top"), ("sandstone",(202,179,124),"sand"),
    ("sandstone_top",(218,197,149),"polished"),
    ("polished_granite",(153,104,86),"polished"),
    ("polished_diorite",(197,197,184),"polished"),
    ("polished_andesite",(130,140,137),"polished")]
blocks = [material(*spec) for spec in specs] + [items["sugar_cane"], items["vine"]]
for spec, image in zip(specs, blocks):
    items[spec[0]] = image
for name, image in items.items():
    image.save(OUT / (name + ".png"))

block_atlas = Image.new("RGBA", (16 * len(blocks), 16))
for index, image in enumerate(blocks):
    block_atlas.paste(image, (index * 16, 0))
block_atlas.save(ASSETS / "crafting_blocks.png")
(ASSETS / "crafting_blocks.rgba").write_bytes(struct.pack("<ii", *block_atlas.size) + block_atlas.tobytes())

names = ["paper","book","clay_ball","brick","snowball","charcoal","sugar_cane","vine",
         "coal_block","iron_block","gold_block","copper_block","diamond_block","hay_bale",
         "sandstone","polished_granite","polished_diorite","polished_andesite"]
raw = (ASSETS / "item_icons_expansion.rgba").read_bytes()
width,height = struct.unpack("<ii",raw[:8])
base = Image.frombytes("RGBA",(width,height),raw[8:])
atlas = Image.new("RGBA", (640,704))
atlas.paste(base.crop((0,0,640,640)), (0,0))
for index,name in enumerate(names,84):
    atlas.paste(items[name].resize((64,64), Image.Resampling.NEAREST), ((index%10)*64,(index//10)*64))
atlas.save(ASSETS / "item_icons_expansion.png")
(ASSETS / "item_icons_expansion.rgba").write_bytes(struct.pack("<ii", *atlas.size) + atlas.tobytes())
print("Generated 20 original 16x16 textures, 14 block tiles, and 18 inventory sprites")
