"""Render the native V22/V23 audit fields; no synthetic/generated scenery."""
import argparse
import ast
import csv
import json
from collections import deque
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

p = argparse.ArgumentParser()
p.add_argument('directory', type=Path)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
text = (root/'tools/build_density_previews.py').read_text()
palette = np.array(ast.literal_eval(text.split('palette = np.array(')[1].split(', dtype=')[0]), dtype='u1')
names = [s.strip() for s in (root/'include/Biome.h').read_text().split('enum class Biome')[1].split('{', 1)[1].split('};', 1)[0].split(',') if s.strip() != 'Count']
dtype = np.dtype([('biome', 'u1'), ('flags', 'u1'), ('height', '<i2'),
                  ('temperature', '<f4'), ('humidity', '<f4'), ('continent', '<f4'),
                  ('river', '<f4'), ('erosion', '<f4'), ('ravine', '<f4')])
font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 18)
small = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 13)
oceans = [names.index(n) for n in ('Ocean', 'ColdOcean', 'FrozenOcean', 'WarmOcean')]

def regions(grid, include=None):
    seen = np.zeros(grid.shape, dtype=bool)
    result = []
    for z in range(grid.shape[0]):
        for x in range(grid.shape[1]):
            if seen[z, x] or (include is not None and not include[z, x]):
                continue
            q = deque([(z, x)])
            seen[z, x] = True
            count = 0
            value = grid[z, x]
            while q:
                zz, xx = q.popleft()
                count += 1
                for nz, nx in ((zz-1, xx), (zz+1, xx), (zz, xx-1), (zz, xx+1)):
                    if 0 <= nz < grid.shape[0] and 0 <= nx < grid.shape[1] and not seen[nz, nx] and grid[nz, nx] == value:
                        seen[nz, nx] = True
                        q.append((nz, nx))
            result.append(count)
    return result

images, metrics = {}, []
for path in sorted(args.directory.glob('map-*.bin')):
    _, seed, version, extent = path.stem.split('-')
    seed, version, extent = int(seed), int(version[1:]), int(extent)
    data = np.fromfile(path, dtype=dtype)
    if data.size != 512*512:
        raise ValueError(f'Incorrect map length: {path}')
    data = data.reshape(512, 512)
    ocean = np.isin(data['biome'], oceans)
    labels = regions(data['biome'], ~ocean)
    heights = data['height'].astype(float)
    shade = np.clip(1 + np.gradient(heights)[0]*.018 - np.gradient(heights)[1]*.018, .65, 1.15)
    biome = np.clip(palette[data['biome']].astype(float)*shade[..., None], 0, 255).astype('u1')
    elev = np.stack((np.clip(70+heights*.70, 0, 255), np.clip(105+heights*.54, 0, 255), np.clip(62+heights*.83, 0, 255)), axis=-1).astype('u1')
    elev[ocean] = (28, 74, 122)
    climate = np.stack((data['temperature']*255, data['humidity']*225, (1-data['temperature'])*230), axis=-1).astype('u1')
    waterways = np.repeat(np.clip(50+heights*.65, 0, 220).astype('u1')[..., None], 3, axis=2)
    waterways[ocean] = (18, 45, 84)
    waterways[data['river'] > .05] = (42, 189, 242)
    for field, pixels in [('biomes', biome), ('elevation', elev), ('climate', climate), ('waterways', waterways)]:
        im = Image.fromarray(pixels)
        im.save(args.directory/f'{path.stem}-{field}.png')
        images[seed, version, extent, field] = im
    land_h = heights[~ocean]
    metrics.append(dict(seed=seed, version=version, extent=extent,
        land_percent=float((~ocean).mean()*100), biomes=int(np.unique(data['biome']).size),
        biome_regions=len(labels), median_biome_region_blocks=float(np.median(labels)*(extent/512)**2),
        weighted_biome_region_blocks=float(sum(n*n for n in labels)/max(1, sum(labels))*(extent/512)**2),
        tiny_biome_area_percent=float(sum(n for n in labels if n <= 2)/max(1, sum(labels))*100),
        height_p50=float(np.percentile(land_h, 50)), height_p95=float(np.percentile(land_h, 95)),
        height_p99=float(np.percentile(land_h, 99)), maximum_height=float(heights.max()),
        mountain_percent=float(((heights > 160) & ~ocean).mean()*100),
        river_percent=float((data['river'] > .05).mean()*100),
        mushroom_percent=float((data['biome'] == names.index('MushroomFields')).mean()*100),
        ice_spikes_percent=float((data['biome'] == names.index('IceSpikes')).mean()*100)))
    print(seed, version, extent, 'biomes', metrics[-1]['biomes'], flush=True)

for seed, extent in sorted({(r['seed'], r['extent']) for r in metrics}):
    canvas = Image.new('RGB', (2180, 1315), (24, 28, 34))
    draw = ImageDraw.Draw(canvas)
    draw.text((16, 10), f'Seed {seed} | {extent:,} x {extent:,} blocks | 512 x 512 samples | North at top', font=font, fill='white')
    for row, version in enumerate((22, 23)):
        for col, field in enumerate(('biomes', 'elevation', 'climate', 'waterways')):
            x, y = 16+544*col, 65+550*row
            draw.text((x, y-24), f'V{version} {field.upper()}', font=font, fill='white')
            canvas.paste(images[seed, version, extent, field], (x, y))
    for i, name in enumerate(names):
        x, y = 16+(i % 7)*308, 1140+(i//7)*25
        draw.rectangle((x, y, x+12, y+12), fill=tuple(palette[i]))
        draw.text((x+18, y-2), name, font=small, fill='white')
    draw.text((16, 1280), 'Climate: red=warm, blue=cold, green=wet. Narrow waterways may fall between samples. Region areas are map-clipped.', font=small, fill='white')
    canvas.save(args.directory/f'{seed}-{extent}-comparison.png')

with (args.directory/'metrics.csv').open('w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(metrics[0]))
    w.writeheader(); w.writerows(metrics)
(args.directory/'metrics.json').write_text(json.dumps(metrics, indent=2))

# Actual deterministic plans, not indicators inferred from the biome image.
structures = list(csv.DictReader((args.directory/'structures.csv').open()))
caves = list(csv.DictReader((args.directory/'entrances.csv').open()))
for seed in sorted({int(r['seed']) for r in structures}):
    extent = 15360
    canvas = Image.new('RGB', (1060, 570), (24, 28, 34)); draw = ImageDraw.Draw(canvas)
    for i, version in enumerate((22, 23)):
        x0 = 12+i*524
        canvas.paste(images[seed, version, extent, 'biomes'], (x0, 32))
        draw.text((x0, 8), f'V{version} landmarks + entrances', font=font, fill='white')
        for rows, color, radius in ((structures, '#ffc548', 2), (caves, '#ff66dd', 1)):
            for r in rows:
                if int(r['seed']) != seed or int(r['version']) != version:
                    continue
                x, z = float(r['x']), float(r['z'])
                px, pz = int(x/extent*512+256), int(z/extent*512+256)
                if 0 <= px < 512 and 0 <= pz < 512:
                    draw.ellipse((x0+px-radius, 32+pz-radius, x0+px+radius, 32+pz+radius), fill=color)
    draw.text((12, 550), 'Gold: accepted small structures (8,192-block survey); pink: accepted cave mouths (3,072-block survey).', font=small, fill='white')
    canvas.save(args.directory/f'{seed}-distribution.png')
