"""V16 audit: temperature coherence at several scales, not just region count.

Run after --aesthetics-audit=... --generator=16. Maps are 512x512 samples;
component areas/diameters are sampled estimates, not exact world geometry.
"""
import argparse
import csv
import json
from collections import deque
from pathlib import Path

import numpy as np


DTYPE = np.dtype([('biome', 'u1'), ('reserved', 'u1'), ('height', '<i2'),
                  ('temperature', '<f4'), ('humidity', '<f4'),
                  ('continentalness', '<f4'), ('river', '<f4'),
                  ('macro_temperature', '<f4'), ('macro_humidity', '<f4')])


def components(grid):
    seen = np.zeros(grid.shape, dtype=bool)
    sizes = []
    for z, x in np.ndindex(grid.shape):
        if seen[z, x]:
            continue
        value = grid[z, x]
        queue = deque([(z, x)])
        seen[z, x] = True
        count = 0
        while queue:
            zz, xx = queue.popleft()
            count += 1
            for nz, nx in ((zz-1, xx), (zz+1, xx), (zz, xx-1), (zz, xx+1)):
                if (0 <= nz < grid.shape[0] and 0 <= nx < grid.shape[1]
                        and not seen[nz, nx] and grid[nz, nx] == value):
                    seen[nz, nx] = True
                    queue.append((nz, nx))
        sizes.append(count)
    return np.array(sizes)


def edges(field):
    return np.concatenate([np.abs(np.diff(field.astype(float), axis=a)).ravel()
                           for a in (0, 1)])


def analyze(directory):
    result = {'maps': [], 'routes': [], 'generation': [], 'unchanged_fields': []}
    for seed in (20260917, 123456789, 42):
        for extent in (4096, 30720):
            step = extent / 512
            before = None
            for version in (9, 15, 16):
                data = np.fromfile(directory / f'map-{seed}-v{version}-{extent}.bin',
                                   DTYPE).reshape(512, 512)
                t = data['temperature']
                category = np.digitize(t, [.17, .34, .60, .80])
                sizes = components(category)
                neighbor = edges(t)
                categories = edges(category)
                row = {'seed': seed, 'version': version, 'extent': extent,
                       'category_components': len(sizes),
                       'singleton_area_percent': float(np.count_nonzero(sizes == 1) / t.size * 100),
                       'area_weighted_region_km2': float((sizes.astype(float)**2).sum() / t.size * (step/1000)**2),
                       'median_region_equivalent_width_blocks': float(np.sqrt(np.median(sizes))*step),
                       'boundary_fraction': float((categories > 0).mean()),
                       'extreme_neighbor_edges': int((categories == 4).sum()),
                       'neighbor_delta_mean': float(neighbor.mean()),
                       'neighbor_delta_p99': float(np.percentile(neighbor, 99)),
                       'neighbor_delta_max': float(neighbor.max()),
                       'temperature_std': float(t.std()),
                       'category_area_percent': (np.bincount(category.ravel(), minlength=5)/t.size*100).tolist(),
                       'macro_correlation': (float(np.corrcoef(t.ravel(), data['macro_temperature'].ravel())[0, 1])
                                             if data['macro_temperature'].std() > 0 else None),
                       'biome_types': int(np.unique(data['biome']).size), 'scale_deltas': {}}
                for distance in (64, 256, 512, 1024, 2048):
                    n = max(1, round(distance/step))
                    diff = np.concatenate([np.abs(t[n:]-t[:-n]).ravel(),
                                           np.abs(t[:, n:]-t[:, :-n]).ravel()])
                    row['scale_deltas'][str(distance)] = float(diff.mean())
                assert row['extreme_neighbor_edges'] == 0, 'Freezing/hot adjacency'
                if version == 15:
                    before = data
                if version == 16:
                    for field in ('height', 'humidity', 'continentalness', 'river',
                                  'macro_temperature', 'macro_humidity'):
                        assert before[field].tobytes() == data[field].tobytes(), field
                    result['unchanged_fields'].append({'seed': seed, 'extent': extent,
                                                       'samples': t.size, 'bit_identical': True})
                result['maps'].append(row)

    for version in (15, 16):
        for length in (500, 1000, 1500):
            def rows(name):
                with (directory/name).open() as file:
                    return [r for r in csv.DictReader(file)
                            if int(r['version']) == version and int(r['length']) == length
                            and float(r['land_fraction']) > .8]
            climate = rows('climate-routes.csv')
            biomes = rows('short-routes.csv')
            result['routes'].append({'version': version, 'length': length, 'routes': len(climate),
                **{f: float(np.mean([float(r[f]) for r in climate])) for f in
                   ('temperature_std', 'temperature_range', 'climate_transitions', 'max_adjacent_delta')},
                'biome_types': float(np.mean([float(r['biome_types']) for r in biomes]))})
        with (directory/'generation.csv').open() as file:
            rows = [r for r in csv.DictReader(file) if int(r['version']) == version]
        for seed in (20260917, 123456789, 42, None):
            times = np.array([float(r['ms']) for r in rows if seed is None or int(r['seed']) == seed])
            result['generation'].append({'version': version, 'seed': seed, 'samples': len(times),
                                         'mean_ms': float(times.mean()), 'median_ms': float(np.median(times))})
    (directory/'climate-summary.json').write_text(json.dumps(result, indent=2))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    result = analyze(parser.parse_args().directory)
    print(json.dumps(result, indent=2))
