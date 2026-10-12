"""V18 geographical validation. Sample area is world area, not spawn probability.

Four-connected areas use bounded overview windows: edge-touching components
are truncated, not estimates of complete planetary continent sizes.
"""
import argparse,csv,json,hashlib
from pathlib import Path
from collections import deque
import numpy as np

p=argparse.ArgumentParser();p.add_argument('directory',type=Path);args=p.parse_args();root=args.directory
dtype=np.dtype([('biome','u1'),('reserved','u1'),('height','<i2'),('temperature','<f4'),('humidity','<f4'),('continent','<f4'),('river','<f4'),('macro_temperature','<f4'),('macro_humidity','<f4')])
names=['Plains','Forest','Desert','Mountains','SnowyPlains','Meadow','SnowySlopes','StonyPeaks','SnowyPeaks','BirchForest','Beach','Ocean','IceSpikes','SnowyTaiga','FrozenOcean','FrozenRiver','SnowyBeach','Taiga','OldGrowthTaiga','ColdOcean','StonyShore','DarkForest','FlowerForest','WindsweptHills','Jungle','SparseJungle','Swamp','Savanna','SavannaPlateau','DesertHills','Badlands','WoodedBadlands','MushroomFields','WarmOcean','River']
def regions(mask):
 seen=np.zeros(mask.shape,bool);counts=[];edge=[];h,w=mask.shape
 for z,x in zip(*np.nonzero(mask)):
  if seen[z,x]:continue
  seen[z,x]=True;q=deque([(z,x)]);n=0;boundary=False
  while q:
   z,x=q.popleft();n+=1;boundary|=z in (0,h-1) or x in (0,w-1)
   for a,b in ((z-1,x),(z+1,x),(z,x-1),(z,x+1)):
    if 0<=a<h and 0<=b<w and mask[a,b] and not seen[a,b]:seen[a,b]=True;q.append((a,b))
  counts.append(n);edge.append(boundary)
 return np.array(counts),np.array(edge)
result={'maps':[],'rivers':{},'generation':{},'vegetation':{},'climate_hashes':{}}
for path in sorted(root.glob('map-*-v18-*.bin')):
 _,seed,_,extent=path.stem.split('-');extent=int(extent)
 current=np.fromfile(path,dtype=dtype)
 old=np.fromfile(root/f'map-{seed}-v17-{extent}.bin',dtype=dtype)
 for field in ('temperature','humidity','macro_temperature','macro_humidity'):
  assert current[field].tobytes()==old[field].tobytes(),(path,field)
 result['climate_hashes'][f'{seed}-{extent}']={field:hashlib.sha256(current[field].tobytes()).hexdigest() for field in ('temperature','humidity','macro_temperature','macro_humidity')}
 for v in (17,18):
  a=np.fromfile(root/f'map-{seed}-v{v}-{extent}.bin',dtype=dtype).reshape(512,512)
  ocean=np.isin(a['biome'],(11,14,19,33));river=np.isin(a['biome'],(15,34));land=~ocean
  row={'seed':int(seed),'version':v,'extent':extent,'land_percent':float(land.mean()*100),'river_percent':float(river.mean()*100),'biomes':{name:float((a['biome']==i).mean()*100) for i,name in enumerate(names)}}
  for name,mask in (('land',land),('ocean',ocean),('dry_land',land&~river)):
   counts,edge=regions(mask);total=int(counts.sum());scale=(extent/512/1000)**2
   row[name]={'components':len(counts),'weighted_area_km2':float((counts.astype(float)**2).sum()/max(1,total)*scale),'largest_fraction':float(counts.max(initial=0)/max(1,total)),'edge_area_fraction':float(counts[edge].sum()/max(1,total)),'small_area_fraction':float(counts[counts<4].sum()/max(1,total))}
  row['coast_boundary_percent']=float((np.mean(ocean[1:]!=ocean[:-1])+np.mean(ocean[:,1:]!=ocean[:,:-1]))*50)
  deltas=np.concatenate([np.abs(np.diff(a['height'].astype(float),axis=0))[land[1:]&land[:-1]],np.abs(np.diff(a['height'].astype(float),axis=1))[land[:,1:]&land[:,:-1]]])
  row['terrain_mean_adjacent_delta']=float(deltas.mean());row['terrain_p95_adjacent_delta']=float(np.percentile(deltas,95));result['maps'].append(row)
for path in root.glob('rivers-*.csv'):
 rows=list(csv.DictReader(path.open()));width=np.array([float(r['actual_water_width']) for r in rows]);incoming=np.array([int(r['incoming']) for r in rows])
 assert all(float(r['from_y'])>=float(r['to_y'])>=64 for r in rows)
 result['rivers'][path.stem]={ 'reaches':len(rows),'length_km':sum(float(r['length']) for r in rows)/1000,'junctions':sum(incoming>=2).item(),'width_median':float(np.median(width)),'width_p95':float(np.percentile(width,95)),'dry_reaches':int((width==0).sum()),'small_2_7_fraction':float(((width>=2)&(width<=7)).mean()),'main_8_24_fraction':float(((width>=8)&(width<=24)).mean()),'max_level':max(float(r['from_y']) for r in rows)}
for v in (17,18):
 rows=[r for r in csv.DictReader((root/'generation.csv').open()) if int(r['version'])==v]
 result['generation'][str(v)]={'mean_ms':float(np.mean([float(r['ms']) for r in rows])),'median_ms':float(np.median([float(r['ms']) for r in rows])),'mean_allocated_bytes':float(np.mean([int(r['allocated_bytes']) for r in rows]))}
 rows=[r for r in csv.DictReader((root/'vegetation-refinement.csv').open()) if int(r['version'])==v]
 result['vegetation'][str(v)]={field:sum(int(r[field]) for r in rows) for field in ('trees','bamboo','other_plants','columns')}
assert result['vegetation']['17']==result['vegetation']['18'],'V18 altered fixed-habitat vegetation'
(root/'geography-balance-summary.json').write_text(json.dumps(result,indent=2))
for v in (17,18):
 rows=[r for r in result['maps'] if r['version']==v and r['extent']==30720]
 print('V',v,'land%',np.mean([r['land_percent'] for r in rows]),'land km2',np.mean([r['land']['weighted_area_km2'] for r in rows]),'dry land km2',np.mean([r['dry_land']['weighted_area_km2'] for r in rows]),'ocean km2',np.mean([r['ocean']['weighted_area_km2'] for r in rows]),'Mushroom%',np.mean([r['biomes']['MushroomFields'] for r in rows]))
print(json.dumps({k:v for k,v in result.items() if k!='maps'},indent=2))
