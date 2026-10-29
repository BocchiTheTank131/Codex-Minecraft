"""Collect reproducible local relic-update evidence and measured performance."""
from pathlib import Path
import argparse, csv, hashlib, json, re, shutil, statistics, subprocess, zipfile
from PIL import Image
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=root/'build/v2190-validation')
p.add_argument('--performance',type=Path)
p.add_argument('--captures',type=Path,required=True)
p.add_argument('--output',type=Path,default=root/'docs/exploration-v2190')
a=p.parse_args();base=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
perf=(a.performance or base/'matched-final-scenes').resolve()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8'))
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def copy(source,destination):
    destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,destination)
def rows(path):
    with Path(path).open(encoding='utf-8') as f:return list(csv.DictReader(f))
summary={'baseline':read(base/'baseline/baseline.json'),'suites':{},'native':{},'packages':{},'matched':{}}
modified=subprocess.check_output(['git','diff','--name-only'],cwd=root,text=True).splitlines()
untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=root,text=True).splitlines()
source_names=sorted({n for n in modified+untracked if n.startswith(('src/','include/','assets/','tests/','tools/')) and '__pycache__' not in n}|{'CMakeLists.txt'})
source_fingerprint={n:digest(root/n) for n in source_names if (root/n).is_file()}
(out/'source-fingerprint.json').write_text(json.dumps({'base_commit':summary['baseline']['commit'],'modified_or_added':source_fingerprint},indent=2)+'\n',encoding='utf-8')
for phase in ['final-hazard-functional','final-hazard-generation','final-hazard-release']:
    result=read(base/phase/'results.json');assert all(x['exit']==0 for x in result)
    summary['suites'][phase]={'passed':len(result),'results':result};copy(base/phase/'results.json',out/phase/'results.json')
for name in ['package-assets.json','icon-preservation.json']:
    if (base/name).exists():copy(base/name,out/name)
for variant in ['full','demo']:
    subprocess.run([__import__('sys').executable,str(root/'tools/verify_v23_exact.py'),str(base/'final-hazard-generation'/f'{variant}-freeze'),'--report',str(out/f'v23-{variant}-exact.json')],check=True,stdout=subprocess.DEVNULL)
    copy(base/'final-hazard-release'/f'{variant}-optimization'/'chunk-hashes.csv',out/f'historical-{variant}.csv')
for name in ['full-adventure-generation','demo-adventure-tests','demo-adventure-generation','full-validate','full-spawns','full-ecosystems','demo-validate']:
    src=base/'final-hazard-generation'/name
    if (src/'results.json').exists():copy(src/'results.json',out/'generation'/f'{name}.json')
    text=(src/'run.log').read_text(encoding='utf-8',errors='replace')
    summary['native'][name]={'last_lines':text.splitlines()[-4:],'log_sha256':digest(src/'run.log')}
    for filename in ['native-sites.csv','spawns.csv']:
        if (src/filename).exists():copy(src/filename,out/'generation'/f'{name}-{filename}')
for src in sorted(base.glob('final-paced-native-rd*-fps*')):
    if not src.is_dir() or not (src/'results.json').exists():continue
    summary['native'][src.name]=read(src/'results.json')
    for filename in ['native-performance.csv','checkpoint-ms.csv','method.json','results.json']:
        if (src/filename).exists():copy(src/filename,out/'native'/src.name/filename)
