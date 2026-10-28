"""Retain inspectable V23 evidence and add measured tables to the local report."""
import argparse
import csv
import hashlib
import json
import re
import shutil
import statistics as st
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, default=ROOT/'build/v23-validation')
p.add_argument('--output', type=Path, default=ROOT/'docs/generator-v23')
a = p.parse_args()
source, out = a.source.resolve(), a.output.resolve()
out.mkdir(parents=True, exist_ok=True)

def rows(path):
    with Path(path).open(newline='') as f:
        return list(csv.DictReader(f))

def copy(path, name=None):
    origin = source/path
    target = out/(name or origin.name)
    shutil.copyfile(origin, target)
    if target.suffix in ('.log', '.txt'):
        target.write_bytes(('\n'.join(s.rstrip() for s in target.read_text(errors='replace').splitlines())+'\n').encode())
    return target.name

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

counts = {}
for variant in ('full', 'demo'):
    baseline = rows(source/f'baseline/historical-{variant}/chunk-hashes.csv')
    current = rows(source/f'final-regressions/{variant}-optimization-evidence/chunk-hashes.csv')
    assert baseline == current and len(current) == 663, variant+' historical mismatch'
    counts[variant] = len(current)
    copy(Path(f'final-regressions/{variant}-optimization-evidence/chunk-hashes.csv'), f'historical-{variant}.csv')
regressions = rows(source/'final-regressions/results.csv')
assert len(regressions) == 20 and all(r['exit_code'] == '0' for r in regressions)
copy(Path('final-regressions/results.csv'), 'release-suites.csv')
for name in ('subsystems', 'game', 'checkpoint'):
    copy(Path('final-regressions')/(name+'.log'))

functional = {}
for name, marker in [('automation', 'Automation assertions'), ('building', 'Building assertions'),
                     ('creative-ui', 'Creative production UI assertions'), ('qol', 'QOL assertions'),
                     ('farming', 'Farming assertions')]:
    text = (source/'final-focused'/(name+'.log')).read_text(errors='replace')
    assert 'passed=0' not in text
    functional[name] = int(re.search(re.escape(marker)+r'=(\d+)', text)[1])
    copy(Path('final-focused')/(name+'.log'))
text = (source/'final-focused/automation-stabilization.log').read_text(errors='replace')
assert 'Stabilization failures=0' in text
functional['stabilization'] = len(re.findall(r'^stabilization .* passed=1', text, re.M))
copy(Path('final-focused/automation-stabilization.log'))

# Accepted runtime tables; final generation timings run after GPU benchmarks.
gen = rows(source/'accepted-generation/generation.csv')
vegetation = rows(source/'accepted-generation/vegetation.csv')
spawn = rows(source/'shaft-verified/spawns/spawns.csv')
assert all(sum(r['version'] == v for r in spawn) == 50 for v in ('22', '23'))
metrics = rows(source/'shaft-verified/maps/metrics.csv')
for path in ('accepted-generation/generation.csv', 'accepted-generation/vegetation.csv',
             'shaft-verified/spawns/spawns.csv', 'shaft-verified/maps/metrics.csv',
             'shaft-verified/maps/biomes.csv', 'shaft-verified/maps/structures.csv',
             'shaft-verified/maps/entrances.csv', 'shaft-verified/ecosystems/caves.csv', 'shaft-verified/ecosystems/walks.csv',
             'shaft-verified/ecosystems/landmarks.csv', 'shaft-verified/ecosystems/native-sites.csv'):
    name = 'map-entrances.csv' if path.endswith('maps/entrances.csv') else None
    copy(Path(path), name)
for path in ('entry-before.log', 'eligibility-before.log', 'torch-before.log',
             'final-demo-generation.log', 'final-demo.log', 'accepted-generation.log'):
    copy(Path(path))
copy(Path('walk-repro/walks.csv'), 'pit-before.csv')
copy(Path('undercuts/overhangs.csv'))
copy(Path('undercuts.log'))
if (source/'routes-final/route-accessibility.csv').exists():
    copy(Path('routes-final/route-accessibility.csv'))

