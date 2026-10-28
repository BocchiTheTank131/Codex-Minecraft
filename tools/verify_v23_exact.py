"""Compare complete V23 block/loot/encounter byte exports with frozen v2.18.0."""
import argparse,hashlib,json,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('actual',type=Path);p.add_argument('--reference',type=Path,default=Path(__file__).resolve().parents[1]/'tests/fixtures/v23-exact-v2180.zip');p.add_argument('--report',type=Path);a=p.parse_args()
with zipfile.ZipFile(a.reference) as z:
 manifest=json.loads(z.read('manifest.json'));files=manifest['files'];assert len(files)==495
 for name,digest in files.items():
  reference=z.read(name);actual=(a.actual/name).read_bytes()
  assert hashlib.sha256(reference).hexdigest()==digest, 'Damaged reference: '+name
  assert actual==reference, 'V23 exact byte mismatch: '+name
report={'matched_files':len(files),'reference_commit':manifest['commit'],'reference_sha256':hashlib.sha256(a.reference.read_bytes()).hexdigest(),'actual_directory':str(a.actual.resolve()),'encoding':manifest['encoding']}
if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n', encoding='utf-8')
print(json.dumps(report,indent=2))
