"""Retain inspectable v2.18.1 evidence and append measured tables to the report."""
import csv,hashlib,json,re,shutil,statistics,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1];B=R/'build/v2181-validation';D=R/'docs/engine-v2181';D.mkdir(exist_ok=True)
def rows(p):return list(csv.DictReader(p.open()))
def copy(p,name=None):
 dest=D/(name or p.relative_to(B));dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest)
def phase(name):
 root=B/name;assert (root/'events.jsonl').exists(),name
 events=[json.loads(x) for x in (root/'events.jsonl').read_text(encoding='utf-8').splitlines()];assert all(x['exit']==0 for x in events),name
 expected={'final-streaming':4,'final-scenes':6,'final-proxies-verified':8,'final-automation-performance':8,'final-traversal':18,'final-rd64-recovery':2}
 assert len(events)==expected[name],(name,len(events))
 for f in [root/'method.json',root/'events.jsonl',*root.glob('*/*.csv')]:copy(f)
 return events
for name,count in [('final-functional',12),('final-release',20),('final-generation',6)]:
 records=json.loads((B/name/'results.json').read_text(encoding='utf-8'));assert len(records)==count and all(x['exit']==0 for x in records),name
 copy(B/name/'results.json')
 for f in (B/name).glob('*/run.log'):copy(f)
 for f in (B/name).glob('*/*.csv'):copy(f)
for name in ['cpu-final','final-streaming','final-scenes','final-proxies-verified','final-automation-performance','final-traversal','final-rd64-recovery']:
 if name!='cpu-final':phase(name)
 for f in (B/name).glob('*/*.csv'):copy(f)
copy(B/'cpu-final/exact.json')
for f in [B/'baseline/fingerprints.csv',B/'baseline/method.json',B/'baseline-source/INSTRUMENTATION.json',B/'baseline/frozen-v23/stages.csv',B/'cache-phase/stages.csv']:
 if f.exists():copy(f)
for name in ['matched-streaming','retire-order-rd64','retire-phase-rd64']:
 for f in (B/name).glob('**/*.csv'):
  if '-profile' not in str(f):copy(f)
with zipfile.ZipFile(R/'tests/fixtures/v23-exact-v2180.zip') as z:
 m=json.loads(z.read('manifest.json'));assert len(m['files'])==495
 for variant in ['full','demo']:
  for name in m['files']:assert (B/'final-generation'/(variant+'-freeze')/name).read_bytes()==z.read(name)
 (D/'v23-exact.json').write_text(json.dumps({'files_per_variant':495,'reference_commit':m['commit'],'archive_sha256':hashlib.sha256((R/'tests/fixtures/v23-exact-v2180.zip').read_bytes()).hexdigest()},indent=2)+'\n', encoding='utf-8')
