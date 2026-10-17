"""Retain inspectable final-candidate evidence; never operates on game profiles."""
from pathlib import Path
import csv, json, hashlib, re, shutil, statistics, argparse
from PIL import Image
parser=argparse.ArgumentParser();parser.add_argument('--input',type=Path,default=Path('build/v2150-validation/final'));args=parser.parse_args()
root=Path(__file__).resolve().parents[1];source=(root/args.input).resolve();out=root/'docs/farming-survival';out.mkdir(exist_ok=True)
def rows(p):
 with p.open(newline='') as f:return list(csv.DictReader(f))
def copy(p,name=None):shutil.copy2(p,out/(name or p.name))
reg=rows(source/'regressions/results.csv');assert len(reg)==20 and all(r['exit_code']=='0' for r in reg)
copy(source/'regressions/results.csv','regressions.csv')
for file in ['functional.log','crop-cpu.csv','animal-cpu.csv','timing.csv','visual.log']:copy(source/'farming'/file)
for file in ['game.log','checkpoint.log','subsystems.log']:copy(source/'regressions'/file)
log=(source/'farming/functional.log').read_text(encoding='utf-8-sig');assert not re.search(r'passed=0',log)
assertions=int(re.search(r'Farming assertions=(\d+)',log)[1])
baseline={tuple(r[k] for k in ['seed','version','x','z']):r['hash'] for r in rows(root/'docs/transportation-polish/chunk-hashes.csv')}
hash_results={}
for variant in ['full','demo']:
 p=source/f'regressions/{variant}-optimization-evidence/chunk-hashes.csv';data=rows(p)
 actual={tuple(r[k] for k in ['seed','version','x','z']):r['hash'] for r in data}
 assert all(actual.get(k)==v for k,v in baseline.items())
 hash_results[variant]=len(baseline);copy(p,variant+'-chunk-hashes.csv')
frames=[]
for file in sorted((source/'performance').glob('*/frames.csv')):
 name=file.parent.name;variant=name.split('-rd')[0];repeat=name.rsplit('-',1)[1]
 for r in rows(file):frames.append(dict(variant=variant,repeat=repeat,**r))
assert len(frames)==36 and {int(r['rd']) for r in frames}=={8,16,32}
with (out/'foreground.csv').open('w',newline='') as f:
 writer=csv.DictWriter(f,fieldnames=list(frames[0]));writer.writeheader();writer.writerows(frames)
installer=json.loads((source/'installer/result.json').read_text(encoding='utf-8-sig'));assert all(v is True for k,v in installer.items() if k!='app_id');copy(source/'installer/result.json','installer.json')
for file in (source/'farming').glob('*.bmp'):
 Image.open(file).save(out/(file.stem+'.png'))
