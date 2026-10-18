"""Compare every historical ID and occupied atlas pixel against a release ref."""
import argparse
import io
import json
from pathlib import Path
import re
import subprocess
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('--baseline', default='v2.15.1')
parser.add_argument('--output', type=Path)
parser.add_argument('--visual-dir', type=Path, help='Optional production --building-visual fixture output')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]

def old(path):
    return subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], cwd=root)

def ids(source, name):
    body = source.split('enum class ' + name, 1)[1].split('{', 1)[1].split('}', 1)[0]
    body = re.sub(r'//[^\n]*|/\*.*?\*/', '', body, flags=re.S)
    result, value = {}, 0
    for token in body.split(','):
        token = token.strip()
        if not token:
            continue
        fields = token.split('=')
        if len(fields) == 2:
            value = int(fields[1].strip(), 0)
        if fields[0].strip() != 'Count':
            result[fields[0].strip()] = value
        value += 1
    return result

proof = {'baseline': args.baseline}
for path, name in [('include/Block.h', 'Block'), ('include/Survival.h', 'Item')]:
    before = ids(old(path).decode(), name)
    after = ids((root / path).read_text(), name)
    assert all(after.get(key) == value for key, value in before.items()), name + ' IDs changed'
    proof[name] = {'historical_ids_unchanged': len(before), 'current_types': len(after)}
for name in ['crafting_blocks.png', 'inventory_atlas_16.png']:
    before = Image.open(io.BytesIO(old('assets/' + name))).convert('RGBA')
    after = Image.open(root / 'assets' / name).convert('RGBA')
    if name.startswith('crafting'):
        assert before.tobytes() == after.crop((0, 0, *before.size)).tobytes()
        proof[name] = {'old_pixels_identical': True}
    else:
        for index in range(206):
            x, y = index % 10 * 16, index // 10 * 16
            box = x, y, x + 16, y + 16
            assert before.crop(box).tobytes() == after.crop(box).tobytes(), str(index)
        proof[name] = {'occupied_cells_identical': 206}
atlas=Image.open(root/'assets/inventory_atlas_16.png').convert('RGBA')
entries=re.findall(r'BUILDING\((\w+),', (root/'include/BuildingContent.inc').read_text())
for index,name in enumerate(entries,206):
    icon=Image.open(root/'assets/inventory'/(re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()+'.png')).convert('RGBA')
    bounds=icon.getbbox()
    assert bounds and min(bounds[:2])>=1 and max(bounds[2:])<=15, name+' clipped'
    assert abs(bounds[0]+bounds[2]-16)<=1 and abs(bounds[1]+bounds[3]-16)<=1, name+' off center'
    x,y=index%10*16,index//10*16
    assert icon.tobytes()==atlas.crop((x,y,x+16,y+16)).tobytes(), name+' atlas mismatch'
proof['new_icons']={'centered_and_unclipped':len(entries),'individual_atlas_matches':len(entries)}
if args.visual_dir:
    views={}
    # Fixed fixture cameras frame a red flower over its green stalk. This catches
    # reversed atlas V coordinates as well as missing back/edge-facing planes.
    for name in ['pot-front','pot-side','pot-back']:
        im=Image.open(args.visual_dir/(name+'.bmp')).convert('RGB');w,h=im.size
        assert (w,h)==(1280,720), 'Unexpected visual fixture resolution'
        red=[];green=[]
        for y in range(h//2-35,h//2+70):
            for x in range(w//2-8,w//2+8):
                r,g,b=im.getpixel((x,y))
                if r>180 and g<85 and b<85: red.append(y)
                if g>65 and g>r*1.25 and g>b*1.25: green.append(y)
        assert len(red)>200 and len(green)>200, name+' missing flower/stalk'
        assert sum(red)/len(red)+15<sum(green)/len(green), name+' upside down'
        views[name]={'flower_pixels':len(red),'stalk_pixels':len(green),'upright':True}
    proof['production_pot_views']=views
text = json.dumps(proof, indent=2)
if args.output:
    args.output.write_text(text + '\n')
print(text)
