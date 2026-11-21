"""Repeated village traversal/render work; matched native full-game simulation."""
from pathlib import Path
import os,subprocess,json,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=Path(__file__).resolve().parents[1];results=[]
for repeat in range(2):
 for q,rd in [(0,8),(1,8),(2,8),(1,16),(1,32)]:
  for variant in (['old','new'] if repeat==0 else ['new','old']):
   exe=r/('build/v2202-validation/'+('baseline-driver' if variant=='old' else 'driver')+'/Release/StabilityDriver.exe');name=f'{variant}-r{repeat}-q{q}-rd{rd}';dest=out/name;dest.mkdir();profile=out/(name+'-profile');profile.mkdir();(profile/'.voxel_test_sandbox').touch();(profile/'voxel_settings.cfg').write_text('music_volume=0\n');env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile)
   print(name,flush=True)
   with (dest/'run.log').open('w') as f:code=subprocess.run([str(exe),'--village-benchmark',str(dest),'30',str(rd),'60','12',str(q)],env=env,cwd=profile,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW).returncode
   results.append(dict(name=name,exit=code,sha256=hashlib.sha256(exe.read_bytes()).hexdigest()));(out/'results.json').write_text(json.dumps(results,indent=2));assert code==0,(name,code)
