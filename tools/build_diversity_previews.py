"""Render V22's native audit maps and area/connectivity measurements."""
import argparse, ast, csv, json
from collections import deque
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--seed',type=int);args=p.parse_args()
root=Path(__file__).resolve().parents[1]
text=(root/'tools/build_density_previews.py').read_text()
palette=np.array(ast.literal_eval(text.split('palette = np.array(')[1].split(', dtype=')[0]),dtype='u1')
names=[s.strip() for s in (root/'include/Biome.h').read_text().split('enum class Biome')[1].split('{',1)[1].split('};',1)[0].split(',') if s.strip()!='Count']
dtype=np.dtype([('biome','u1'),('reserved','u1'),('height','<i2'),('temperature','<f4'),('humidity','<f4'),('continent','<f4'),('river','<f4'),('macro_temperature','<f4'),('macro_humidity','<f4')])
font=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',18);small=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',13)
ocean_ids=[names.index(b) for b in ('Ocean','FrozenOcean','ColdOcean','WarmOcean')]

def regions(grid):
    h,w=grid.shape;seen=np.zeros_like(grid,dtype=bool);sizes=[]
    for z in range(h):
        for x in range(w):
            if seen[z,x] or not grid[z,x]:continue
            q=deque([(z,x)]);seen[z,x]=True;count=0
            while q:
                zz,xx=q.popleft();count+=1
                for nz,nx in ((zz-1,xx),(zz+1,xx),(zz,xx-1),(zz,xx+1)):
                    if 0<=nz<h and 0<=nx<w and grid[nz,nx] and not seen[nz,nx]:
                        seen[nz,nx]=True;q.append((nz,nx))
            sizes.append(count)
    return sizes

metrics=[];images={}
for file in sorted(args.directory.glob('map-*.bin')):
    _,seed,version,extent=file.stem.split('-');seed=int(seed);version=int(version[1:]);extent=int(extent)
    if args.seed is not None and seed!=args.seed:continue
    data=np.fromfile(file,dtype=dtype)
    if len(data)!=512*512:continue
    data=data.reshape(512,512);ocean=np.isin(data['biome'],ocean_ids);land=~ocean
    land_sizes=regions(land);ocean_sizes=regions(ocean);scale=(extent/512/1000)**2
    boundary=np.count_nonzero(land[1:]!=land[:-1])+np.count_nonzero(land[:,1:]!=land[:,:-1])
    row=dict(seed=seed,version=version,extent=extent,land_percent=float(land.mean()*100),
             land_regions=len(land_sizes),ocean_regions=len(ocean_sizes),
             weighted_land_km2=sum(n*n for n in land_sizes)/max(1,sum(land_sizes))*scale,
             weighted_ocean_km2=sum(n*n for n in ocean_sizes)/max(1,sum(ocean_sizes))*scale,
             largest_ocean_fraction=max(ocean_sizes,default=0)/max(1,sum(ocean_sizes)),
             coastline_km=boundary*extent/512/1000,
             river_percent=float((data['river']>.1).mean()*100),
             MushroomFields_percent=float((data['biome']==names.index('MushroomFields')).mean()*100),
             IceSpikes_percent=float((data['biome']==names.index('IceSpikes')).mean()*100))
    metrics.append(row)
    height=np.repeat(np.clip((data['height'].astype(float)+64)*255/320,0,255).astype('u1')[...,None],3,axis=2)
    height[ocean]=(42,91,151)
    continent=np.clip((data['continent']+.5)*1.,0,1)
    cont=np.stack((continent*255,(1-np.abs(continent-.5)*2)*210,(1-continent)*255),axis=-1).astype('u1')
    river=np.repeat(np.clip((data['height'].astype(float)+64)*.65,0,180).astype('u1')[...,None],3,axis=2)
    river[ocean]=(22,47,85);river[data['river']>.1]=(50,185,240)
    for field,pixels in [('biomes',palette[data['biome']]),('continentalness',cont),('rivers',river),('elevation',height)]:
        image=Image.fromarray(pixels);image.save(args.directory/(file.stem+'-'+field+'.png'))
        images[seed,version,extent,field]=image
    print(seed,version,extent,'land',round(row['land_percent'],2),'weighted km2',round(row['weighted_land_km2'],2),flush=True)

for seed,extent in sorted({(r['seed'],r['extent']) for r in metrics}):
    versions=sorted(r['version'] for r in metrics if r['seed']==seed and r['extent']==extent)
    ylegend=60+len(versions)*550
    canvas=Image.new('RGB',(2180,ylegend+174),(24,28,34));draw=ImageDraw.Draw(canvas)
    draw.text((16,12),f'Seed {seed} | {extent:,} x {extent:,} blocks | 512 x 512 samples | North at top',font=font,fill='white')
    for row,version in enumerate(versions):
        for col,field in enumerate(('biomes','continentalness','rivers','elevation')):
            x=16+col*544;y=60+row*550
            draw.text((x,y-23),f'V{version} {field.upper()}',font=font,fill='white');canvas.paste(images[seed,version,extent,field],(x,y))
    for i,name in enumerate(names):
        x=16+(i%7)*308;y=ylegend+(i//7)*25
        draw.rectangle((x,y,x+12,y+12),fill=tuple(palette[i]));draw.text((x+18,y-2),name,font=small,fill='white')
    draw.text((16,ylegend+135),'River channels blue; sampled maps may miss narrow streams. Region areas are clipped to the map boundary.',font=small,fill='white')
    canvas.save(args.directory/f'{seed}-{extent}-comparison.png')
with (args.directory/'region-metrics.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(metrics[0]));writer.writeheader();writer.writerows(metrics)
(args.directory/'region-metrics.json').write_text(json.dumps(metrics,indent=2))
