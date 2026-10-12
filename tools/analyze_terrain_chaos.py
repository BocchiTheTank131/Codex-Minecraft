"""V15 controlled-chaos measurements; widths are axis-transect proxies."""
import argparse, csv, json
from pathlib import Path
import numpy as np

p=argparse.ArgumentParser();p.add_argument('directory',type=Path);root=p.parse_args().directory
dtype=np.dtype([('biome','u1'),('reserved','u1'),('height','<i2'),('temperature','<f4'),
    ('humidity','<f4'),('continentalness','<f4'),('river','<f4'),('macro_temperature','<f4'),('macro_humidity','<f4')])

def width(mask,step):
    values=[]
    for row in list(mask)+list(mask.T):
        changes=np.diff(np.r_[False,row,False].astype(int));starts=np.flatnonzero(changes==1);ends=np.flatnonzero(changes==-1)
        values.extend((ends-starts)[(starts>0)&(ends<len(row))])
    return float(np.mean(values)*step) if values else None

result={'maps':[]}
for seed in (20260917,123456789,42):
    for extent in (4096,30720):
        before=None
        for version in (9,14,15):
            file=root/f'map-{seed}-v{version}-{extent}.bin'
            if not file.exists():continue
            a=np.fromfile(file,dtype).reshape(512,512);step=extent/512
            delta=np.abs(np.diff(a['height'].astype(float),axis=1))/step
            row={'seed':seed,'extent':extent,'version':version,'height_percentiles':np.percentile(a['height'],[50,95,99,100]).tolist(),
                'width_y140':width(a['height']>140,step),'width_y180':width(a['height']>180,step),
                'slope_percentiles':np.percentile(delta,[50,90,99]).tolist(),
                'mean_slope':float(delta.mean()),'biome_types':int(np.unique(a['biome']).size),
                'coast_width':width(np.isin(a['biome'],[10,16,20]),step)}
            if version==14:before=a
            if version==15:
                assert a['height'].max()<=255,'Terrain envelope exceeds natural cap'
                for field in ('macro_temperature','macro_humidity','continentalness','river'):
                    assert np.array_equal(before[field],a[field]),f'Inherited {field} changed'
            result['maps'].append(row)

def records(file):return list(csv.DictReader((root/file).open()))
result['routes']=[]
for version in ('14','15'):
    for length in ('500','1000','1500'):
        rows=[r for r in records('short-routes.csv') if r['version']==version and r['length']==length and float(r['land_fraction'])>.8]
        result['routes'].append({'version':version,'length':int(length),'rows':len(rows),**{
            field:float(np.mean([float(r[field]) for r in rows])) for field in
            ('biome_types','height_std','mean_slope','steep_fraction','ridge_count') if rows and field in rows[0]}})
result['generation']=[];result['connectivity']=[]
for version in ('14','15'):
    times=np.array([float(r['ms']) for r in records('generation.csv') if r['version']==version])
    result['generation'].append({'version':version,'mean_ms':float(times.mean()),'median_ms':float(np.median(times))})
    rows=[r for r in records('connectivity.csv') if r['version']==version]
    result['connectivity'].append({'version':version,**{field:sum(int(r[field]) for r in rows)
        for field in ('exposed_faces','surface_detached_blocks','surface_detached_components','undercut_columns','bytes')}})
result['flowers']=records('flowers.csv')
(root/'chaos-summary.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
