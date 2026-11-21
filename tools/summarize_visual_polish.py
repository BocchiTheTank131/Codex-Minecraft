"""Make labeled comparisons from native captures and summarize matched measurements."""
from pathlib import Path
import csv
import hashlib
import json
import statistics
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'build/v2202-validation'
OUTPUT = ROOT / 'docs/visual-polish-v2202'
OUTPUT.mkdir(exist_ok=True)

def rows(path):
    with path.open() as f:
        return list(csv.DictReader(f))

def stats(values):
    a = sorted(values)
    return dict(n=len(a), mean=statistics.mean(a), median=statistics.median(a),
                p95=a[int((len(a)-1)*.95)], p99=a[int((len(a)-1)*.99)], worst=a[-1])

manifest = {}
for name, scenes in {
    'sky': ['forest', 'sunrise', 'sunset', 'night'],
    'weather': ['rain', 'storm', 'snow'],
    'water-cave': ['ocean', 'underwater', 'cave'],
    'foliage-village': ['foliage', 'village-night'],
}.items():
    sheet = Image.new('RGB', (1280, 390*len(scenes)), '#151b24')
    draw = ImageDraw.Draw(sheet)
    for y, scene in enumerate(scenes):
        for x, variant in enumerate(['before', 'after']):
            path = next((EVIDENCE / ('native-before' if variant=='before' else 'native-final-render') / 'q1-rd8' / scene).glob('*.bmp'))
            if scene=='underwater':path=next((EVIDENCE/'underwater-final'/('underwater-'+variant)/'underwater-survival').glob('*.bmp'))
            native = Image.open(path)
            assert native.size == (1280, 720)
            sheet.paste(native.resize((640, 360), Image.Resampling.LANCZOS), (x*640, y*390+30))
            draw.text((x*640+10, y*390+9), scene+' / '+('v2.20.1' if x == 0 else 'v2.20.2'), fill='white')
            manifest[str(path.relative_to(EVIDENCE))] = hashlib.sha256(path.read_bytes()).hexdigest()
    sheet.save(OUTPUT / (name+'.png'))

benchmarks = {}
for path in sorted((EVIDENCE/'benchmarks-final-render').glob('*/frames.csv')):
    data = rows(path)
    benchmarks[path.parent.name] = {
        key: stats([float(r[column]) for r in data if float(r[column]) >= 0])
        for key, column in [('work_ms', 'work_ms'), ('gpu_ms', 'gpu_ms'), ('wall_ms', 'wall_ms'),
                            ('simulation_ms', 'simulation_ms'), ('triangles', 'triangles'),
                            ('loaded', 'loaded'), ('ram_mb', 'ram_mb'), ('visible_entity_parts','visible_entity_parts')]
    }

summary = []
for quality, distance in [(0,8), (1,8), (2,8), (1,16), (1,32)]:
    record = dict(quality=quality, rd=distance)
    for variant in ['old', 'new']:
        runs = [benchmarks[f'{variant}-r{r}-q{quality}-rd{distance}'] for r in range(2)]
        record[variant] = {k: [r[k] for r in runs] for k in runs[0]}
    old = statistics.mean(s['median'] for s in record['old']['work_ms'])
    new = statistics.mean(s['median'] for s in record['new']['work_ms'])
    record['median_work_change_percent'] = 100*(new/old-1)
    for metric in ['gpu_ms','wall_ms']:
        before=statistics.mean(r['median'] for r in record['old'][metric]);after=statistics.mean(r['median'] for r in record['new'][metric]);record[metric+'_median_change_percent']=100*(after/before-1)
    record['terrain_medians_equal']=all(record['old']['triangles'][i]['median']==record['new']['triangles'][i]['median'] for i in range(2))
    record['visible_part_medians_equal']=all(record['old']['visible_entity_parts'][i]['median']==record['new']['visible_entity_parts'][i]['median'] for i in range(2))
    record['target_percent'] = [2,4,5][quality]
    record['within_median_work_target'] = record['median_work_change_percent'] <= record['target_percent']
    summary.append(record)

sheet = Image.new('RGB', (1280, 390*3), '#151b24')
draw = ImageDraw.Draw(sheet)
for y, (scene, label) in enumerate([('08-night-villager', 'Night shelter / villager'),
                                   ('02-trading', 'Trading interface'),
                                   ('09-villager-at-work', 'Villager at work')]):
    for x, variant in enumerate(['before', 'after']):
        path = EVIDENCE/'night-verified'/('route-'+variant)/(scene+'.bmp')
        sheet.paste(Image.open(path).resize((640,360), Image.Resampling.LANCZOS), (x*640,y*390+30))
        draw.text((x*640+10,y*390+9), label+' / '+['v2.20.1','v2.20.2'][x],fill='white')
        manifest[str(path.relative_to(EVIDENCE))] = hashlib.sha256(path.read_bytes()).hexdigest()
sheet.save(OUTPUT/'village-gameplay.png')
sheet = Image.new('RGB', (1280,390), '#151b24')
draw = ImageDraw.Draw(sheet)
for x,q in enumerate([0,2]):
    path = next((EVIDENCE/'night-verified'/'full-atmosphere'/f'lights-q{q}').glob('*.bmp'))
    sheet.paste(Image.open(path).resize((640,360),Image.Resampling.LANCZOS),(x*640,30))
    draw.text((x*640+10,9),'Native source fixture / '+['Low: halos off','High: bounded halos'][x],fill='white')
    manifest[str(path.relative_to(EVIDENCE))] = hashlib.sha256(path.read_bytes()).hexdigest()
sheet.save(OUTPUT/'light-source-quality.png')

(OUTPUT/'capture-sha256.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
(OUTPUT/'measurements.json').write_text(json.dumps(dict(runs=benchmarks, comparisons=summary), indent=2)+'\n', encoding='utf-8')
table = ['| Quality / RD | Old median work ms, runs | New median work ms, runs | Change | Old/New p99 work ms, runs |',
         '| --- | --- | --- | --- | --- |']
for s in summary:
    def values(variant, metric):
        return '/'.join(f'{r[metric]:.3f}' for r in s[variant]['work_ms'])
    table.append(f"| {['Low','Medium','High'][s['quality']]} / {s['rd']} | {values('old','median')} | {values('new','median')} | {s['median_work_change_percent']:+.1f}% | {values('old','p99')} → {values('new','p99')} |")
(OUTPUT/'benchmark-table.md').write_text('\n'.join(table)+'\n', encoding='utf-8')
print('\n'.join(table).replace('→','->'))
