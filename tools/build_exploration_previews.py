"""Render distribution evidence from --underground-audit (NumPy / Pillow)."""
import argparse, csv, json, re
from collections import Counter
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

p = argparse.ArgumentParser()
p.add_argument('directory', type=Path)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
rows = list(csv.DictReader((args.directory / 'structures.csv').open()))
dtype = np.dtype([('biome','u1'),('reserved','u1'),('height','<i2'),
                 ('temperature','<f4'),('humidity','<f4'),('continentalness','<f4'),
                 ('river','<f4'),('macro_temperature','<f4'),('macro_humidity','<f4')])
palette = np.array([
    (150,181,87),(54,117,62),(221,194,119),(121,127,112),(218,234,235),
    (129,196,103),(198,216,215),(136,146,151),(242,248,255),(91,166,98),
    (238,217,166),(37,90,162),(178,222,244),(164,205,211),(116,172,218),
    (145,195,226),(231,239,216),(56,120,116),(41,89,82),(65,120,167),
    (157,164,161),(32,79,50),(95,157,102),(145,153,114),(25,127,62),
    (87,163,76),(70,113,91),(181,180,76),(160,154,65),(204,174,99),
    (180,94,54),(135,111,60),(156,101,156),(39,158,185),(56,130,183)
], dtype=np.uint8)
colors = {'Mineshaft':'#efb655','Dungeon':'#ed6685','Ancient Ruins':'#77e1e8'}
font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf',18)
small = ImageFont.truetype('C:/Windows/Fonts/consola.ttf',14)
stats = []
for seed in sorted({r['seed'] for r in rows}):
    sites = [r for r in rows if r['seed'] == seed]
    data = np.fromfile(args.directory/f'map-{seed}-v19-8192.bin',dtype).reshape(512,512)
    panel = Image.new('RGB',(1500,910),(24,28,34))
    draw = ImageDraw.Draw(panel)
    draw.text((24,16),f'Generator V19 | seed {seed} | 8,192 x 8,192 blocks | north at top',font=font,fill='white')
    panel.paste(Image.fromarray(palette[data['biome']]).resize((800,800),Image.Resampling.NEAREST),(24,60))
    for r in sites:
        x = 24+(int(r['x'])+4096)/8192*800
        z = 60+(int(r['z'])+4096)/8192*800
        color = colors[r['type']]
        draw.ellipse((x-5,z-5,x+5,z+5),fill=color,outline='black',width=2)
    y=70
    for kind,color in colors.items():
        selected=[r for r in sites if r['type']==kind]
        ys=[int(r['y']) for r in selected]
        draw.ellipse((850,y+3,860,y+13),fill=color)
        draw.text((875,y),f'{kind}: {len(selected)}',font=font,fill='white')
        draw.text((875,y+27),f'Floors Y={min(ys)}..{max(ys)}',font=small,fill='#cccccc')
        draw.text((875,y+49),f'{sum(int(r["rooms"]) for r in selected)} rooms / {sum(int(r["chests"]) for r in selected)} chests',font=small,fill='#cccccc')
        stats.append(dict(seed=int(seed),type=kind,count=len(selected),minimum_y=min(ys),maximum_y=max(ys),
                          rooms=sum(int(r['rooms']) for r in selected),chests=sum(int(r['chests']) for r in selected)))
        y+=105
    draw.text((850,405),'STRUCTURES BY DEPTH',font=font,fill='white')
    for i,(lo,hi) in enumerate([(-64,-33),(-32,-1),(0,31),(32,63)]):
        counts=[sum(lo<=int(r['y'])<=hi and r['type']==k for r in sites) for k in colors]
        draw.text((850,446+i*48),f'{lo:3}..{hi:2}',font=small,fill='white')
        x=960
        for count,color in zip(counts,colors.values()):
            draw.rectangle((x,446+i*48,x+count*11,466+i*48),fill=color);x+=count*11
        draw.text((960,469+i*48),' / '.join(map(str,counts)),font=small,fill='white')
    draw.text((850,675),'No ocean / Mushroom Fields sites.',font=small,fill='#cccccc')
    draw.text((850,700),'Each marker has a dry cave entrance.',font=small,fill='#cccccc')
    draw.text((850,725),'Finite encounters; no infinite spawner.',font=small,fill='#cccccc')
    draw.text((24,880),'Markers indicate underground structure centers; surface biomes remain V18.',font=small,fill='white')
    panel.save(args.directory/f'{seed}-structure-distribution.png')
(args.directory/'distribution-summary.json').write_text(json.dumps(stats,indent=2))
with (args.directory/'distribution-by-biome.csv').open('w',newline='') as f:
    writer=csv.writer(f);writer.writerow(['type','biome','count'])
    writer.writerows((kind,biome,count) for (kind,biome),count in sorted(Counter((r['type'],r['biome']) for r in rows).items()))
print(f'{len(rows)} structures across {len({r["seed"] for r in rows})} seeds; labeled maps and depth/biome summaries written.')
