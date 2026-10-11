"""Label v10/v11 density audit maps and quantify fragmentation (development only)."""
from pathlib import Path
from collections import deque
import argparse, csv, json, re
import numpy as np
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
parser.add_argument('--versions', nargs=2, type=int, default=[10,11])
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
enum = (root / 'include/Biome.h').read_text().split('enum class Biome')[1].split('};')[0]
names = [n.strip() for n in enum.split('{', 1)[1].split(',') if n.strip() != 'Count']
dtype = np.dtype([('biome', 'u1'), ('reserved', 'u1'), ('height', '<i2'),
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
for binary in sorted(args.directory.glob('map-*.bin')):
    _, seed, version, extent = binary.stem.split('-')
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
              'terrain':np.repeat(np.clip((data['height'].astype('f4')+64)*255/384,0,255).astype('u1')[...,None],3,axis=2)}
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
        for row,version in enumerate(args.versions):
            for col,name in enumerate(('biomes','temperature','humidity','continentalness','terrain')):
                x=20+col*536;y=78+row*558
                draw.text((x,y-24),f'V{version} {name.upper()}',font=font,fill='white')
                panel.paste(Image.open(args.directory/f'map-{seed}-v{version}-{extent}-{name}.png'),(x,y))
        for i,name in enumerate(names):
            x=20+(i%7)*380;y=1199+(i//7)*26
            draw.rectangle((x,y,x+14,y+14),fill=tuple(palette[i]));draw.text((x+22,y-2),re.sub(r'(?<!^)(?=[A-Z])',' ',name),font=small,fill='white')
        draw.text((20,1330),'T/H: 0 blue -> 0.5 green -> 1 red | Continentalness: -0.5 blue -> +0.5 red | Elevation: black low -> white high; water blue',font=small,fill='white')
        panel.save(args.directory/f'{seed}-{extent}-comparison.png')

# Cross-sections use the generated block IDs, with air transparent black and
# separate colors for aquifers/lava/ores. Source data is at native block scale.
block_names=(root/'include/Block.h').read_text().split('enum class Block')[1].split('};')[0]
block_names=re.sub(r'//[^\n]*','',block_names)
block_names=[v.strip().split('=')[0].strip() for v in block_names.split('{',1)[1].split(',') if v.strip()]
colors=np.zeros((256,3),dtype='u1');colors[:]=(101,102,105)
for i,n in enumerate(block_names):
    if n=='Air':colors[i]=(10,14,21)
    elif 'Water' in n:colors[i]=(43,101,164)
    elif n=='Lava':colors[i]=(244,101,24)
    elif n=='Bedrock':colors[i]=(34,35,38)
    elif 'CoalOre' in n:colors[i]=(49,51,56)
    elif 'IronOre' in n:colors[i]=(177,141,113)
    elif 'CopperOre' in n:colors[i]=(203,119,71)
    elif 'GoldOre' in n:colors[i]=(232,185,57)
    elif 'DiamondOre' in n:colors[i]=(55,213,207)
    elif n=='Deepslate':colors[i]=(58,61,70)
    elif 'Snow' in n or 'Ice' in n:colors[i]=(209,232,243)
    elif 'Sand' in n:colors[i]=(209,185,121)
    elif 'Dirt' in n:colors[i]=(126,94,61)
    elif 'Grass' in n or 'Leaves' in n:colors[i]=(89,136,67)
for binary in sorted(args.directory.glob('section-*.bin')):
    data=np.fromfile(binary,dtype='u1').reshape(384,512)
    image=Image.fromarray(colors[data]).resize((1024,768),Image.Resampling.NEAREST)
    panel=Image.new('RGB',(1100,860),(24,28,34));draw=ImageDraw.Draw(panel)
    draw.text((65,12),binary.stem+' | Y=-64..319 | sea level 64',font=font,fill='white')
    panel.paste(image,(65,60))
    for y in (319,256,192,128,64,0,-64):
        sy=60+min(767,(319-y)*2);draw.text((5,sy-8),str(y),font=small,fill='white')
        draw.line((60,sy,65,sy),fill='white')
    draw.text((65,835),'Black: air | blue: water | orange: lava | cyan: diamond | yellow: gold',font=small,fill='white')
    panel.save(args.directory/(binary.stem+'.png'))

for binary in sorted(args.directory.glob('volume-*.bin')):
    volume=np.fromfile(binary,dtype='u1').reshape(384,64,64)
    # Remove a diagonal wedge and cap the view at Y=96 to expose cave interiors.
    volume=volume[:161].copy()
    zz,xx=np.indices((64,64));volume[:,xx+zz<50]=0
    canvas=Image.new('RGB',(850,710),(20,25,32));draw=ImageDraw.Draw(canvas)
    for y in range(volume.shape[0]):
        for z in range(63,-1,-1):
            for x in range(63,-1,-1):
                block=int(volume[y,z,x])
                if not block:continue
                sx=425+(x-z)*3;sy=610-(x+z)*1.5-y*2
                c=colors[block]
                if x==0 or volume[y,z,x-1]==0:
                    draw.polygon([(sx,sy),(sx+3,sy+1.5),(sx+3,sy+3.5),(sx,sy+2)],fill=tuple((c*.62).astype(int)))
                if z==0 or volume[y,z-1,x]==0:
                    draw.polygon([(sx,sy),(sx-3,sy+1.5),(sx-3,sy+3.5),(sx,sy+2)],fill=tuple((c*.82).astype(int)))
                if y==volume.shape[0]-1 or volume[y+1,z,x]==0:
                    draw.polygon([(sx,sy),(sx+3,sy+1.5),(sx,sy+3),(sx-3,sy+1.5)],fill=tuple(c))
    draw.text((20,16),binary.stem+' | generated 64 x 64 volume | Y=-64..96',font=font,fill='white')
    draw.text((20,45),'Diagonal cut exposes dry caves, aquifers and deep ore layers',font=small,fill='white')
    canvas.save(args.directory/(binary.stem+'-cutaway.png'))

# Standard scientific plot, built from generated ore counts per sampled stone.
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
rows=list(csv.DictReader((args.directory/'ores.csv').open()))
fig,axes=plt.subplots(1,5,figsize=(15,7),sharey=True)
for ax,ore in zip(axes,('coal','iron','copper','gold','diamond')):
    for seed in sorted({r['seed'] for r in rows}):
        sample=[r for r in rows if r['seed']==seed]
        y=np.array([int(r['y']) for r in sample]);count=np.array([int(r[ore]) for r in sample]);stone=np.array([int(r['solid']) for r in sample])
        # Four-level bins reduce noisy single-slice counts; retain raw CSV.
        ratio=np.divide(count.reshape(-1,4).sum(1),stone.reshape(-1,4).sum(1),out=np.zeros(96),where=stone.reshape(-1,4).sum(1)>0)*100
        ax.plot(ratio,y.reshape(-1,4).mean(1),label=seed,linewidth=1)
    ax.set_title(ore.title());ax.axhline(64,color='steelblue',linestyle=':',linewidth=.8);ax.axhline(0,color='gray',linestyle=':',linewidth=.8)
    ax.set_xlabel('% of sampled solid blocks');ax.grid(alpha=.2);ax.set_ylim(-64,320)
axes[0].set_ylabel('World Y');axes[-1].legend(title='Seed',fontsize=8);fig.suptitle(f'V{args.versions[1]} actual ore distribution | 48 regional chunks per seed | 4-block depth bins')
fig.tight_layout();fig.savefig(args.directory/'ore-distribution.png',dpi=140);plt.close(fig)