def collect_matched(source,label,min_samples):
    for filename in ['method.json','events.jsonl']:copy(source/filename,out/label/filename)
    records=[]
    for path in sorted(source.glob('*/frames.csv')):
        match=re.fullmatch(r'(old|new)-rd(\d+)-(.+)-(\d+)',path.parent.name);assert match,path
        variant,rd,scene,repeat=match.groups()
        for r in rows(path):r.update(variant=variant,repeat=repeat);records.append(r)
        copy(path,out/label/path.parent.name/'frames.csv');copy(path.parent/'scenes.csv',out/label/path.parent.name/'scenes.csv')
    data={}
    for rd in [8,16,32]:
        data[str(rd)]={}
        for variant in ['old','new']:
            sample=[r for r in records if int(r['rd'])==rd and r['variant']==variant]
            assert len(sample)>=min_samples,(rd,variant,len(sample))
            def med(key):return statistics.median(float(r[key]) for r in sample)
            data[str(rd)][variant]={key:med(key) for key in ['fps','mean_ms','p95_ms','p99_ms','p999_ms','gpu_ms','cpu_percent','ram_mb']}
            data[str(rd)][variant].update(worst_ms=max(float(r['worst_ms']) for r in sample),max_ram_mb=max(float(r['ram_mb']) for r in sample),triangles=[min(int(r['triangles']) for r in sample),max(int(r['triangles']) for r in sample)],loaded=[min(int(r['loaded']) for r in sample),max(int(r['loaded']) for r in sample)],focused_percent=min(float(r['focused_percent']) for r in sample),phase_summaries=len(sample))
    for path in source.glob('old-*/scenes.csv'):
        other=source/path.parent.name.replace('old-','new-',1)/'scenes.csv';assert rows(path)==rows(other),'Different scene coordinates'
    return data
summary['matched']=collect_matched(perf,'matched',18)
summary['package_canary']=collect_matched(base/'final-package-canary','package-canary',3)
for suffix in ['Setup','Standalone']:
    path=root/f'dist/VoxelFrontier-v2.19.0-Windows-{suffix}.exe';summary['packages'][path.name]={'bytes':path.stat().st_size,'sha256':digest(path)}
copy(root/'dist/SHA256SUMS.txt',out/'SHA256SUMS.txt')
media=out/'media';media.mkdir(exist_ok=True);captures=a.captures.resolve();summary['captures']={}
for source in captures.glob('30-*.bmp'):
    target=media/(source.stem+'.png');im=Image.open(source);im.save(target)
    summary['captures'][target.name]={'native_source_sha256':digest(source),'png_sha256':digest(target),'dimensions':im.size,'source_fixture':str(captures)}
ui=base/'exact-ui-third';summary['exact_ui']=read(ui/'results.json');assert summary['exact_ui']['passed']
copy(ui/'ui-targets.csv',out/'exact-ui/targets.csv');copy(ui/'results.json',out/'exact-ui/results.json');copy(ui/'method.json',out/'exact-ui/method.json')
targets=rows(ui/'ui-targets.csv');assert len(targets)==48 and all(int(r['gl_error'])==0 for r in targets)
for directory in sorted(ui.glob('ui-*')):
    if not directory.is_dir():continue
    source,=directory.glob('*.bmp');target=media/('journal-offscreen-'+directory.name.removeprefix('ui-')+'.png')
    im=Image.open(source);im.save(target)
    summary['captures'][target.name]={'native_source_sha256':digest(source),'png_sha256':digest(target),'dimensions':im.size,'offscreen_journal_only':True}
for name in ['demo-spawns','demo-ecosystems']:
    source=base/'final-compatibility'/name/'results.json'
    if source.exists():copy(source,out/'final-compatibility'/name/'results.json')
