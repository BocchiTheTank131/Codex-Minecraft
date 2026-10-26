"""Bake cached 64px previews from the production mesher, textures and boat models.

Requires a Release build, MSVC/CMake and Pillow on the authoring machine only.
The game does not run this tool or create thumbnails while opening inventory.
Legacy 16px files remain source artwork for silhouette-shaped non-block items.
"""
from pathlib import Path
import argparse, csv, hashlib, json, struct, subprocess, time, os, uuid, io, re
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
version=re.search(r'project\(VoxelFrontier VERSION ([0-9.]+)',(ROOT/'CMakeLists.txt').read_text()).group(1)
validation=ROOT/('build/v'+version.replace('.','')+'-validation')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=validation/'generated')
parser.add_argument('--skip-build',action='store_true')
parser.add_argument('--check',action='store_true')
args=parser.parse_args()
assets=ROOT/'assets';manifest=assets/'inventory_previews.json'
inputs=['src/World.cpp','src/Renderer.cpp','src/Building.cpp','src/Definitions.cpp','src/Transportation.cpp',
        'include/Block.h','include/Survival.h','include/BuildingContent.inc','include/Definitions.h',
        'assets/crafting_blocks.rgba','assets/inventory_atlas_16.png','tests/stability/icons.cpp',
        'tools/generate_item_thumbnails.py','src/Automation.cpp','include/Automation.h','include/Building.h','include/AutomationContent.inc','tools/generate_automation_assets.py']
def atomic_bytes(path, data):
    path=Path(path);temporary=path.with_name(path.name+'.'+uuid.uuid4().hex+'.tmp')
    temporary.write_bytes(data)
    for attempt in range(4):
        try:
            os.replace(temporary,path);return
        except OSError:
            if attempt==3: raise
            time.sleep(.05*(attempt+1))
def save_png(image, path):
    buffer=io.BytesIO();image.save(buffer,format='PNG');atomic_bytes(path,buffer.getvalue())
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def verify():
    data=json.loads(manifest.read_text())
    for name,expected in data['inputs'].items():
        assert digest(ROOT/name)==expected, 'Rebake thumbnails after changing '+name
    for name,expected in data['outputs'].items():
        assert digest(ROOT/name)==expected, 'Thumbnail asset changed: '+name
    print('Thumbnail source fingerprint and all exported assets verified:',data['items'],'items')
if args.check:
    verify();raise SystemExit
start=time.perf_counter();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
art=Image.open(assets/'inventory_atlas_16.png').convert('RGBA')
source=assets/'inventory_art_source.rgba';atomic_bytes(source,struct.pack('<ii',*art.size)+art.tobytes())
if not args.skip_build:
    subprocess.run(['cmake','--build',str(ROOT/'build/installed-release'),'--config','Release','--parallel','4','--','/nodeReuse:false'],check=True)
    subprocess.run(['cmake','-S',str(ROOT/'tests/stability'),'-B',str(validation/'driver'),'-G','Visual Studio 18 2026','-A','x64'],check=True)
    subprocess.run(['cmake','--build',str(validation/'driver'),'--config','Release','--parallel','2','--','/nodeReuse:false'],check=True)
driver=validation/'driver/Release/StabilityDriver.exe'
with (output/'native-export.log').open('w') as log:
    subprocess.run([str(driver),'--icons-export',str(output),str(source)],check=True,stdout=log,stderr=subprocess.STDOUT)
