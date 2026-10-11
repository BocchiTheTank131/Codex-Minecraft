"""Compare exact chunk hashes from opt-in generation audits (no terrain tolerance)."""
import argparse
import csv
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('baseline', type=Path)
parser.add_argument('current', type=Path)
parser.add_argument('--versions', nargs='+', type=int)
args = parser.parse_args()

def key(row):
    return tuple(int(row[name]) for name in ('seed', 'version', 'x', 'z'))

expected = {}
for row in csv.DictReader(args.baseline.open()):
    if args.versions is None or int(row['version']) in args.versions:
        identity = key(row)
        if identity in expected and expected[identity] != row['hash']:
            raise SystemExit(f'Non-deterministic baseline: {identity}')
        expected[identity] = row['hash']
if not expected:
    raise SystemExit('No baseline chunks matched the requested versions')
checked = set()
rows = 0
for row in csv.DictReader(args.current.open()):
    identity = key(row)
    if identity not in expected:
        continue
    if row['hash'] != expected[identity]:
        raise SystemExit(f'Hash mismatch: {identity}: {row["hash"]} != {expected[identity]}')
    checked.add(identity)
    rows += 1
missing = expected.keys() - checked
if missing:
    raise SystemExit(f'Missing baseline chunks: {sorted(missing)[:5]}')
print(f'PASS: {rows} hash rows / {len(checked)} distinct chunks; exact blocks unchanged')