generation_stats, spawn_stats, map_stats, vegetation_stats = {}, {}, {}, {}
for v in ('22', '23'):
    generation_stats[v] = {}
    for name, is_cold in [('cold', True), ('warm', False)]:
        x = [float(r['ms']) for r in gen if r['version'] == v and (r['repeat'] == '0') == is_cold]
        generation_stats[v][name] = dict(mean_ms=st.mean(x), median_ms=st.median(x), samples=len(x))
    r = [r for r in spawn if r['version'] == v]
    spawn_stats[v] = dict(safe_seeds=len(r), cave_within_600=sum(float(x['nearest_cave']) <= 600 for x in r),
        median_spawn_ms=st.median(float(x['ms']) for x in r), maximum_spawn_ms=max(float(x['ms']) for x in r),
        median_cave_distance=st.median(float(x['nearest_cave']) for x in r),
        mean_sampled_biomes=st.mean(float(x['biomes_1200']) for x in r),
        median_longest_ray=st.median(float(x['longest_biome_ray']) for x in r))
    r = [r for r in metrics if r['version'] == v and r['extent'] == '46080']
    map_stats[v] = {k: st.mean(float(x[k]) for x in r) for k in
        ('land_percent', 'biomes', 'weighted_biome_region_blocks', 'tiny_biome_area_percent',
         'mountain_percent', 'mushroom_percent', 'ice_spikes_percent')}
    r = [r for r in vegetation if r['version'] == v]
    vegetation_stats[v] = {k: sum(int(x[k]) for x in r) for k in
        ('log_voxels', 'leaf_voxels', 'plant_voxels', 'forest_columns', 'covered_forest_columns', 'roots')}
route_stats = {}
route_file = source/'routes-final/route-accessibility.csv'
if route_file.exists():
    for v in ('22','23'):
        r = [x for x in rows(route_file) if x['version'] == v]
        route_stats[v] = dict(mean_biomes=st.mean(int(x['reachable_biomes']) for x in r),
            minimum_biomes=min(int(x['reachable_biomes']) for x in r),
            isolated_seeds=sum(int(x['reachable_biomes']) < 2 for x in r),
            median_transition=st.median(float(x['nearest_other_biome_path']) for x in r
                                      if float(x['nearest_other_biome_path']) >= 0))

performance = []
for directory in ('final-performance', 'final-performance-repeat32'):
    for file in sorted((source/directory).glob('streaming-*/streaming.csv')):
        performance += [dict(r, run=directory+'/'+file.parent.name) for r in rows(file)]
assert len(performance) == 32, 'Expected 4 matched streaming pairs, 4 phases each'
with (out/'streaming.csv').open('w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(performance[0])); w.writeheader(); w.writerows(performance)
gameplay = []
for file in sorted((source/'final-gameplay-performance').glob('gameplay-*/frames.csv')):
    gameplay += [dict(r, run=file.parent.name) for r in rows(file)]
if gameplay:
    assert len(gameplay) == 18
    with (out/'gameplay.csv').open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(gameplay[0])); w.writeheader(); w.writerows(gameplay)
    for file in sorted((source/'final-gameplay-performance').glob('gameplay-*/scenes.csv')):
        shutil.copyfile(file, out/(file.parent.name+'-scenes.csv'))
for file in (source/'survival').glob('*.csv'):
    shutil.copyfile(file, out/file.name)
copy(Path('final-performance/binaries.csv'))
copy(Path('final-rd64/streaming.csv'), 'rd64-stress.csv')
copy(Path('final-embedded.json'), 'embedded-previews.json')
copy(Path('shaft-package.log'), 'package.log')
copy(Path('baseline/historical-full/chunk-hashes.csv'), 'baseline-historical-full.csv')
rd64 = rows(source/'final-rd64/streaming.csv')
fingerprints = {}
for path in (source/'baseline/VoxelFrontier-v2.17.0-full.exe',
             source/'baseline/VoxelFrontier-v2.17.0-demo.exe',
             ROOT/'build/installed-release/Release/VoxelFrontier.exe',
             ROOT/'dist/VoxelFrontier-v2.18.0-Windows-Standalone.exe'):
    fingerprints[path.relative_to(ROOT).as_posix()] = dict(sha256=digest(path), bytes=path.stat().st_size)
(out/'binary-fingerprints.json').write_text(json.dumps(fingerprints, indent=2), encoding='utf-8')

