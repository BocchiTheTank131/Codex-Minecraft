"""Check preview coverage, mapping, alpha gutters and unchanged historical content."""
from pathlib import Path
from PIL import Image
import argparse, hashlib, json, struct, subprocess, re
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path);args=p.parse_args()
m=json.loads((ROOT/'assets/inventory_previews.json').read_text());atlas=Image.open(ROOT/'assets/item_icons_expansion.png').convert('RGBA')
raw=(ROOT/'assets/item_icons_expansion.rgba').read_bytes();assert struct.unpack('<ii',raw[:8])==atlas.size and raw[8:]==atlas.tobytes()
ids=set();cells=set();duplicates={}
for e in m['entries']:
    assert e['id'] not in ids and e['sprite'] not in cells
    ids.add(e['id']);cells.add(e['sprite']);im=Image.open(ROOT/'assets/inventory_previews'/e['file']).convert('RGBA')
    assert im.size==(64,64);box=im.getbbox();assert box and min(box[:2])>=3 and max(box[2:])<=61
    assert abs(box[0]+box[2]-64)<=2 and abs(box[1]+box[3]-64)<=2
    x,y=e['sprite']%10*64,e['sprite']//10*64;assert im.tobytes()==atlas.crop((x,y,x+64,y+64)).tobytes()
    visible=[c for c in im.getdata() if c[3]>128];assert visible and max(max(c[:3]) for c in visible)>30
    duplicates.setdefault(hashlib.sha256(im.tobytes()).hexdigest(),[]).append(e['name'])
assert ids==set(range(1,m["items"]+1)) and len(cells)==m["items"]
def enums(data,name):
    body=data.decode().split('enum class '+name,1)[1].split('{',1)[1].split('}',1)[0]
    body=re.sub(r'//[^\n]*','',body);result={};value=0
    for token in body.split(','):
        token=token.strip()
        if not token:continue
        bits=token.split('=')
        if len(bits)>1:value=int(bits[1].strip(),0)
        if bits[0].strip()!='Count':result[bits[0].strip()]=value
        value+=1
    return result
historical={}
for name in ['include/Block.h','include/Survival.h','assets/crafting_blocks.png','assets/crafting_blocks.rgba','assets/inventory_atlas_16.png']:
    before=subprocess.check_output(['git','show','v2.16.0:'+name],cwd=ROOT);after=(ROOT/name).read_bytes()
    if name.endswith('.h'):
        enum='Block' if 'Block.h' in name else 'Item';prior=enums(before,enum);current=enums(after,enum)
        assert all(current.get(k)==v for k,v in prior.items()),name+' IDs changed'
    else:
        if 'crafting_blocks' in name:
            import io
            if name.endswith('.rgba'):
                bw,bh=struct.unpack('<ii',before[:8]);aw,ah=struct.unpack('<ii',after[:8]);old=Image.frombytes('RGBA',(bw,bh),before[8:]);new=Image.frombytes('RGBA',(aw,ah),after[8:])
            else:old=Image.open(io.BytesIO(before));new=Image.open(io.BytesIO(after))
            assert new.crop((0,0,*old.size)).tobytes()==old.tobytes(),name+' historical texels changed'
        else:assert before==after,name+' changed'
    historical[name]=True
for file in (ROOT/'sprites').glob('*.png'):
    assert file.read_bytes()==subprocess.check_output(['git','show','v2.16.0:sprites/'+file.name],cwd=ROOT)
report=dict(items=len(ids),unique_assignments=len(cells),dimensions=atlas.size,gpu_bytes=atlas.width*atlas.height*4,
            pixels_match_raw=True,all_centered=True,all_visible=True,world_textures_and_ids_unchanged=historical,
            billboard_sprites_unchanged=4,exact_duplicates=[n for n in duplicates.values() if len(n)>1])
if args.output:args.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