blob=(output/'thumbnails-128.rgba').read_bytes();width,height=struct.unpack('<ii',blob[:8]);assert width==1280 and len(blob)==8+width*height*4
image=Image.frombytes('RGBA',(width,height),blob[8:]);atlas=Image.new('RGBA',(width//2,height//2))
entries=list(csv.DictReader((output/'coverage.csv').open()));names=set();sprites=set();records=[]
individual=assets/'inventory_previews';individual.mkdir(exist_ok=True)
for entry in entries:
    id=int(entry['item']);cell=int(entry['sprite']);assert id not in names and cell not in sprites
    names.add(id);sprites.add(cell);x,y=cell%10*128,cell//10*128
    tile=image.crop((x,y,x+128,y+128));visible=tile.getbbox();assert visible,(id,'invisible source')
    glyph=tile.crop(visible);factor=56/max(glyph.size)
    size=tuple(max(1,round(v*factor)) for v in glyph.size)
    glyph=glyph.convert('RGBa').resize(size,Image.Resampling.LANCZOS).convert('RGBA')
    icon=Image.new('RGBA',(64,64));icon.paste(glyph,((64-size[0])//2,(64-size[1])//2))
    # Remove negligible reconstruction coverage from fully transparent gutters.
    icon.putalpha(icon.getchannel('A').point(lambda a:0 if a<12 else a))
    box=icon.getbbox();assert box and min(box[:2])>=1 and max(box[2:])<=63,(id,'clipped',box)
    assert max(box[2]-box[0],box[3]-box[1])>=45,(id,'too small',box)
    # Extend edge RGB into transparent pixels without extending alpha. Linear
    # UI sampling then avoids dark fringes; every cell retains its own gutter.
    for iteration in range(2):
        previous=icon.copy();updates=[]
        for py in range(1,63):
            for px in range(1,63):
                if previous.getpixel((px,py))[3]:continue
                for dx,dy in [(0,-1),(-1,0),(1,0),(0,1)]:
                    c=previous.getpixel((px+dx,py+dy))
                    if c[3]:updates.append((px,py,c[:3]+(0,)));break
        for px,py,c in updates:icon.putpixel((px,py),c)
    filename=f'{id:03d}.png';save_png(icon,individual/filename)
    atlas.paste(icon,(cell%10*64,cell//10*64))
    records.append(dict(id=id,sprite=cell,name=entry['name'],category=entry['category'],block=int(entry['block']),state=int(entry['state']),vertices=int(entry['vertices']),bounds=box,file=filename))
assert names==set(range(1,len(entries)+1)), 'Missing registry IDs'
save_png(atlas,assets/'item_icons_expansion.png');atomic_bytes(assets/'item_icons_expansion.rgba',struct.pack('<ii',*atlas.size)+atlas.tobytes())
# Contact sheets use the same cell positions for an immediate visual comparison.
before=Image.open(ROOT/'build/v2161-validation/baseline/icons.png').convert('RGBA') if (ROOT/'build/v2161-validation/baseline/icons.png').exists() else art.resize(atlas.size,Image.Resampling.NEAREST)
for title,sourceImage in [('before',before),('after',atlas)]:
    sheet=Image.new('RGB',(800,((len(records)+9)//10)*82),(43,44,49));draw=ImageDraw.Draw(sheet)
    for entry in records:
        id=entry['id'];cell=entry['sprite'];icon=sourceImage.crop((cell%10*64,cell//10*64,cell%10*64+64,cell//10*64+64));x=cell%10*80;y=cell//10*82
        sheet.paste(icon,(x+8,y),icon);draw.text((x+6,y+65),str(id),fill='white')
    sheet.save(output/(title+'-contact.png'))
outputs=['assets/item_icons_expansion.png','assets/item_icons_expansion.rgba','assets/inventory_art_source.rgba']+[f'assets/inventory_previews/{r["file"]}' for r in records]
data=dict(schema=1,tile_pixels=64,supersample_pixels=128,columns=10,rows=atlas.height//64,items=len(entries),
          camera=dict(azimuth_degrees=45,elevation_degrees=30,projection='orthographic',padding_factor=1.13),
          inputs={name:digest(ROOT/name) for name in inputs},outputs={name:digest(ROOT/name) for name in outputs},entries=records)
atomic_bytes(manifest,(json.dumps(data,indent=2)+'\n').encode())
(output/'authoring-timing.json').write_text(json.dumps(dict(seconds=time.perf_counter()-start,atlas_bytes=atlas.width*atlas.height*4,items=len(entries)),indent=2)+'\n')
verify()
print('Cached textured previews:',atlas.size,'; authoring seconds:',round(time.perf_counter()-start,2))
