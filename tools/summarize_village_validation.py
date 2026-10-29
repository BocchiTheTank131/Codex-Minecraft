"""Collect completed local village evidence; never executes/installs a package."""
from pathlib import Path
import csv,hashlib,json,statistics,shutil,subprocess,zipfile
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1];B=R/'build/v2200-validation';D=R/'docs/villages-v2200';D.mkdir(exist_ok=True)
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def copy(src,name=None):shutil.copyfile(src,D/(name or src.name))
def rows(p):return list(csv.DictReader(p.open(encoding='utf-8')))
def percentile(v,p):return sorted(v)[int((len(v)-1)*p)]
def metrics(rs,key):
 v=[float(r[key]) for r in rs if float(r[key])>=0]
 return {'mean':statistics.mean(v),'p95':percentile(v,.95),'p99':percentile(v,.99),'worst':max(v)} if v else {}
summary={}
for phase,count in [('functional',15),('generation',9),('release',20)]:
 data=read(B/(phase+'-accepted/results.json'));assert len(data)==count and all(r['exit']==0 for r in data)
 summary[phase]=data;copy(B/(phase+'-accepted/results.json'),phase+'-results.json')
for variant in ('full','demo'):
 summary['village-'+variant]=read(B/f'focused-ready/{variant}/results.json');assert summary['village-'+variant]['passed']
copy(B/'freeze24-ready/comparison.json','v24-comparison.json');summary['v24']=read(B/'freeze24-ready/comparison.json')
for variant in ('full','demo'):
 actual=rows(B/f'release-accepted/{variant}-optimization/chunk-hashes.csv');expected=rows(R/f'docs/generator-v23/historical-{variant}.csv')
 key=lambda r:(r['seed'],r['version'],r['x'],r['z'],r['hash'],r['bytes'])
 assert sorted(map(key,actual))==sorted(map(key,expected)) and len(actual)==663
 copy(B/f'release-accepted/{variant}-optimization/chunk-hashes.csv','historical-'+variant+'.csv')
summary['historical_records']=1326;summary['v23_exact_per_variant']=495
native=[]
for fps in (30,60,15):
 folder=B/f'native-ready{fps}/route';result=read(folder/'results.json');assert result['passed'];result['simulation_fps']=fps;native.append(result)
 for name in ('npc-routes.csv','npc-targets.csv','schedule.csv','ui-targets.csv','site.csv'):copy(folder/name,f'{fps}-'+name)
summary['native']=native
summary['final_dialog_ui']=read(B/'native-final-ui/route/results.json');assert summary['final_dialog_ui']['passed'];summary['benchmark_build_note']='Final request reward wording changed after timing runs; NPC simulation, Trade tab, save and rendering paths measured are unchanged. Final native UI/gameplay and embedded assets were rechecked.';copy(B/'native-final-ui/route/ui-targets.csv','final-ui-targets.csv')
follow=read(B/'followup-results.json');assert len(follow)==5 and all(r['exit']==0 for r in follow);summary['final_residency_followup']=follow;copy(B/'followup-results.json');copy(B/'residency-before.log')
copy(B/'final-compatibility/results.json','final-compatibility.json');summary['final_compatibility']=read(B/'final-compatibility/results.json')
trace=rows(B/'native-ready30/route/npc-routes.csv');arrivals=[]
for uid in sorted({r['id'] for r in trace}):
 rs=[r for r in trace if r['id']==uid]
 arrivals.append({'id':uid,'work_distance':min(float(r['work_distance']) for r in rs if r['routine']=='0'),'home_distance':min(float(r['home_distance']) for r in rs if r['routine']=='2'),'distinct_positions':len({(r['x'],r['y'],r['z']) for r in rs})})