packages=[]
for line in (R/'dist/SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
 if not line.strip():continue
 sha,name=line.split(maxsplit=1);p=R/'dist'/name;assert hashlib.sha256(p.read_bytes()).hexdigest()==sha
 packages.append({'name':name,'bytes':p.stat().st_size,'sha256':sha})
(D/'packages.json').write_text(json.dumps(packages,indent=2)+'\n', encoding='utf-8')
assert json.loads((R/'assets/inventory_previews.json').read_text(encoding='utf-8'))['outputs']['assets/item_icons_expansion.rgba']=='4b910fecaa9f4f6f5f99336da40d7be82bc5bbd231cd3dc0264efd50ad6ac03d'
summary={'baseline_commit':'507fefdec29f5b2848d1344b520d94a34fe8df95','published':False,'installed':False,'historical_hashes':1326,'exact_v23_files_per_variant':495,'release_suites':20,'packages':packages}
text=['## Final regression evidence\n','All 20 packaged Release suites passed (11 full, nine demo). All 1,326 historical generator records match; both full/demo variants match all 495 frozen V23 block/loot/encounter files. Cold/warm complete exports, 1/2/4/8 parallel callers, ecosystem tests and 50 deterministic V23 Survival spawn seeds pass. Demo profiles contain no persistent world, player, inventory or navigation files.\n', 'Twelve functional groups pass, including the new immutable-snapshot and 24-teleport/save/death/reload stress cases. Original assertion totals are retained in the logs. The narrowed RD8–32 cleanup policy was rechecked with matched repeated RD32 streaming.\n']
# Frame percentile summaries are medians of independent phases, never pooled percentiles.
text+=['## Final RD32 streaming comparison\n','Two matched runs per variant, final policy. Medians of phase summaries; worst is the largest frame across runs. Geometry and queues are recorded in the linked raw data.\n','| Phase | Mean ms old/new | p95 | p99 | p99.9 | Worst | Peak retirement ms |\n|---|---:|---:|---:|---:|---:|---:|\n']
for label in ['warmup','flight8x','recovery','teleport']:
 values={v:[x for p in (B/'final-streaming').glob(v+'-*/streaming.csv') for x in rows(p) if x['phase']==label] for v in ['old','new']}
 cells=[]
 for key in ['mean_ms','p95_ms','p99_ms','p999_ms','worst_ms','peak_retire_ms']:
  fun=max if key in ['worst_ms','peak_retire_ms'] else statistics.median
  cells.append(' / '.join(f'{fun(float(x[key]) for x in values[v]):.3f}' for v in ['old','new']))
 text.append('| '+label+' | '+' | '.join(cells)+' |\n')
text+=['\n## Foreground scenes\n','One matched run per scene/RD, three 3-second phases after bounded warm-up. These short samples validate workloads and do not establish small FPS improvements. Frame metrics include presentation; GPU intervals exclude it. Rows below aggregate scene/phase summaries, so consult raw CSVs for geometry and scene-specific tails.\n','| RD | Mean ms old/new | p95 | p99 | p99.9 | Worst | GPU ms | Max RAM MiB | Terrain capacity MiB |\n|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n']
for rd in [8,16,32]:
 values={v:[x for p in (B/'final-scenes').glob(f'{v}-rd{rd}-*/frames.csv') for x in rows(p)] for v in ['old','new']}
 for v,data in values.items():assert len(data)==21,(rd,v,len(data));assert all(float(x['focused_percent'])>=99 and float(x['visible_percent'])>=99 for x in data),(rd,v,'focus')
 loc=[(B/'final-scenes'/f'{v}-rd{rd}-0/scenes.csv').read_bytes() for v in ['old','new']];assert loc[0]==loc[1],(rd,'scene mismatch')
 cells=[]
 for key in ['mean_ms','p95_ms','p99_ms','p999_ms','worst_ms','gpu_ms','ram_mb','terrain_capacity']:
  fun=max if key in ['worst_ms','ram_mb','terrain_capacity'] else statistics.median
  cells.append(' / '.join(f'{fun(float(x[key]) for x in values[v])/(1048576 if key=="terrain_capacity" else 1):.3f}' for v in ['old','new']))
 text.append(f'| {rd} | '+' | '.join(cells)+' |\n')
text+=['\n## Constrained logical-CPU proxies\n','Both variants use the same RD16/Low memory preset. Affinity is set before process initialization. These are logical CPU masks and a soft terrain-memory preset, not hard system-RAM limits or physical low-end/iGPU certification.\n','| CPUs | Generation/mesh limits old/new | Recovery mean ms old/new | Recovery seconds old/new |\n|---:|---|---:|---:|\n']
for cpu in [2,4]:
 data={v:[x for p in (B/'final-proxies-verified').glob(f'{v}-cpu{cpu}-*/streaming.csv') for x in rows(p) if x['phase']=='recovery'] for v in ['old','new']}
 for x in data['new']:assert x['gen_limit']=='1' and x['mesh_limit']=='1'
 limits=' / '.join(f"{data[v][0]['gen_limit']}+{data[v][0]['mesh_limit']}" for v in ['old','new'])
 cells=[' / '.join(f'{statistics.median(float(x[k]) for x in data[v]):.3f}' for v in ['old','new']) for k in ['mean_ms','recovery_seconds']]
 text.append(f'| {cpu} | {limits} | '+' | '.join(cells)+' |\n')
text+=['\n## RD64 stress and queue recovery\n','One extended matched pair, with a 45-second flight-recovery window and 20 seconds after teleport. This is a stress sample, not a repeated worst-frame guarantee. The full-scene retirement experiment still leaves indivisible driver/render stalls.\n','| Variant | Flight recovery seconds | End recovery gen/mesh/upload | Teleport mean / p99 / p99.9 / worst ms | Peak retirement ms | Teleport page allocations | End teleport gen/mesh/upload |\n|---|---:|---|---|---:|---:|---|\n']
for v in ['old','new']:
 data=rows(B/'final-rd64-recovery'/f'{v}-rd64-0/streaming.csv');rec=next(x for x in data if x['phase']=='recovery');tp=next(x for x in data if x['phase']=='teleport')
 values=' / '.join(f"{float(tp[k]):.3f}" for k in ['mean_ms','p99_ms','p999_ms','worst_ms'])
 queues=lambda x:'/'.join(x[k] for k in ['generation','mesh','upload_queue'])
 text.append(f"| {v} | {float(rec['recovery_seconds']):.3f} | {queues(rec)} | {values} | {float(tp['peak_retire_ms']):.3f} | {tp['page_allocations']} | {queues(tp)} |\n")
text+=['\nBoth flight workloads eventually settle in this window. Teleport still has outstanding work at 20 seconds; its later completion time is unmeasured here. Fewer new allocations and lower retirement peaks do not establish a reliable whole-frame worst-case improvement. Fixed-time streaming geometry differs while queues are active; resident/vertex/capacity fields are retained.\n']
text+=['\n## Actual traversal and automation\n','Matched RD8/16/32 Survival traversal includes rain, equipment, progressive maps, boating, active streaming, checkpoint/reload, and 100/1,000/10,000 automation components. Deliberate 10/15/20/30/60/120 FPS limits and a 500 ms injected stall are not counted as spontaneous engine stalls. Per-run CSVs retain work/frame/save timings, memory and simulation debt. Isolated automation timing includes preparation/reconciliation and device commits.\n']
assert len([x for x in (B/'final-traversal').glob('*/survival-rd*.csv')])==18
text+=['\n## Local packages\n','Both variants report version 2.18.1, embed the unchanged 361-icon atlas, and passed packaged executable suites. The installer was built without running it or changing the installed game.\n']
for p in packages:text.append(f"- `{p['name']}` — {p['bytes']:,} bytes; SHA256 `{p['sha256']}`.\n")
text+=['\nRaw [evidence index](engine-v2181/summary.json), [package digests](engine-v2181/packages.json), [Release suite results](engine-v2181/final-release/results.json), [functional results](engine-v2181/final-functional/results.json), and [V23 reference verification](engine-v2181/v23-exact.json).\n']
summary['notes']='See report for rejected general RD32 deletion policy, inclusive save timers, unverified hard memory/iGPU limits, and unresolved RD64 whole-frame/driver stalls.'
(D/'summary.json').write_text(json.dumps(summary,indent=2)+'\n', encoding='utf-8')
p=R/'docs/ENGINE_OPTIMIZATION_VALIDATION_v2.18.1.md'
# Retain the hand-written limits section while replacing only generated tables.
s=p.read_text(encoding='utf-8');start=s.index('<!-- final-evidence -->');end=s.index('## Limits and rejected approaches',start);p.write_text(s[:start]+'<!-- final-evidence -->\n\n'+''.join(text)+'\n'+s[end:], encoding='utf-8')
print('Evidence retained:',D)
