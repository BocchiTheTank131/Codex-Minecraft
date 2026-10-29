"""Final-binary exact generation checks after the loaded-NPC association fix."""
import csv,hashlib,json,os,subprocess,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1];out=R/'build/v2200-validation/final-compatibility';out.mkdir(exist_ok=False);results=[]
def run(name,exe,args):
 dest=out/name;dest.mkdir();profile=out/(name+'-profile');profile.mkdir();(profile/'.voxel_test_sandbox').touch();env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile);args=[str(x).replace('{out}',str(dest)) for x in args]
 print(name,flush=True)
 with (dest/'run.log').open('w') as f:code=subprocess.run([str(exe),*args],cwd=profile,env=env,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW).returncode
 results.append({'name':name,'exit':code,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()});assert code==0,(name,code)
for variant,driver,exe in [('full',R/'build/v2200-validation/driver/Release/StabilityDriver.exe',R/'build/installed-release/Release/VoxelFrontier.exe'),('demo',R/'build/v2200-validation/demo-driver/Release/StabilityDriver.exe',R/'dist/VoxelFrontier-v2.20.0-Windows-Standalone.exe')]:
 run(variant+'-v23',driver,['--generation-freeze','{out}'])
 with zipfile.ZipFile(R/'tests/fixtures/v23-exact-v2180.zip') as z:
  files=json.loads(z.read('manifest.json'))['files'];assert len(files)==495
  for name in files:assert (out/(variant+'-v23')/name).read_bytes()==z.read(name)
 run(variant+'-historical',exe,['--optimization-smoke={out}','--seed=991'])
 def hashes(path):return {(r['seed'],r['version'],r['x'],r['z']):(r['hash'],r['bytes']) for r in csv.DictReader(path.open())}
 actual=hashes(out/(variant+'-historical')/'chunk-hashes.csv');assert len(actual)==663 and actual==hashes(R/f'docs/generator-v23/historical-{variant}.csv')
summary={'runs':results,'historical_matching_records':1326,'v23_matching_fixtures_per_variant':495,'v24':json.loads((R/'build/v2200-validation/freeze24-ready/comparison.json').read_text())}
(out/'results.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print('Final full/demo exact compatibility passed',flush=True)