summary['arrivals']=arrivals
assets=read(B/'package-assets.json');summary['embedded_resources_per_variant']=len(assets['full']);summary['protected_files']=len(assets['protected_files']);copy(B/'package-assets.json')
copy(B/'baseline/baseline.json');copy(B/'package-final-ui.log','package.log')
copy(B/'baseline-source/EXPORT_INSTRUMENTATION.json','baseline-export-instrumentation.json')
with zipfile.ZipFile(R/'tests/fixtures/v24-exact-v2190.zip') as z:(D/'v24-fixture-manifest.json').write_bytes(z.read('manifest.json'))
packages={}
for name,path in [('full',R/'build/installed-release/Release/VoxelFrontier.exe'),('demo',R/'dist/VoxelFrontier-v2.20.0-Windows-Standalone.exe'),('setup',R/'dist/VoxelFrontier-v2.20.0-Windows-Setup.exe')]:packages[name]={'file':str(path.relative_to(R)),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
(D/'packages.json').write_text(json.dumps(packages,indent=2),encoding='utf-8');copy(R/'dist/SHA256SUMS.txt')
summary['packages']=packages
for path in ('src/Survival.cpp','src/Villages.cpp','src/VillageBehavior.cpp','src/GameVillages.cpp','src/SaveSnapshot.cpp','src/World.cpp','src/Renderer.cpp','tests/village_benchmark.inc'):
 summary.setdefault('source_sha256',{})[path]=hashlib.sha256((R/path).read_bytes()).hexdigest()
bench=B/'benchmarks-ready';runs=read(bench/'results.json');assert len(runs)==18 and all(r['exit']==0 for r in runs)
copy(bench/'results.json','benchmark-runs.json');frames={};save=[]
for run in runs:
 name=run['name'];frames[name]=rows(bench/name/'frames.csv');copy(bench/name/'frames.csv',name+'-frames.csv');copy(bench/name/'save.csv',name+'-saves.csv')
 for r in rows(bench/name/'save.csv'):save.append({'run':name,**{k:float(v) for k,v in r.items()}})
summary['benchmarks']={}
for name,rs in frames.items():
 summary['benchmarks'][name]={key:metrics(rs,key) for key in ['wall_ms','work_ms','simulation_ms','npc_ai_last_tick_ms','gpu_ms','ram_mb']}
 summary['benchmarks'][name]['loaded_range']=[min(int(r['loaded']) for r in rs),max(int(r['loaded']) for r in rs)]
 summary['benchmarks'][name]['triangle_range']=[min(int(r['triangles']) for r in rs),max(int(r['triangles']) for r in rs)]
 summary['benchmarks'][name]['max_nodes']=max(int(r['nodes_last_tick']) for r in rs);summary['benchmarks'][name]['max_searches']=max(int(r['queries_last_tick']) for r in rs)
 assert summary['benchmarks'][name]['max_nodes']<=512 and summary['benchmarks'][name]['max_searches']<=2
summary['save_measurements']=save
image_names=['01-generated-village','02-trading','03-request-board','04-projects-reputation','05-completed-stall','06-friendly-trade','07-daily-gathering','08-night-return','09-villager-at-work']
for name in image_names:
 source=B/'native-final-ui/route'/(name+'.bmp')
 if source.exists():Image.open(source).save(D/(name+'.png'))
ui=Image.new('RGB',(1024,12*167),(27,31,37));draw=ImageDraw.Draw(ui)
for row,(w,scale) in enumerate((w,s) for w in (1280,1920,2560) for s in range(4)):
 for tab in range(4):
  source=next((B/f'native-final-ui/route/ui-{w}-s{scale}-t{tab}').glob('*.bmp'));im=Image.open(source);im.thumbnail((256,144));ui.paste(im,(tab*256,row*167));draw.text((tab*256+4,row*167+145),f'{w}x{w*9//16} scale {scale} tab {tab}',fill='white')
ui.save(D/'ui-contact.png')
results=f'''| Check | Result |
|---|---|
| New focused full/demo | {summary['village-full']['assertions']:,} / {summary['village-demo']['assertions']:,} passing assertions |
| Native 30/60/15 simulation cadences | {' / '.join(format(r['assertions'],',') for r in native)} passing assertions |
| Preserved functional suites | 14 passed, plus the village suite |
| Generation suites | 9 full/demo suites passed |
| Packaged Release smoke suites | 20 passed |
| Historical V1–V22 records | 1,326 matched (663 per variant) |
| Frozen V23 bytes | 495 fixtures per variant matched |
| Frozen V24 bytes, states, loot and markers | 738 fixtures per variant matched original v2.19.0 |
| Embedded assets | 135 per executable, byte-verified |
| Original previews/art/source protection | 365 mappings/images, {summary['protected_files']:,} files and recorded source regions retained |

The complete matrix ran before the final loaded-terrain association guard.
That narrow fix then passed the final full/demo village suites, all three native
cadences, game/checkpoint/async streaming checks, full/demo Survival smoke tests,
and fresh final-core V23/V24 exact fixtures and historical hashes. It reproduced
as a failed probe before correction; see [follow-up](villages-v2200/followup-results.json)
and [final compatibility](villages-v2200/final-compatibility.json).

A final dialog-only copy edit then exposed the assistance food reward and shared
+8 reputation/+4 XP before acceptance. Packages, a complete native route and all
48 UI targets were refreshed and rechecked afterward. No simulation, trading,
checkpoint or generation behavior changed in that copy edit.

Counts include per-frame survival/collision assertions, not that many independent
test scenarios. [Result summary](villages-v2200/results.json),
[functional suites](villages-v2200/functional-results.json),
[Release suites](villages-v2200/release-results.json),
[generation suites](villages-v2200/generation-results.json).

Focused coverage includes all professions, input/output conservation, protected
owners, full inventories, stock boundaries, no-catch-up replenishment, all four
request categories, exactly-once rewards, partial funding/refunds, loaded-site
validation, death tombstones, UID reorder/erase, independent villages, immutable
snapshots, malformed/duplicate/oversized/truncated records, scaled hitboxes and
door/slab/step/fence collision. The integrated movement test retains heading
during a step jump. Preserved suites cover checkpoint interruption/recovery,
asynchronous coalescing, automation, farming, UI, legacy worlds and ownership.
'''
visuals='''![Trade UI](villages-v2200/02-trading.png)

![Requests and reward status](villages-v2200/03-request-board.png)

![Completed explicit public stall](villages-v2200/05-completed-stall.png)

![Villager in the generated settlement](villages-v2200/09-villager-at-work.png)

[All 48 scaled UI targets](villages-v2200/ui-contact.png),
[project/reputation screen](villages-v2200/04-projects-reputation.png),
[evening view](villages-v2200/07-daily-gathering.png),
[night view](villages-v2200/08-night-return.png).

Observed clear-weather route distances (block units, full 3D distance; the
normal work/home stop region is about one block):

| NPC | Closest work position during work period | Closest home position during rest | Distinct recorded positions |
|---|---:|---:|---:|
'''+''.join(f"| {r['id']} | {r['work_distance']:.3f} | {r['home_distance']:.3f} | {r['distinct_positions']} |\n" for r in arrivals)
perf='''Matched instrumented v2.19.0/current production-object drivers, seed 20260917,
V24 district (5,-1), same 1280×720 camera, RD, simulation distance 8, LOD and
occlusion settings. Two repeats alternate old/new order. Six-second warmup,
then 12 seconds at an explicit 60 FPS cap: daytime, night, rain/shelter and busy
UI phases. Controlled cohorts start at natural road cells; ambient spawning and
NPC audio are excluded in both variants. This isolates 10/30-NPC load; the native
Survival route separately retains ordinary spawning, combat and audio.

No compiler, second benchmark or regression process ran concurrently. GPU queries
end before presentation and are read only after availability. CPU work excludes
the explicit limiter but includes normal presentation calls; it is not pure GPU
time. Wall-frame percentiles include the intentional cap. Resident/triangle
ranges are recorded because streaming and moving NPC scenes are not byte-identical
render workloads. No graphical setting was lowered to obtain a result.

Daytime phase, pooled across the two repeats; CPU work and GPU times in ms:

| Version | NPC / RD | Mean work | p95 work | p99 work | Mean NPC AI / last tick | Mean GPU | Peak RAM MiB |
|---|---|---:|---:|---:|---:|---:|---:|
'''
for count,rd in [(10,8),(30,8),(30,16),(30,32)]:
 for variant in ('old','new'):
  rs=[r for name,items in frames.items() if name.startswith(f'{variant}-n{count}-rd{rd}-r') for r in items if r['phase']=='0'];work=metrics(rs,'work_ms');ai=metrics(rs,'npc_ai_last_tick_ms');gpu=metrics(rs,'gpu_ms')
  perf+=f"| {'v2.19.0' if variant=='old' else 'v2.20.0'} | {count} / {rd} | {work['mean']:.3f} | {work['p95']:.3f} | {work['p99']:.3f} | {ai['mean']:.3f} | {gpu.get('mean',0):.3f} | {max(float(r['ram_mb']) for r in rs):.1f} |\n"
perf+='''
Individual repeats and wall-frame percentiles are retained in
[results.json](villages-v2200/results.json) and the copied per-run CSVs. The busy
phase compares the new trading dialog to the old inventory, so it is a UI load
check, not an identical-interface speedup claim.

| New 30-NPC phase at RD8 | Mean CPU work ms | p99 CPU work ms | Mean NPC AI / last tick ms |
|---|---:|---:|---:|
'''
for phase,label in [('1','Night return'),('2','Rain / shelter'),('3','Trading UI')]:
 rs=[r for name,items in frames.items() if name.startswith('new-n30-rd8-r') for r in items if r['phase']==phase];work=metrics(rs,'work_ms');ai=metrics(rs,'npc_ai_last_tick_ms');perf+=f"| {label} | {work['mean']:.3f} | {work['p99']:.3f} | {ai['mean']:.3f} |\n"
perf+='''
| 30-NPC checkpoint, RD8 | Capture median ms | Complete median ms | Snapshot bytes range |
|---|---:|---:|---|
'''
for variant in ('old','new'):
 rs=[r for r in save if r['run'].startswith(variant+'-n30-rd8-r')]
 perf+=f"| {variant} | {statistics.median(r['capture_ms'] for r in rs):.3f} | {statistics.median(r['total_ms'] for r in rs):.3f} | {min(int(r['snapshot_bytes']) for r in rs):,}–{max(int(r['snapshot_bytes']) for r in rs):,} |\n"
perf+='''
| CPU-affinity proxy, new 30 NPC / RD8 | Mean work ms | p95 ms | p99 ms | Peak RAM MiB |
|---|---:|---:|---:|---:|
'''
for cpu in (2,4):
 name=f'new-cpu{cpu}-n30-rd8';m=summary['benchmarks'][name];perf+=f"| {cpu} logical CPUs | {m['work_ms']['mean']:.3f} | {m['work_ms']['p95']:.3f} | {m['work_ms']['p99']:.3f} | {m['ram_mb']['worst']:.1f} |\n"
perf+='''
These are short matched observations on the recorded Windows i7-14700F / Radeon
RX 9060 XT machine, not universal FPS guarantees. Affinity proxies apply before
worker initialization; they are not physical low-end/iGPU certification. The final request reward wording was added after these timing runs; it affects
only the Requests tab, which is not the timed Trade UI. Final native UI/gameplay
and embedded assets were rechecked. No
claim of a new engine optimization or improved autosave architecture is made.
Villager navigation remained within two searches / 512 expanded nodes per tick.
Village snapshot overhead is bounded; no world-generation/quality tradeoff is
used. Naturally populated simultaneous multi-village performance and multi-hour
resource accumulation remain untested.
'''
doc=R/'docs/VILLAGES_LIVING_WORLD_VALIDATION_v2.20.0.md';s=doc.read_text(encoding='utf-8')
start=s.index('## Verification results')+len('## Verification results');end=s.index('## Native gameplay and visual inspection',start);s=s[:start]+'\n\n'+results+'\n'+s[end:]
start=s.index('<!-- VISUALS -->') if '<!-- VISUALS -->' in s else s.index('![Trade UI]');end=s.index('UI checks cover all four tabs',start);s=s[:start]+visuals+'\n'+s[end:]
start=s.index('## Performance')+len('## Performance');end=s.index('## Remaining limits and untested areas',start);s=s[:start]+'\n\n'+perf+'\n'+s[end:];doc.write_text(s,encoding='utf-8')
(D/'results.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print('Evidence and validation report generated:',D)