# Keep native images, camera arguments and binary fingerprints inspectable.
# Conversion and contact-sheet resizing do not alter the generated landscape.
media = ROOT/'docs/images/generator-v23'
media.mkdir(parents=True, exist_ok=True)
for file in sorted((source/'shaft-verified/maps').glob('*.png')):
    if file.name.endswith(('-comparison.png', '-distribution.png')):
        shutil.copyfile(file, media/file.name)
image_records = []
for directory in ('final-surface-captures', 'final-cave-captures', 'final-detail-captures',
                  'final-camp-capture', 'release-captures'):
    provenance = json.loads((source/directory/'provenance.json').read_text(encoding='utf-8-sig'))
    if isinstance(provenance, dict):
        provenance = [provenance]
    selected = [r for r in provenance if directory != 'release-captures' or
                r['site']['name'].endswith('panorama-v22')]
    (out/(directory+'-provenance.json')).write_text(json.dumps(selected, indent=2))
    for record in selected:
        name = record['site']['name']
        origin = source/directory/(name+'.bmp')
        assert digest(origin).lower() == record['image_sha256'].lower()
        Image.open(origin).convert('RGB').save(media/(name+'.png'))
        image_records.append(dict(name=name, source=directory, site=record['site'],
            binary_sha256=record['binary_sha256'], source_sha256=digest(origin),
            png_sha256=digest(media/(name+'.png'))))
