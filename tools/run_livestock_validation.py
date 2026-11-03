"""Focused pig/cow and existing-villager checks in disposable profiles."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);results=[]
for variant,exe in [('full',r/'build/v2201-validation/driver/Release/StabilityDriver.exe'),('demo',r/'build/v2201-validation/demo-driver/Release/StabilityDriver.exe')]:
 for suite in ['village-livestock','village-tests']:
  name=variant+'-'+suite;dest=out/name;dest.mkdir();profile=out/(name+'-profile');profile.mkdir();(profile/'.voxel_test_sandbox').touch();(profile/'voxel_settings.cfg').write_text('render_distance=8\nmusic_volume=0\n');env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile);print('Running',name,flush=True)
  with (dest/'run.log').open('w') as f:code=subprocess.run([str(exe),'--'+suite,str(dest)],cwd=profile,env=env,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW).returncode
  results.append(dict(name=name,exit=code,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest()));(out/'results.json').write_text(json.dumps(results,indent=2));assert code==0,(name,code)
print('Livestock and village checks passed',flush=True)
