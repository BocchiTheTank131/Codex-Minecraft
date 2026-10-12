"""Reproducible V13/V14 measurements from the existing opt-in terrain audit."""
import argparse
import csv
import json
from pathlib import Path
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
args = parser.parse_args()
root = args.directory
dtype = np.dtype([('biome', 'u1'), ('reserved', 'u1'), ('height', '<i2'),
    ('temperature', '<f4'), ('humidity', '<f4'), ('continentalness', '<f4'),
    ('river', '<f4'), ('macro_temperature', '<f4'), ('macro_humidity', '<f4')])

def runs(mask):
    """Axis transects excluding clipped runs; a width proxy, not coast normals."""
    widths = []
    for row in list(mask) + list(mask.T):
        edges = np.diff(np.r_[False, row, False].astype(int))
        start, end = np.flatnonzero(edges == 1), np.flatnonzero(edges == -1)
        widths.extend((end-start)[(start > 0) & (end < mask.shape[1])])
    return np.array(widths)

def width(mask, step):
    values = runs(mask)
    return float(values.mean()*step) if values.size else None

result = {'method': '512x512 map samples; mountain and coastline widths are axis-transect proxies', 'maps': []}
for seed in (20260917, 123456789, 42):
    for extent in (4096, 30720):
        previous = None
        for version in (13, 14):
            a = np.fromfile(root/f'map-{seed}-v{version}-{extent}.bin', dtype).reshape(512, 512)
            step = extent//512
            row = {'seed': seed, 'version': version, 'extent': extent,
                'height_percentiles_95_99_999': np.percentile(a['height'], [95,99,99.9]).tolist(),
                'height_delta_std': float(np.std(np.diff(a['height'].astype(float), axis=1))),
                'mountain_width_y180': width(a['height'] > 180, step),
                'coastal_width': width(np.isin(a['biome'], [10,16,20]), step),
                'beach_width': width(np.isin(a['biome'], [10,16]), step),
                'stony_width': width(a['biome'] == 20, step),
                'beach_samples': int(np.isin(a['biome'], [10,16]).sum()),
                'stony_samples': int((a['biome'] == 20).sum()),
                'temperature_neighbor_max': float(np.max(np.abs(np.diff(a['temperature'], axis=1))))}
            for distance in (500,1000):
                offset = round(distance/step)
                row[f'temperature_change_{distance}'] = float(np.mean(np.abs(a['temperature'][:,offset:] - a['temperature'][:,:-offset])))
            if previous is not None:
                for field in ('macro_temperature','macro_humidity','continentalness','river','humidity'):
                    if not np.array_equal(previous[field], a[field]):
                        raise AssertionError(f'Inherited field changed: {seed}/{extent}/{field}')
            previous = a
            result['maps'].append(row)

def records(file):
    return list(csv.DictReader((root/file).open()))

result['flowers'] = records('flowers.csv')
for seed in ('20260917','123456789','42'):
    rows = [r for r in result['flowers'] if r['seed'] == seed]
    ratio = int(rows[1]['flowers'])/int(rows[0]['flowers'])
    if not .45 <= ratio <= .55:
        raise AssertionError(f'Generated flower reduction outside tolerance: {seed}: {ratio}')

result['generation'] = []
for version in ('13','14'):
    rows = [r for r in records('generation.csv') if r['version'] == version]
    times = np.array([float(r['ms']) for r in rows])
    result['generation'].append({'version': version,'rows': len(rows),
        'mean_ms': float(times.mean()), 'median_ms': float(np.median(times))})
    rows = [r for r in records('connectivity.csv') if r['version'] == version]
    result.setdefault('connectivity', []).append({'version': version,
        'surface_detached_blocks': sum(int(r['surface_detached_blocks']) for r in rows),
        'exposed_faces': sum(int(r['exposed_faces']) for r in rows),
        'block_bytes': sum(int(r['bytes']) for r in rows)})
    rows = [r for r in records('short-routes.csv') if r['version'] == version and float(r['land_fraction']) > .8]
    result.setdefault('routes', []).extend({'version': version, 'length': length,
        'mean_biomes': float(np.mean([int(r['biome_types']) for r in rows if r['length'] == str(length)]))}
        for length in (500,1000,1500))

(root/'refinement-summary.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