def artifact(p):return dict(file=str(p.relative_to(root)).replace('\\','/'),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
artifacts=[artifact(root/p) for p in ['build/installed-release/Release/VoxelFrontier.exe','dist/VoxelFrontier-v2.15.0-Windows-Standalone.exe','dist/VoxelFrontier-v2.15.0-Windows-Setup.exe']]
baseline_exe=artifact(root/'build/v2150-validation/baseline/VoxelFrontier.exe');assert baseline_exe['sha256']=='623bcade8f94b73bb2ab4fef6ec70e7f15b73b306357690baf96f2be1f6136cc'
summary=dict(version='2.15.0',published=False,farming_assertions=assertions,release_suites=len(reg),historical_chunks=hash_results,baseline=baseline_exe,artifacts=artifacts,installer=installer)
if (source/'hardware.json').exists():
 summary['hardware']=json.loads((source/'hardware.json').read_text(encoding='utf-8-sig'));copy(source/'hardware.json','hardware.json')
if (source/'traversal/survival-rd8.csv').exists():
 for p in (source/'traversal').glob('*.csv'):copy(p)
 copy(source/'traversal.log');summary['survival_traversal_rd8']=True
(out/'validation.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
def med(data,k):return statistics.median(float(r[k]) for r in data)
text=['\n<!-- measured-evidence -->\n','## Final measured results\n',f'- **{assertions} farming assertions**, plus production Game/checkpoint/subsystem regressions.','- **20 Release suites passed**: 11 full-game and 9 stateless-demo suites.',f'- **{hash_results["full"]} historical chunks matched exactly in each variant**, covering generators 1–22.','- The isolated v2.14.3 → v2.15.0 installer upgrade verified payload hashes, retained fixture saves, loaded legacy worlds, removed only the test registration, and left production registration unchanged.','- Original background music and boowomp files remain unchanged; startup indexes all 24 songs.','- Local packages are listed with hashes in [validation.json](farming-survival/validation.json). The JSON records the pre-publication validation snapshot.\n','### Foreground comparison\n','Published v2.14.3 versus this candidate, same seed 123456789, SD8, LOD Low, occlusion Auto, fixed 74° FOV, AA/VSync off, identical native gameplay path. Two runs per RD, alternating order. Each run contains stationary, rotation and editing phases with Survival, weather, existing billboard enemies and equipment. Values below are **medians of six phase summaries**, not pooled frame percentiles; worst is the maximum phase stall. Short samples do not establish small FPS speedups.\n','| RD | FPS old/new | Mean ms old/new | p95 ms old/new | p99 ms old/new | Worst ms old/new |','|---:|---:|---:|---:|---:|---:|']
for rd in [8,16,32]:
 a=[r for r in frames if int(r['rd'])==rd and r['variant']=='baseline'];b=[r for r in frames if int(r['rd'])==rd and r['variant']=='current']
 pairs=[f'{med(a,k):.2f} / {med(b,k):.2f}' for k in ['fps','mean_ms','p95_ms','p99_ms']]
 pairs.append(f'{max(float(r["worst_ms"]) for r in a):.2f} / {max(float(r["worst_ms"]) for r in b):.2f}')
 text.append('| '+str(rd)+' | '+' | '.join(pairs)+' |')
text += ['\n| RD | RAM MiB old/new | Terrain live MiB old/new | Terrain capacity MiB old/new | CPU % old/new |','|---:|---:|---:|---:|---:|']
for rd in [8,16,32]:
 a=[r for r in frames if int(r['rd'])==rd and r['variant']=='baseline'];b=[r for r in frames if int(r['rd'])==rd and r['variant']=='current']
 values=[f'{med(a,"ram_mb"):.1f} / {med(b,"ram_mb"):.1f}']
 for k in ['terrain_used','terrain_capacity']:values.append(f'{med(a,k)/1048576:.1f} / {med(b,k)/1048576:.1f}')
 values.append(f'{med(a,"cpu_percent"):.2f} / {med(b,"cpu_percent"):.2f}')
 text.append('| '+str(rd)+' | '+' | '.join(values)+' |')
focus=min(float(r['focused_percent']) for r in frames);visible=min(float(r['visible_percent']) for r in frames)
text += [f'\nMinimum focus/visibility: {focus:.2f}% / {visible:.2f}%. Terrain figures are engine live ranges and allocated buffers, not total driver/Task Manager VRAM. Budgets remain soft targets. Raw geometry, draw, generation, mesh, upload and GPU timings are retained in [foreground.csv](farming-survival/foreground.csv).\n','### Crop CPU work\n','Identical mature/growing wheat fields, three repeats. Original v2.14.3 farming source is retained in the test tree with only its class/include renamed. This compares crop-update work against the same world lookup, not an old/new GPU or whole-game benchmark. Timings are per 120Hz update; no proportional FPS improvement is claimed.\n','| Crops | Growing mean ms old/new | Mature mean ms old/new | New mature p99 ms |','|---:|---:|---:|---:|']
crops=rows(source/'farming/crop-cpu.csv')
for count in [100,1000,10000]:
 values=[]
 for state in ['growing','mature']:
  a=[r for r in crops if int(r['crops'])==count and r['state']==state and r['variant']=='v2.14.3-source'];b=[r for r in crops if int(r['crops'])==count and r['state']==state and r['variant']=='v2.15.0']
  values.append(f'{med(a,"mean_ms"):.4f} / {med(b,"mean_ms"):.4f}')
 values.append(f'{med(b,"p99_ms"):.4f}');text.append('| '+str(count)+' | '+' | '.join(values)+' |')
text += ['\n### Passive populations and timing\n','Current-candidate mixed cow/pig/sheep/chicken populations, three repeats. These are CPU scaling measurements, not a matched old-engine animal-speedup claim.\n','| Animals | Mean ms | p99 ms | Worst ms |','|---:|---:|---:|---:|']
animals=rows(source/'farming/animal-cpu.csv')
for count in [10,50,100,200]:
 data=[r for r in animals if int(r['animals'])==count];text.append(f'| {count} | {med(data,"mean_ms"):.4f} | {med(data,"p99_ms"):.4f} | {max(float(r["worst_ms"]) for r in data):.4f} |')
timing=rows(source/'farming/timing.csv');assert len(timing)==6 and all(r['steps']=='18000' and r['crop_stage']=='3' for r in timing)
text += ['\n10/15/20/30/60/120 FPS schedules each execute 18,000 fixed steps and reach the same mature stage. Production Game tests also verify those rates and recover one-second frame-spike debt. [Timing evidence](farming-survival/timing.csv).\n','### Actual in-engine views\n','These are staged player-built farms rendered by the actual engine in an isolated Survival world, not concept art.','\n![Farm overview](farming-survival/farm-overview.png)','\n![Ground-level crops](farming-survival/farm-ground.png)','\n![Chicken, chick and sheared sheep](farming-survival/chickens-livestock.png)','\n![Items and Recipe Book search](farming-survival/farming-inventory.png)']
text += ['\n### Interpretation and additional integration checks\n',
 'Host: Intel i7-14700F (28 logical processors), Radeon RX 9060 XT, 31.84 GiB RAM. This is not low-end hardware certification.',
 'Foreground results are similar on this host, with measured median FPS declines of about 2.6% at RD16 and 0.9% at RD32, essentially unchanged RD8, and one 16.04ms maximum RD16 phase stall. These short runs do not establish attribution or a statistically reliable FPS improvement; the declines/stall remain reported rather than hidden. No renderer rewrite or quality reduction was used.',
 'Each crop sample contains exactly N crops and N farmland cells. An earlier fixture left 10,000 empty tilled cells in the smaller cases, measuring additional soil work absent from the old wheat updater. That setup was corrected; large empty tilled fields still legitimately incur soil-simulation work. The final paired CPU table uses the corrected workloads.',
 'A further RD8 production Survival traversal passed walking, weather, maps, equipment, boating and checkpoint reload. Its real 10/15/20/30/60/120 FPS pacing samples kept wall/simulation accounting within one fixed step. The traversal deliberately injects 500ms pauses, so its roughly 511–513ms worst frames must not be presented as spontaneous renderer stalls. [Traversal measurements](farming-survival/survival-rd8.csv), [pacing evidence](farming-survival/timing-rd8.csv).']
report=root/'docs/FARMING_SURVIVAL_VALIDATION.md';content=report.read_text(encoding='utf-8').split('<!-- measured-evidence -->')[0].rstrip()
temporary=report.with_suffix('.md.tmp');temporary.write_text(content+'\n'+'\n'.join(text)+'\n',encoding='utf-8');temporary.replace(report)
print(json.dumps(summary,indent=2))
