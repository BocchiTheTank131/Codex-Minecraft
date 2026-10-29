"""Run a native relic fixture with a fresh isolated profile; preserve all evidence."""
from pathlib import Path
import argparse, os, subprocess, json, hashlib
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--mode',choices=['tests','generation','route','ui','review'],default='tests')
p.add_argument('--fps',type=int,choices=[15,30,60],default=30)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--demo',action='store_true')
p.add_argument('--world-source',type=Path)
p.add_argument('--rd',type=int,choices=[8,16,32],default=8)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
profile=out/'profile';profile.mkdir();(profile/'.voxel_test_sandbox').touch()
(profile/'voxel_settings.cfg').write_text('render_distance=8\nmusic_volume=0\n',encoding='utf-8')
env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile)
exe=root/('build/v2190-validation/'+('demo-driver' if a.demo else 'driver')+'/Release/StabilityDriver.exe')
args=[str(exe),'--adventure-'+a.mode,str(out),str(a.fps),str(a.rd)]
if a.world_source:
    import shutil
    source=out/'recorded-native-world.vxw';shutil.copy2(a.world_source,source);args.append(str(source))
(out/'method.json').write_text(json.dumps({'command':args,'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'profile':str(profile),'native_route_starting_kit':'Established Survival common resources and iron armor; every relic and core comes from native containers; recorded teleports between sites'},indent=2),encoding='utf-8')
with (out/'run.log').open('w',encoding='utf-8') as log:
    result=subprocess.run(args,cwd=profile,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
print('\n'.join((out/'run.log').read_text(encoding='utf-8',errors='replace').splitlines()[-18:]))
raise SystemExit(result.returncode)
