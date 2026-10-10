"""Label v9/v10 geography audit maps and quantify fragmentation (development only)."""
from pathlib import Path
from collections import deque
import argparse, csv, json, re
import numpy as np
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
enum = (root / 'include/Biome.h').read_text().split('enum class Biome')[1].split('};')[0]
names = [n.strip() for n in enum.split('{', 1)[1].split(',') if n.strip() != 'Count']
dtype = np.dtype([('biome', 'u1'), ('height', 'u1'), ('reserved', '<u2'),
                 ('temperature', '<f4'), ('humidity', '<f4'), ('continentalness', '<f4'),
                 ('river', '<f4'), ('macro_temperature', '<f4'), ('macro_humidity', '<f4')])
palette = np.array([
    (150,181,87),(54,117,62),(221,194,119),(121,127,112),(218,234,235),
    (129,196,103),(198,216,215),(136,146,151),(242,248,255),(91,166,98),
    (238,217,166),(37,90,162),(178,222,244),(164,205,211),(116,172,218),
    (145,195,226),(231,239,216),(56,120,116),(41,89,82),(65,120,167),
    (157,164,161),(32,79,50),(95,157,102),(145,153,114),(25,127,62),
    (87,163,76),(70,113,91),(181,180,76),(160,154,65),(204,174,99),
    (180,94,54),(135,111,60),(156,101,156),(39,158,185),(56,130,183)
], dtype=np.uint8)
font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 18)
small = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 14)

def components(grid):
    """One linear four-connected traversal; no external labeling dependency."""
    height, width = grid.shape
    seen = np.zeros(grid.shape, dtype=bool)
    result = []
    for z in range(height):
        for x in range(width):
            if seen[z, x]:
                continue
            value = grid[z, x]
            seen[z, x] = True
            queue = deque([(z, x)])
            count = 0
            while queue:
                zz, xx = queue.popleft()
                count += 1
                for nz, nx in ((zz-1,xx),(zz+1,xx),(zz,xx-1),(zz,xx+1)):
                    if 0 <= nz < height and 0 <= nx < width and not seen[nz,nx] and grid[nz,nx] == value:
                        seen[nz,nx] = True
                        queue.append((nz,nx))
            result.append((int(value), count))
    return result

def gradient_color(values):
    # Cold/dry blue, temperate/intermediate green, warm/wet amber/red.
    anchors = np.array([(39,76,158),(60,163,178),(90,183,107),(228,200,95),(207,77,51)])
    p = np.clip(values, 0, 1) * 4
    i = np.minimum(p.astype(int), 3)
    return (anchors[i] * (1-(p-i)[...,None]) + anchors[i+1] * (p-i)[...,None]).astype('uint8')

metrics = []
for binary in sorted(args.directory.glob('*.bin')):
    seed, version, extent = binary.stem.split('-')
    extent = int(extent)
    data = np.fromfile(binary, dtype=dtype).reshape(512,512)
    biome = data['biome']
    counts = components(biome)
    ocean_ids = [i for i,n in enumerate(names) if 'Ocean' in n]
    oceans = np.isin(biome, ocean_ids).astype('u1')
    ocean_components = [n for v,n in components(oceans) if v == 1]
    climate = np.digitize(data['macro_temperature'], [.17,.34,.60,.80])
    climate_components = components(climate)
    row = dict(seed=int(seed), version=int(version[1:]), extent=extent,
               biome_components=len(counts), singleton_biome_components=sum(n == 1 for _,n in counts),
               area_weighted_biome_component_km2=sum(n*n for _,n in counts)/biome.size*(extent/512/1000)**2,
               ocean_percent=float(oceans.mean()*100), ocean_components=len(ocean_components),
               largest_ocean_fraction=max(ocean_components, default=0)/max(1,int(oceans.sum())),
               climate_components=len(climate_components),
               mean_temperature_neighbor_delta=float((np.abs(np.diff(data['temperature'],axis=0)).mean()+np.abs(np.diff(data['temperature'],axis=1)).mean())/2),
               mean_humidity_neighbor_delta=float((np.abs(np.diff(data['humidity'],axis=0)).mean()+np.abs(np.diff(data['humidity'],axis=1)).mean())/2))
    for n in ('IceSpikes','MushroomFields'):
        row[n+'_percent'] = float((biome == names.index(n)).mean()*100)
    metrics.append(row)
    fields = {'biomes':palette[biome], 'temperature':gradient_color(data['temperature']),
              'humidity':gradient_color(data['humidity']),
              'continentalness':gradient_color((data['continentalness']+.5)/1.0),
              'terrain':np.repeat(data['height'][...,None],3,axis=2)}
    # Blue ocean/river overlay preserves readable elevation without implying GI.
    fields['terrain'][np.isin(biome,ocean_ids+ [names.index('River'),names.index('FrozenRiver')])] = (52,106,161)
    for name,pixels in fields.items():
        Image.fromarray(pixels).save(args.directory/(binary.stem+'-'+name+'.png'))
    print(binary.stem, 'biome regions:',len(counts),'ocean regions:',len(ocean_components),flush=True)

with (args.directory/'metrics.csv').open('w',newline='') as file:
    writer=csv.DictWriter(file,fieldnames=list(metrics[0]));writer.writeheader();writer.writerows(metrics)
(args.directory/'metrics.json').write_text(json.dumps(metrics,indent=2))
for seed in sorted({str(r['seed']) for r in metrics}):
    for extent in (30720,4096):
        panel=Image.new('RGB',(2700,1350),(24,28,34));draw=ImageDraw.Draw(panel)
        draw.text((20,12),f'Seed {seed} | {extent:,} x {extent:,} blocks | 512 x 512 samples | North at top',font=font,fill='white')
        for row,version in enumerate((9,10)):
            for col,name in enumerate(('biomes','temperature','humidity','continentalness','terrain')):
                x=20+col*536;y=78+row*558
                draw.text((x,y-24),f'V{version} {name.upper()}',font=font,fill='white')
                panel.paste(Image.open(args.directory/f'{seed}-v{version}-{extent}-{name}.png'),(x,y))
        for i,name in enumerate(names):
            x=20+(i%7)*380;y=1199+(i//7)*26
            draw.rectangle((x,y,x+14,y+14),fill=tuple(palette[i]));draw.text((x+22,y-2),re.sub(r'(?<!^)(?=[A-Z])',' ',name),font=small,fill='white')
        draw.text((20,1330),'T/H: 0 blue -> 0.5 green -> 1 red | Continentalness: -0.5 blue -> +0.5 red | Elevation: black low -> white high; water blue',font=small,fill='white')
        panel.save(args.directory/f'{seed}-{extent}-comparison.png')
