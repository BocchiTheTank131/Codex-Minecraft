"""Retain compact evidence for the local v2.17.0 stabilization pass."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, default=ROOT/'build/v2170-stabilization')
p.add_argument('--output', type=Path, default=ROOT/'docs/prerelease-v2170')
a = p.parse_args()
source, output = a.source.resolve(), a.output.resolve()
output.mkdir(parents=True, exist_ok=True)

def copy(path, name=None):
    path = source/path
    target = output/(name or path.name)
    shutil.copyfile(path, target)
    if target.suffix in ('.log', '.txt'):
        target.write_text('\n'.join(s.rstrip() for s in target.read_text(errors='replace').splitlines())+'\n')

counts = {}
for suite, marker in [('automation', 'Automation assertions'), ('building', 'Building assertions'),
                      ('creative-ui', 'Creative production UI assertions'), ('qol', 'QOL assertions'),
                      ('farming', 'Farming assertions')]:
    path = Path('verified-focused')/(suite+'.log')
    text = (source/path).read_text(errors='replace')
    assert not re.search(r'passed=0', text), suite
    counts[suite] = int(re.search(re.escape(marker)+r'=(\d+)', text)[1])
    copy(path)
path = Path('verified-focused/automation-stabilization.log')
text = (source/path).read_text(errors='replace')
checks = re.findall(r'^stabilization .* passed=([01])$', text, re.M)
assert checks and all(c == '1' for c in checks) and 'Stabilization failures=0' in text
counts['stabilization'] = len(checks)
copy(path, 'stabilization.log')
text = (source/'demo-core.log').read_text(errors='replace')
assert not re.search(r'passed=0', text)
counts['demo-automation'] = int(re.search(r'Demo automation assertions=(\d+)', text)[1])
copy(Path('demo-core.log'))

regressions = list(csv.DictReader((source/'final-regressions/results.csv').open()))
assert len(regressions) == 20 and all(r['exit_code'] == '0' for r in regressions)
copy(Path('final-regressions/results.csv'), 'release-suites.csv')
for name in ['subsystems.log', 'game.log', 'checkpoint.log']:
    copy(Path('final-regressions')/name)

def hashes(path):
    rows = list(csv.DictReader(path.open()))
    result = {(r['seed'], r['version'], r['x'], r['z']): (r['bytes'], r['hash']) for r in rows}
    assert len(result) == len(rows), 'Duplicate hash fixture'
    return result

historical = []
for variant in ['full', 'demo']:
    path = Path(f'final-regressions/{variant}-optimization-evidence/chunk-hashes.csv')
    expected = hashes(ROOT/f'docs/icon-visuals-v2161/{variant}-chunk-hashes.csv')
    actual = hashes(source/path)
    assert actual == expected, variant+' historical terrain changed'
    historical.append(dict(variant=variant, matching=True, rows=len(actual),
                           versions=sorted({int(k[1]) for k in actual})))
    copy(path, variant+'-chunk-hashes.csv')

packages = []
for line in (ROOT/'dist/SHA256SUMS.txt').read_text().splitlines():
    if not line.strip():
        continue
    expected, filename = line.split(maxsplit=1)
    filename = filename.strip()
    data = (ROOT/'dist'/filename).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    assert digest == expected
    packages.append(dict(filename=filename, bytes=len(data), sha256=digest))
assert len(packages) == 2
shutil.copyfile(ROOT/'dist/SHA256SUMS.txt', output/'SHA256SUMS.txt')
for name in ['before-corrected.log', 'range-before.log', 'demo-core.log', 'precision.csv',
             'embedded-previews.json', 'icon-coverage.json', 'preview-packaging.json', 'package.log']:
    copy(Path(name))
for directory in (source/'benchmarks').iterdir():
    if directory.is_dir():
        for file in directory.glob('*.csv'):
            copy(file.relative_to(source), directory.name+'-'+file.name)
    elif directory.suffix == '.json':
        copy(directory.relative_to(source), 'benchmark-'+directory.name)

result = dict(baseline_commit='396edb629a947097ed372d6b49e0d6ef28fa8a22',
              assertions=counts, release_suites=len(regressions), historical_hashes=historical,
              packages=packages, published=False, precision_migration='Deferred')
(output/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