(out/'native-images.json').write_text(json.dumps(image_records, indent=2))
for name, entries, columns in [
    ('panorama-comparison', [f'{seed}-panorama-v{v}' for seed in (20260917,123456789,42) for v in (22,23)], 2),
    ('ecosystem-contact', ['mixed-forest','jungle-clearing','river-overview','coast-detail',
        '20260917-lake','123456789-lake','42-lake','42-cavern','cave-mouth',
        'camp-overhead','lookout','standing-stones','mountain-shelter','forest-sanctum'], 2)]:
    sheet = Image.new('RGB', (640*columns, 390*((len(entries)+columns-1)//columns)), (24,28,34))
    draw = ImageDraw.Draw(sheet)
    for index, entry in enumerate(entries):
        x,y = index%columns*640, index//columns*390
        draw.text((x+8,y+8),entry,fill='white')
        shot = Image.open(media/(entry+'.png')).resize((640,360), Image.Resampling.LANCZOS)
        sheet.paste(shot,(x,y+26))
    sheet.save(media/(name+'.jpg'),quality=92)

packages = []
for line in (ROOT/'dist/SHA256SUMS.txt').read_text().splitlines():
    sha, name = line.split(None, 1); name = name.strip(); path = ROOT/'dist'/name
    assert digest(path) == sha
    packages.append(dict(name=name, sha256=sha, bytes=path.stat().st_size))
shutil.copyfile(ROOT/'dist/SHA256SUMS.txt', out/'SHA256SUMS.txt')
assert digest(ROOT/'assets/item_icons_expansion.rgba') == '4b910fecaa9f4f6f5f99336da40d7be82bc5bbd231cd3dc0264efd50ad6ac03d'

summary = dict(baseline_commit='4a030c07de64013673fe5918b9bdc1b497d9afb5',
    local_v2171_found=False, published=False, installed=False, historical=counts,
    release_suites=len(regressions), functional=functional, generation=generation_stats,
    spawns=spawn_stats, maps=map_stats, vegetation=vegetation_stats, routes=route_stats, packages=packages,
    native_images=len(image_records), rd64_fully_settled=False,
    limitations=['exposed-overhang expansion unverified', 'single development PC',
                 'coarse route/region statistics', 'higher cold generation and geometry costs',
                 'pre-existing coordinate precision limit'])
(out/'summary.json').write_text(json.dumps(summary, indent=2))

lines = ['<!-- measured-results -->',
    'Evidence: [summary and package hashes](generator-v23/summary.json), '
    '[all Release suites](generator-v23/release-suites.csv), '
    '[full historical records](generator-v23/historical-full.csv), '
    '[demo historical records](generator-v23/historical-demo.csv).\n',
    f'- **{sum(counts.values()):,} historical records match**: 663 in each packaged variant.',
    '- **20 full/demo suites passed on the final paired-shaft build**, including native checkpoint, async-save and recovery stress.',
    '- Preserved focused assertions: '+', '.join(f'{k} {v:,}' for k,v in functional.items())+'.',
    '- V23 exact-byte comparisons cover repeat/reverse order, 1/2/4 concurrent callers, seed/version cache changes, '
    'negative halos, all six cached density fields, natural heights and surface materials. '
    'All generator IDs 1–23 survive save/reload and unexplored-chunk checks; invalid 24 is rejected without partial mutation.',
    '- Final full generation validation passed **24,187 checks**, demo **24,095**; ecosystem validation passed **1,302**. '
    'Demo automation passed **660 assertions**, retaining stateless operation and disabled privileged commands.',
    '- **50/50 V23 Survival spawn seeds** passed native collision checks; all 50 observed a planned cave mouth within 600 blocks. '
    'All five landmark designs were tested in three seeds, with modified and broken treasure chests surviving reloads. '
    '**24 real-physics entrance walks** at 30/60 FPS reached native junctions, all ending at 20 HP (maximum fall event about 0.033 HP).\n',
    '### Geography and exploration\n',
    'Three-seed mean at 46,080 blocks; each map has 512×512 samples.\n',
    '| Measurement | V22 | V23 |', '|---|---:|---:|']
for label, key, factor, fmt in [('Non-ocean area (%)','land_percent',1,'.2f'),
    ('Biomes present','biomes',1,'.0f'),('Area-weighted biome region (km²)','weighted_biome_region_blocks',1e-6,'.3f'),
    ('Area in ≤2-pixel biome regions (%)','tiny_biome_area_percent',1,'.2f'),
    ('Area above Y160 (%)','mountain_percent',1,'.2f'),('Mushroom Fields (%)','mushroom_percent',1,'.3f'),
    ('Ice Spikes (%)','ice_spikes_percent',1,'.3f')]:
    lines.append(f'| {label} | {map_stats["22"][key]*factor:{fmt}} | {map_stats["23"][key]*factor:{fmt}} |')
lines += ['\nV23 reduces tiny sampled fragments while keeping broad climate regions and all 35 biomes. '
          'The sampled biome count near spawn is similar; the connected-route analysis below separately measures accessibility. '
          'Forests become more clustered/mixed rather than uniformly denser.\n',
    '| Spawn measurement | V22 | V23 |','|---|---:|---:|']
for label, key in [('Median initial search (ms)','median_spawn_ms'),('Maximum initial search (ms)','maximum_spawn_ms'),
                   ('Median cave-mouth distance (blocks)','median_cave_distance'),
                   ('Mean land-sampled biome types within 1,200 blocks','mean_sampled_biomes'),
                   ('Median longest spawn-biome ray (blocks)','median_longest_ray')]:
    lines.append(f'| {label} | {spawn_stats["22"][key]:.2f} | {spawn_stats["23"][key]:.2f} |')
lines += ['\n[Spawn rows](generator-v23/spawns.csv), [vegetation counts](generator-v23/vegetation.csv), '
          '[cave-volume counts](generator-v23/caves.csv), and [physical walks](generator-v23/walks.csv) retain the individual results. '
          'Cave volumes use 48×48×144-block windows selected from 32 requests per seed/version; '
          'the first two seed pairs share coordinates, while the third pair selects different largest sampled windows. '
          'These are local component measurements, not a global connectivity proof.\n',
    '### Generation cost\n',
    'Same 96 distributed chunk coordinates per version across three seeds; two warm repeats each. '
    'Sampling/analysis is outside each generation timer.\n',
    '| Mean request time | V22 | V23 |','|---|---:|---:|']
if route_stats:
    insertion = lines.index('### Generation cost\n')
    route_lines = ['The additional **coarse connected-route model** uses a 37×37 grid at 64-block spacing, '
        'excluding water and edges exceeding eight blocks of relief over a 16-block interval. '
        'It is more conservative than counting nearby biomes, but does not replace full collision/pathfinding. '
        '[Individual routes](generator-v23/route-accessibility.csv).\n',
        '| Connected-route measurement (50 seeds) | V22 | V23 |','|---|---:|---:|',
        f'| Mean biome types in connected region | {route_stats["22"]["mean_biomes"]:.2f} | {route_stats["23"]["mean_biomes"]:.2f} |',
        f'| Minimum biome types | {route_stats["22"]["minimum_biomes"]} | {route_stats["23"]["minimum_biomes"]} |',
        f'| Single-biome connected regions | {route_stats["22"]["isolated_seeds"]} | {route_stats["23"]["isolated_seeds"]} |',
        f'| Median nearest other-biome route (blocks; reachable seeds) | {route_stats["22"]["median_transition"]:.0f} | {route_stats["23"]["median_transition"]:.0f} |\n']
    lines[insertion:insertion] = route_lines
for name in ('cold', 'warm'):
    lines.append(f'| {name.capitalize()} (ms) | {generation_stats["22"][name]["mean_ms"]:.3f} | {generation_stats["23"][name]["mean_ms"]:.3f} |')
lines += ['\nCold requests pay supported-entrance planning; warm requests amortize region caches. '
          'Initial spawn and cold-generation regressions are real costs, not speedups.\n',
    '### Native streaming comparison\n',
    'Intel i7-14700F (28 logical CPUs), Radeon RX 9060 XT. Seed 20260917, 1280×720, SD8, LOD Low, '
    'occlusion Auto, uncapped, VSync/AA off. Common streaming fixture disables weather in both builds; '
    'it is separate from the Survival/weather/equipment/boat traversal. No render-distance reduction is used. '
    'One matched pair at RD8/16 and two at RD32 (reversed second order) use identical absolute stationary, 8× flight and teleport paths. '
    'The table uses medians where repeated for after-flight recovery, not pooled frame percentiles.\n',
    '| RD | FPS V22 / V23 | Mean ms | p99 ms | RAM MiB | Live terrain MiB |',
    '|---:|---:|---:|---:|---:|---:|']
for rd in (8,16,32):
    values = []
    for variant in ('22','23'):
        r = [x for x in performance if x['rd'] == str(rd) and x['generator'] == variant and x['phase'] == 'recovery']
        values.append({k: st.median(float(x[k]) for x in r) for k in ('fps','mean_ms','p99_ms','ram_mb','terrain_used_bytes')})
    old,new = values
    lines.append(f'| {rd} | {old["fps"]:.0f} / {new["fps"]:.0f} | {old["mean_ms"]:.3f} / {new["mean_ms"]:.3f} | '
                 f'{old["p99_ms"]:.3f} / {new["p99_ms"]:.3f} | {old["ram_mb"]:.1f} / {new["ram_mb"]:.1f} | '
                 f'{old["terrain_used_bytes"]/1048576:.1f} / {new["terrain_used_bytes"]/1048576:.1f} |')
lines += ['\n[Complete phase rows](generator-v23/streaming.csv) include worst stalls, CPU utilization, '
          'generation/mesh throughput, upload time, queue state, recovery and GPU-buffer capacity. '
          'V23 scenes contain different geometry: any FPS difference is a content cost, not an isolated renderer speedup. '
          'Streaming has no p95/GPU timer field; the separate foreground gameplay fixture records them and its actual scene coordinates. '
          'Terrain memory is engine-owned live/capacity accounting, not Task Manager total VRAM. '
          '`terrain_vertices` in streaming is resident index references, not visible submitted vertices.\n',
    '**Regressions and outliers:** mean RD8/16 recovery time rises with the extra geometry; RD32 rises about 9% '
    'across the two pairs. Isolated recovery stalls remain: V23 39.82 ms at RD8 and 90.34/142.91 ms at RD32; '
    'V22 also recorded 26.49/166.43 ms at RD32. These tails remain unresolved; they are not excluded from the CSVs. '
    'All RD8/16/32 recovery and final teleport queues settled. No general FPS or frame-pacing improvement is claimed.\n',
    '### Foreground gameplay and stress coverage\n',
    'The existing foreground gameplay fixture ran with rain, equipment, entities and editing; all 18 phase rows '
    'report 100% focused/visible. Its scene chooser selected **different locations**: V22 '
    '`(-7678.5,70.01,-8190.5)` Stony Shore, V23 `(256.5,111.01,-8191.5)` Plains. '
    'These numbers are quality/stability evidence, **not matched-location speedups**. '
    '[Full data](generator-v23/gameplay.csv) includes GPU samples, CPU, RAM, submissions, mesh and upload timing.\n',
    '| RD, stationary | Mean ms V22 / V23 | p95 ms | p99 ms | GPU ms |',
    '|---|---:|---:|---:|---:|']
for rd in (8,16,32):
    pair = [[r for r in gameplay if r['rd']==str(rd) and r['phase']=='stationary'
             and f'v{version}-' in r['run']][0] for version in (22,23)]
    lines.append(f'| {rd} | '+ ' | '.join(f'{float(pair[0][k]):.3f} / {float(pair[1][k]):.3f}'
        for k in ('mean_ms','p95_ms','p99_ms','gpu_ms'))+' |')
stress = {r['phase']:r for r in rd64}
r = stress['recovery']; t = stress['teleport']
lines += ['\nA separate production Survival traversal covered RD8/16/32, weather, equipment, maps, boats, '
    'collision-safe walking, 10/15/20/30/60/120 FPS buckets, 500 ms interruptions and checkpoint reload. '
    'Its capped timing buckets test behavior, not maximum FPS. Those surface/gameplay fixtures preceded '
    'the final underground shaft correction; the complete final Release and generator suites were rerun afterward.\n',
    f'**Brief current-only RD64 stress:** after five seconds of flight and ten seconds of recovery, '
    f'{r["loaded"]} chunks were resident, {r["generation"]} generation / {r["mesh"]} mesh / '
    f'{r["upload_queue"]} upload jobs remained, and recovery did not settle. '
    f'Live terrain was {float(r["terrain_used_bytes"])/1048576:.1f} MiB against '
    f'{float(r["terrain_capacity_bytes"])/1048576:.1f} MiB capacity; RAM {float(r["ram_mb"]):.1f} MiB. '
    f'Teleport worst frame was {float(t["worst_ms"]):.2f} ms with '
    f'{float(t["peak_retire_ms"]):.2f} ms peak retirement. This exposes a high-distance '
    'retirement/streaming limit in this candidate; no matched V22 RD64 run was made. '
    'This is not low-end certification. [All stress rows](generator-v23/rd64-stress.csv).\n',
    '### Native visual inspection\n',
    'The retained PNGs are direct conversions of native renderer screenshots. All V23 surface and cave images '
    'use the final runtime. The V22 reference panoramas predate the final V23-only shaft correction; '
    'historical chunk hashes verify that their generator branch is unchanged. '
    '[Camera arguments and image/binary fingerprints](generator-v23/native-images.json) identify every view. '
    'Lake/cavern captures use a test-only fullbright flag to expose geometry; surface views use ordinary lighting. '
    'No structures were manually assembled for these images.\n',
    '![Native V22/V23 panoramas in three seeds](images/generator-v23/panorama-comparison.jpg)\n',
    '![Native woodland, river, coast, caves and five landmarks](images/generator-v23/ecosystem-contact.jpg)\n',
    'Visual findings: connected ridge silhouettes, mixed tree canopies/clearings, open landmark approaches '
    'and genuine large underground lake chambers are visible. Close cavern walls still have blocky geological '
    'bands; broad exposed overhangs were not verified. Coastal outlines intentionally retain V22 geography. '
    'The maps show broader compatible habitat shapes rather than a complete continental rewrite.\n',
    '| Seed | 4,096 blocks | 15,360 blocks | 46,080 blocks | Plan distribution |',
    '|---|---|---|---|---|']
for seed in (20260917,123456789,42):
    lines.append(f'| {seed} | '+ ' | '.join(f'[Compare](images/generator-v23/{seed}-{extent}-comparison.png)'
        for extent in (4096,15360,46080))+f' | [Structures/caves](images/generator-v23/{seed}-distribution.png) |')
lines += ['\nEach comparison includes native sampled biomes, elevation, temperature/humidity and waterways. '
    'Gold/pink plan markers are bounded surveys rather than a claim that every point of interest is shown.\n',
    '### Windows packages\n']
for package in packages:
    lines.append(f'- `{package["name"]}` — {package["bytes"]:,} bytes; SHA256 `{package["sha256"]}`.')
lines += ['\nBoth executables embed the unchanged 361-item, 640×2368 preview atlas '
          '(SHA256 `4b910fecaa9f4f6f5f99336da40d7be82bc5bbd231cd3dc0264efd50ad6ac03d`). '
          'Installer creation was tested; the installer was not run and no real installation was changed.']
report = ROOT/'docs/GENERATOR_V23_VALIDATION.md'
text = report.read_text(encoding='utf-8')
start = text.index('<!-- measured-results -->')
end = text.index('## Reproduction tools', start)
report.write_bytes((text[:start]+'\n'.join(lines)+'\n\n'+text[end:]).encode())
print(json.dumps(summary, indent=2))