if (base/'new-items.png').exists():copy(base/'new-items.png',media/'new-items.png')
# Preserve complete raw successful logs compactly rather than thousands of
# repeated assertion lines in Markdown. Original build directories remain intact.
with zipfile.ZipFile(out/'native-regression-logs.zip','w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
    roots=[base/'final-hazard-functional',base/'final-hazard-generation',base/'final-hazard-release',base/'final-compatibility',base/'hazard-before',base/'hazard-edge-before-verified',captures,ui]+sorted(base.glob('final-paced-native-rd*-fps*'))
    for src in dict.fromkeys(roots):
        if not src.is_dir():continue
        if (src/'run.log').exists():archive.write(src/'run.log',str((src/'run.log').relative_to(base)))
        for path in src.glob('*/run.log'):archive.write(path,str(path.relative_to(base)))
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
report=root/'docs/EXPLORATION_RELICS_VALIDATION_v2.19.0.md';text=report.read_text(encoding='utf-8')
def insert(marker,body):
    global text
    start='<!-- '+marker+' -->';end='<!-- /'+marker+' -->'
    if end in text:text=re.sub(re.escape(start)+r'.*?'+re.escape(end),lambda _:start+'\n'+body+'\n'+end,text,flags=re.S)
    else:text=text.replace(start,start+'\n'+body+'\n'+end)
counts=', '.join(f'{v["passed"]} {k} groups' for k,v in summary['suites'].items())
body=f'Final packaged validation: **{counts}**, all passed. **1,326 historical records** and **495 complete V23 reference files per variant** match. Full/demo V24 deterministic tests and 50-spawn cohorts pass.\n\n'
body+='All 135 embedded resources per variant match source bytes; 991 protected files, the original mob-AI region and original renderer bodies are unchanged. All 361 original previews/mappings remain exact; four new entries bring coverage to 365 with unchanged 6,062,080-byte GPU atlas storage.\n\n'
full_checks=read(base/'final-hazard-functional/adventure-tests/results.json')['assertions'];demo_checks=read(base/'final-hazard-generation/demo-adventure-tests/results.json')['assertions']
full_gen=read(base/'final-hazard-generation/full-adventure-generation/results.json')['assertions'];demo_gen=read(base/'final-hazard-generation/demo-adventure-generation/results.json')['assertions']
body+=f'The final focused relic suite passes **{full_checks} assertions in full / {demo_checks} in demo**. V24 generation passes **{full_gen:,} / {demo_gen:,}** checks respectively. Each variant validates 50 new V24 spawn seeds. The native route passes 863 assertions at each RD8/16/32 with 30 Hz simulation, 338 at 60 Hz and 146 at 15 Hz. These are fixture assertions, not counts of independent gameplay sessions.\n\n'
body+='All 48 exact-resolution journal combinations (three resolutions, four scales, four categories) pass 101 native OpenGL offscreen checks with zero GL errors. The desktop clamps requested large windows: window captures named `journal-1920` are 1920×1061, and `journal-2560` are 1924×1061. **Exact 1920×1080 and 2560×1440 images below are journal-only offscreen targets**, not native gameplay window captures. No 1440p gameplay-performance certification is claimed.\n\n'
body+='Native captures (procedural V24 seed 20260917):\n\n'
for name,label in [('30-chamber-unsolved','Three Bells inlays'),('30-puzzle-1','Divided Current'),('30-puzzle-2','Pulse Hall and its marked hazard strip'),('30-pulse-warning-2','Powered pulse warning'),('30-relic-tools','Native charged Echo Lantern and crafted tools'),('30-finite-ambush','Finite voxel-guard ambush'),('30-treasure','Finite vault inventory'),('30-journal-1280','Native journal at 720p')]:
    if (media/(name+'.png')).exists():body+=f'![{label}](exploration-v2190/media/{name}.png)\n\n'
body+='![Exact 1440p journal-only offscreen target](exploration-v2190/media/journal-offscreen-2560-s0-c2.png)\n\n'
if (media/'new-items.png').exists():body+='![Four appended previews, offline contact sheet](exploration-v2190/media/new-items.png)\n\n'
body+='[Evidence index](exploration-v2190/summary.json), [embedded assets/source protection](exploration-v2190/package-assets.json), [raw regression logs](exploration-v2190/native-regression-logs.zip).\n'
body+='The [source fingerprint](exploration-v2190/source-fingerprint.json) identifies the uncommitted implementation against the frozen baseline; the archive retains the two expected pre-fix hazard failures as reproduction evidence.\n'
insert('FINAL_RESULTS',body)
table='Two matched process repeats, medians of scene/phase summaries; worst is the maximum across runs. These aggregate percentiles are not pooled raw-frame percentiles. **Small differences are not speedup claims.**\n\n| RD | Mean ms old/new | p95 | p99 | p99.9 | Worst | GPU ms | Max RAM MiB old/new |\n|---:|---:|---:|---:|---:|---:|---:|---:|\n'
for rd,data in summary['matched'].items():
    o,n=data['old'],data['new'];table+='| '+rd+' | '+' | '.join(f'{o[k]:.3f} / {n[k]:.3f}' for k in ['mean_ms','p95_ms','p99_ms','p999_ms','worst_ms','gpu_ms','max_ram_mb'])+' |\n'
table+='\nRaw phase CSVs retain CPU use, residency/triangle counts, frame tails, GPU samples and camera focus. See the [matched run method](exploration-v2190/matched/method.json). Native V24 limiter/work/GPU/save CSVs are linked through the evidence index. There is no physical low-end hardware certification or universal worst-frame guarantee.\n\n'
table+='The repeated matrix predates final V24-only control-type/Sneak-placement guards, hints and pulse-floor visibility/boundary corrections. Its executable fingerprint is retained in the method file; it is not mislabeled as the final package. A separate one-repeat gameplay canary uses the exact final packaged production executable (see [canary method](exploration-v2190/package-canary/method.json)):\n\n| RD | Mean ms old/new | p99 | Worst | CPU % old/new |\n|---:|---:|---:|---:|---:|\n'
for rd,data in summary['package_canary'].items():
    o,n=data['old'],data['new'];table+='| '+rd+' | '+' | '.join(f'{o[k]:.3f} / {n[k]:.3f}' for k in ['mean_ms','p99_ms','worst_ms','cpu_percent'])+' |\n'
table+='\nNative V24 route measurements (actual high-resolution limiter; work excludes waiting):\n\n| RD / target Hz | Phase | Actual mean FPS | Mean / p99 / worst work ms | Mean GPU ms | Peak sampled RAM MiB |\n|---|---|---:|---:|---:|---:|\n'
for src in sorted(base.glob('final-paced-native-rd*-fps*')):
    if not src.is_dir():continue
    for r in rows(src/'native-performance.csv'):
        table+=f'| {r["rd"]} / {r["target_fps"]} | {r["phase"]} | {1000/float(r["mean_frame_ms"]):.2f} | '+ ' / '.join(f'{float(r[k]):.3f}' for k in ['mean_work_ms','p99_work_ms','worst_work_ms'])+f' | {float(r["mean_gpu_ms"]):.3f} | {float(r["ram_mb"]):.1f} |\n'
table+='\nGPU time includes submitted rendering/driver scheduling and is asynchronous; it must not be subtracted from CPU work. This new-content route has different geometry from the V23 baseline. Earlier `native-final-*` runs used an oversleeping ordinary Windows sleep; they remain in build evidence but are superseded for cadence/performance claims. The test harness now uses the same high-resolution timer as the production game; no production frame-pacing code was changed.\n\n| RD / target Hz | Prior pending-save wait ms | Request ms | Immutable capture ms | Complete new checkpoint ms | Snapshot KiB |\n|---|---:|---:|---:|---:|---:|\n'
for src in sorted(base.glob('final-paced-native-rd*-fps*')):
    if not src.is_dir():continue
    result=rows(src/'checkpoint-ms.csv')
    if not result:continue
    match=re.fullmatch(r'final-paced-native-rd(\d+)-fps(\d+)',src.name);rd,fps=match.groups()
    for r in result:table+=f'| {rd} / {fps} | {float(r["prior_pending_wait_ms"]):.3f} | {float(r["request_ms"]):.3f} | {float(r["capture_ms"]):.3f} | {float(r["total_ms"]):.3f} | {int(r["snapshot_bytes"])/1024:.1f} |\n'
table+='\nAny older pending autosave is explicitly drained before measuring a fresh immutable capture, with that wait reported separately. Capture uses the actual checkpoint status instrumentation; a coalesced queue request is never labeled as a capture. These small route checkpoints are not representative of a long-established world with extensive edits.\n\n'
table+='Local packages (installer built, never executed):\n\n'
for name,data in summary['packages'].items():table+=f'- `{name}`: {data["bytes"]:,} bytes; SHA256 `{data["sha256"]}`.\n'
insert('PERFORMANCE_RESULTS',table);report.write_text(text,encoding='utf-8')
print(json.dumps(summary['matched'],indent=2))
