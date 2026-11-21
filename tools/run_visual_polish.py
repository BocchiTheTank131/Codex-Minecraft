"""Matched native OpenGL scenes; fresh profiles, no installation."""
from pathlib import Path
import subprocess,os,json,hashlib,argparse
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--variant',choices=['old','new'],required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--quick',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);records=[]
exe=r/('build/v2202-validation/baseline-driver/Release/StabilityDriver.exe' if a.variant=='old' else 'build/v2202-validation/driver/Release/StabilityDriver.exe')
for q,rd in ([(1,8)] if a.quick else [(0,8),(1,8),(2,8),(1,16),(1,32),(0,64)]):
 name=f'q{q}-rd{rd}';dest=out/name;dest.mkdir();profile=out/(name+'-profile');profile.mkdir();(profile/'.voxel_test_sandbox').touch();(profile/'voxel_settings.cfg').write_text('music_volume=0\n');env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile);print(a.variant,name,flush=True)
 with (dest/'run.log').open('w') as f:code=subprocess.run([str(exe),'--village-visual',str(dest),str(q),str(rd)],env=env,cwd=profile,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW).returncode
 records.append(dict(name=name,exit=code,sha256=hashlib.sha256(exe.read_bytes()).hexdigest()));(out/'results.json').write_text(json.dumps(records,indent=2));assert code==0,(name,code)
