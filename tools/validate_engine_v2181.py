"""Run existing/new engine regressions with isolated profiles; never install."""
import argparse,csv,json,os,subprocess,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--phase',choices=['functional','release','generation'],required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
driver=R/'build/v2181-validation/driver/Release/StabilityDriver.exe';demo_driver=R/'build/v2181-validation/demo-driver/Release/StabilityDriver.exe';full=R/'build/installed-release/Release/VoxelFrontier.exe';demo=R/'dist/VoxelFrontier-v2.18.1-Windows-Standalone.exe';results=[]
def run(name,exe,args):
 dest=out/name;dest.mkdir();profile=out/(name+'-profile');profile.mkdir();(profile/'voxel_settings.cfg').write_text('render_distance=2\nmusic_volume=0\n', encoding='utf-8');(profile/'.voxel_test_sandbox').touch();env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile)
 args=[str(v).replace('{out}',str(dest)) for v in args];print('Running',name,flush=True)
 with (dest/'run.log').open('w') as f:code=subprocess.run([str(exe),*args],cwd=profile,env=env,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW).returncode
 results.append({'name':name,'exit':code,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()});(out/'results.json').write_text(json.dumps(results,indent=2), encoding='utf-8')
 if code:raise RuntimeError((name,code))
 if name.startswith('demo-'):
  forbidden=[x for x in profile.rglob('*') if x.is_file() and (x.suffix in ['.vxi','.vxw','.vps','.vxm','.vtn'] or x.name in ['save.checkpoint','checkpoint.manifest','.session.lock','world.info','world_seed.txt'])];assert not forbidden,forbidden
 print('Passed',name,flush=True)
if a.phase=='functional':
 for suite in ['subsystems','game','checkpoint','engine-tests','engine-stream-stress','automation','automation-stabilization','building','creative-ui','qol','farming']:
  run(suite,driver,[] if suite=='subsystems' else ['--'+suite,'{out}'])
 run('legacy-building',driver,['--building-foundation',R/'tests/fixtures/building-v2151'])
elif a.phase=='generation':
 for suite in ['validate','spawns','ecosystems','freeze']:run('full-'+suite,driver,['--generation-'+suite,'{out}'])
 for suite in ['validate','freeze']:run('demo-'+suite,demo_driver,['--generation-'+suite,'{out}'])
 for variant in ['full','demo']:
  import zipfile
  with zipfile.ZipFile(R/'tests/fixtures/v23-exact-v2180.zip') as z:
   m=json.loads(z.read('manifest.json'))
   assert all((out/(variant+'-freeze')/n).read_bytes()==z.read(n) for n in m['files'])
elif a.phase=='release':
 for variant,exe in [('full',full),('demo',demo)]:
  suites=['transport','command','survival','weather','fluid','reset','spectator','menu','optimization','patch272','ui'] if variant=='full' else ['transport','command','survival','weather','fluid','menu','optimization','patch272','ui']
  for suite in suites:run(variant+'-'+suite,exe,['--optimization-smoke={out}' if suite=='optimization' else '--'+suite+'-smoke','--seed=991'])
  def hashes(path):return {(x['seed'],x['version'],x['x'],x['z']):(x['hash'],x['bytes']) for x in csv.DictReader(path.open())}
  expected=hashes(R/f'docs/generator-v23/historical-{variant}.csv');actual=hashes(out/(variant+'-optimization')/'chunk-hashes.csv');assert actual==expected and len(actual)==663,(variant,len(actual))
print('All selected regressions passed',flush=True)
