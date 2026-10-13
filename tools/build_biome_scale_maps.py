"""Render biome-only scale comparisons from the game's 512-square map audits."""
import argparse
import ast
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
parser.add_argument('--version', type=int, default=18)
parser.add_argument('--seed', type=int, default=123456789)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / 'tools/build_density_previews.py').read_text()
palette = np.array(ast.literal_eval(source.split('palette = np.array(')[1].split(', dtype=')[0]), dtype=np.uint8)
size, margin, gap = 1320, 20, 30
panel = Image.new('RGB', (2 * size + 2 * margin + gap, size + 2 * margin + 48), 'black')
draw = ImageDraw.Draw(panel)
font = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 48)
small = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 24)
prefix = f'V{args.version}-biomes-{args.seed}'
metadata = dict(generator=args.version, seed=args.seed, center=[0, 0], north='negative Z', samples=[512, 512], maps=[])
for i, extent in enumerate((15360, 46080)):
    binary = args.directory / f'map-{args.seed}-v{args.version}-{extent}.bin'
    raw = np.fromfile(binary, dtype=np.uint8)
    if raw.size != 512 * 512 * 28:
        raise ValueError(f'Unexpected map payload size: {binary}')
    biomes = raw.reshape(512, 512, 28)[:, :, 0]
    if biomes.max() >= len(palette):
        raise ValueError('Unknown biome ID')
    image = Image.fromarray(palette[biomes])
    image.save(args.directory / f'{prefix}-{extent}-samples.png')
    image = image.resize((size, size), Image.Resampling.NEAREST)
    image.save(args.directory / f'{prefix}-{extent}.png')
    x, y = margin + i * (size + gap), margin
    panel.paste(image, (x, y))
    label = f'{extent:,} × {extent:,} blocks'
    width = draw.textbbox((0, 0), label, font=font)[2]
    draw.rectangle((x + size - width - 28, y, x + size, y + 68), fill='black')
    draw.text((x + size - width - 14, y + 6), label, font=font, fill='white')
    draw.rectangle((x, y, x + size - 1, y + size - 1), outline='white', width=2)
    if i == 1:
        start, top = x + size // 3, y + size // 3
        draw.rectangle((start, top, start + size // 3, top + size // 3), outline='white', width=3)
        draw.line((margin + size, y + size // 2, start, y + size // 2), fill='white', width=3)
    metadata['maps'].append(dict(extent_blocks=extent, sample_spacing_blocks=extent // 512, source=binary.name))
draw.text((margin, size + margin + 10), f'Generator V{args.version} • Seed {args.seed} • North at top • Centred on X=0, Z=0 • 512 × 512 samples per map', font=small, fill='white')
output = args.directory / f'{prefix}-two-scales.png'
panel.save(output)
(args.directory / f'{prefix}-metadata.json').write_text(json.dumps(metadata, indent=2))
print(output.resolve())
