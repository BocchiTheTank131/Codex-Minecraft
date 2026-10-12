"""Analyze V17 audit data; run after benchmarks, never alongside timed fixtures."""
import argparse
import csv
import json
from collections import deque
from pathlib import Path
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
args = parser.parse_args()
root = args.directory
dtype = np.dtype([('biome', 'u1'), ('reserved', 'u1'), ('height', '<i2'),
                  ('temperature', '<f4'), ('humidity', '<f4'), ('continentalness', '<f4'),
                  ('river', '<f4'), ('macro_temperature', '<f4'), ('macro_humidity', '<f4')])

def regions(mask):
    seen = np.zeros(mask.shape, dtype=bool)
    sizes = []
    for z, x in zip(*np.nonzero(mask)):
        if seen[z, x]:
            continue
        seen[z, x] = True
        queue = deque([(z, x)])
        count = 0
        while queue:
            zz, xx = queue.popleft()
            count += 1
            for nz, nx in ((zz-1, xx), (zz+1, xx), (zz, xx-1), (zz, xx+1)):
                if 0 <= nz < 512 and 0 <= nx < 512 and mask[nz, nx] and not seen[nz, nx]:
                    seen[nz, nx] = True
                    queue.append((nz, nx))
        sizes.append(count)
    return sizes

result = {'maps': [], 'vegetation': {}, 'routes': {}, 'generation': {}}
for path in sorted(root.glob('map-*-v17-*.bin')):
    _, seed, _, extent = path.stem.split('-')
    current = np.fromfile(path, dtype=dtype)
    previous = np.fromfile(root / f'map-{seed}-v16-{extent}.bin', dtype=dtype)
    for field in ('temperature', 'humidity', 'macro_temperature', 'macro_humidity'):
        assert np.array_equal(current[field], previous[field]), (path, field)
    for v in (9, 16, 17):
        data = np.fromfile(root / f'map-{seed}-v{v}-{extent}.bin', dtype=dtype).reshape(512, 512)
        water = np.isin(data['biome'], (11, 14, 15, 19, 33, 34))
        land = ~water
        row = {'seed': int(seed), 'version': v, 'extent': int(extent),
               'land_percent': float(land.mean()*100), 'biome_types': int(np.unique(data['biome']).size)}
        for name, mask in (('land', land), ('water', water)):
            counts = regions(mask)
            total = sum(counts)
            row[name+'_regions'] = len(counts)
            row[name+'_regions_ge4_samples'] = sum(n >= 4 for n in counts)
            row[name+'_largest_fraction'] = max(counts, default=0)/max(1, total)
            row[name+'_weighted_area_km2'] = sum(n*n for n in counts)/max(1, total)*(int(extent)/512/1000)**2
        pairs = []
        for axis in (0, 1):
            valid = land[:-1, :] & land[1:, :] if axis == 0 else land[:, :-1] & land[:, 1:]
            pairs.extend(np.abs(np.diff(data['height'].astype(float), axis=axis))[valid])
        row['land_mean_neighbor_height_delta'] = float(np.mean(pairs)) if pairs else 0
        row['land_p95_neighbor_height_delta'] = float(np.percentile(pairs, 95)) if pairs else 0
        row['coast_boundary_percent'] = float((np.mean(water[1:] != water[:-1]) + np.mean(water[:,1:] != water[:,:-1]))*50)
        result['maps'].append(row)

vegetation = list(csv.DictReader((root/'vegetation-refinement.csv').open()))
for v in (16, 17):
    rows = [r for r in vegetation if int(r['version']) == v]
    result['vegetation'][str(v)] = {k: sum(int(r[k]) for r in rows) for k in ('trees', 'bamboo', 'other_plants', 'columns')}
assert result['vegetation']['16']['other_plants'] == result['vegetation']['17']['other_plants']
for k in ('trees', 'bamboo'):
    result['vegetation'][k+'_change_percent'] = (result['vegetation']['17'][k]/result['vegetation']['16'][k]-1)*100

routes = list(csv.DictReader((root/'short-routes.csv').open()))
for length in (500, 1000, 1500):
    rows = [r for r in routes if int(r['length']) == length]
    by_key = {(r['seed'], r['route'], int(r['version'])): r for r in rows}
    keys = [(r['seed'], r['route']) for r in rows if int(r['version']) == 16 and
            float(r['land_fraction']) >= .8 and float(by_key[(r['seed'], r['route'], 17)]['land_fraction']) >= .8]
    result['routes'][str(length)] = {'paired_land_routes': len(keys)}
    for v in (16, 17):
        paired = [by_key[seed, route, v] for seed, route in keys]
        result['routes'][str(length)][str(v)] = {k: float(np.mean([float(r[k]) for r in paired]))
            for k in ('biome_types', 'height_std', 'mean_slope', 'ridge_count', 'steep_fraction')}
generation = list(csv.DictReader((root/'generation.csv').open()))
for v in (16, 17):
    rows = [r for r in generation if int(r['version']) == v]
    result['generation'][str(v)] = {'rows': len(rows), 'mean_ms': float(np.mean([float(r['ms']) for r in rows])),
        'median_ms': float(np.median([float(r['ms']) for r in rows])),
        'mean_allocated_bytes': float(np.mean([int(r['allocated_bytes']) for r in rows]))}
(root/'geography-summary.json').write_text(json.dumps(result, indent=2))
print(json.dumps({k: v for k, v in result.items() if k != 'maps'}, indent=2))
