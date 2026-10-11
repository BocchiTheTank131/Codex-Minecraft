"""Measure bounded, exterior-facing roof cavities in exported terrain volumes.

This is an offline diagnostic, not a generation cleanup. Requires numpy, like
build_density_previews.py. It distinguishes shallow visible recesses from the
unbounded cave-roof count, but does not classify arches or prove connectivity.
"""
import argparse
import csv
import re
from pathlib import Path

import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
enum = (repo / 'include/Block.h').read_text().split('enum class Block')[1]
enum = re.sub(r'//[^\n]*', '', enum)
names = [part.strip().split('=')[0].strip()
         for part in enum.split('{', 1)[1].split('};')[0].split(',')]
terrain = {
    'Stone', 'Dirt', 'Grass', 'Sand', 'Sandstone', 'SnowBlock', 'Gravel',
    'Clay', 'Mud', 'Podzol', 'Mycelium', 'CoarseDirt', 'Granite', 'Diorite',
    'Andesite', 'Deepslate', 'Bedrock', 'Terracotta', 'TerracottaOrange',
    'TerracottaBrown', 'TerracottaRed', 'TerracottaYellow', 'TerracottaWhite',
}
terrain_ids = [i for i, name in enumerate(names)
               if name in terrain or 'Ore' in name]
rows = list(csv.DictReader((args.directory / 'connectivity.csv').open()))
output = []
for row in rows:
    path = args.directory / f'terrain-{row["seed"]}-v{row["version"]}-{row["scene"]}.bin'
    blocks = np.fromfile(path, dtype=np.uint8).reshape(384, 96, 96)
    solid = np.isin(blocks, terrain_ids)
    top = np.where(solid, np.arange(384)[:, None, None], 0).max(axis=0)
    air = blocks == 0
    # A four-block air gap and three-block roof remove tiny chips/noise. Limit
    # to within 40 blocks of the terrain top and above sea level. The corridor
    # must reach a lower exposed column in <=12 blocks along a cardinal axis.
    gap = air[:-3] & air[1:-2] & air[2:-1] & air[3:]
    roof = solid[4:-2] & solid[5:-1] & solid[6:]
    candidate = np.zeros_like(air)
    y = np.arange(378)[:, None, None]
    candidate[:378] = gap[:378] & roof & (y >= 128) & (top-y <= 40)
    exterior = np.zeros_like(air)
    for dz, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        clear = candidate.copy()
        for step in range(1, 13):
            zs, xs = slice(12, 84), slice(12, 84)
            nz = slice(12+dz*step, 84+dz*step)
            nx = slice(12+dx*step, 84+dx*step)
            clear[:381, zs, xs] &= gap[:, nz, nx]
            exposed = np.arange(384)[:, None, None] > top[nz, nx]
            exterior[:, zs, xs] |= clear[:, zs, xs] & exposed
    columns = exterior[:, 12:84, 12:84].any(axis=0)
    land = top[12:84, 12:84] >= 128
    output.append({
        **{k: row[k] for k in ('seed', 'version', 'scene')},
        'land_columns': int(land.sum()),
        'exterior_roof_columns': int(columns.sum()),
        'percent_land_columns': 100*int(columns.sum())/max(1, int(land.sum())),
    })
    print(row['seed'], row['version'], row['scene'], int(columns.sum()))
with (args.directory / 'exterior-roofs.csv').open('w', newline='') as file:
    writer = csv.DictWriter(file, fieldnames=list(output[0]))
    writer.writeheader()
    writer.writerows(output)
