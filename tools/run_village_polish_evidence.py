"""Isolated v2.20.1 native routes, focused checks and matched v2.20.0 benchmarks."""
import argparse,os,json,hashlib,subprocess,time
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--phase',choices=['native','benchmark','exact24','endurance'],required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);records=[]
new=r/'build/v2201-validation/driver/Release/StabilityDriver.exe';old=r/'build/v2201-validation/baseline/driver.exe'
def run(name,exe,args):
 d=out/name;d.mkdir();profile=out/(name+'-profile');profile.mkdir();(profile/'.voxel_test_sandbox').touch();(profile/'voxel_settings.cfg').write_text('render_distance=8\nmusic_volume=0\nterrain_lod=1\nocclusion_culling=1\n');env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile);start=time.time();print('Running',name,flush=True)
 with (d/'run.log').open('w') as f:code=subprocess.run([str(exe),*[str(x).replace('{out}',str(d)) for x in args]],cwd=profile,env=env,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW).returncode
 records.append(dict(name=name,exit=code,seconds=time.time()-start,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest()));(out/'results.json').write_text(json.dumps(records,indent=2));assert code==0,(name,code)
if a.phase=='endurance':
 run('three-village-cycles',new,['--village-endurance','{out}'])
elif a.phase=='native':
 run('polish-full',new,['--village-polish','{out}'])
 run('polish-demo',r/'build/v2201-validation/demo-driver/Release/StabilityDriver.exe',['--village-polish','{out}'])
 for fps in [15,30,60,120]:run('route'+str(fps),new,['--village-route','{out}',fps,8])
elif a.phase=='exact24':
 import zipfile
 for variant,exe in [('full',new),('demo',r/'build/v2201-validation/demo-driver/Release/StabilityDriver.exe')]:
  run(variant,exe,['--generation-freeze24','{out}'])
  with zipfile.ZipFile(r/'tests/fixtures/v24-exact-v2190.zip') as z:
   files=json.loads(z.read('manifest.json'))['files'];assert len(files)==738
   for name,digest in files.items():assert hashlib.sha256(z.read(name)).hexdigest()==digest and (out/variant/name).read_bytes()==z.read(name),name
else:
 for repeat in range(2):
  for count,rd in [(1,8),(10,8),(30,8),(60,8),(30,16),(30,32)]:
   for label,exe in ([('old',old),('new',new)] if repeat==0 else [('new',new),('old',old)]):run(f'{label}-n{count}-rd{rd}-r{repeat}',exe,['--village-benchmark','{out}',count,rd,60,12])
