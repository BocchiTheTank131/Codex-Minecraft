"""Isolated native village routes and matched production-object benchmarks."""
import argparse,hashlib,json,os,subprocess,time,ctypes
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--phase',choices=['route','benchmark','freeze24','focused'],required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--fps',type=int,default=30);p.add_argument('--repeats',type=int,default=2);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
new=R/'build/v2200-validation/driver/Release/StabilityDriver.exe';old=R/'build/v2200-validation/common-benchmark/build/Release/VillageBenchmark.exe'
results=[]
def run(name,exe,args,cpus=None):
 d=out/name;d.mkdir();profile=out/(name+'-profile');profile.mkdir();(profile/'.voxel_test_sandbox').touch()
 (profile/'voxel_settings.cfg').write_text('render_distance=8\nmusic_volume=0\nterrain_lod=1\nocclusion_culling=1\n',encoding='utf-8')
 env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile);args=[str(x).replace('{out}',str(d)) for x in args];print('Starting',name,flush=True);t=time.time()
 with (d/'run.log').open('w',encoding='utf-8') as f:
  child=subprocess.Popen([str(exe),*args],cwd=profile,env=env,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW|(0x4 if cpus else 0))
  if cpus:
   kernel=ctypes.WinDLL('kernel32',use_last_error=True);native=ctypes.WinDLL('ntdll');handle=ctypes.c_void_p(int(child._handle))
   kernel.GetProcessAffinityMask.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_size_t),ctypes.POINTER(ctypes.c_size_t)];kernel.SetProcessAffinityMask.argtypes=[ctypes.c_void_p,ctypes.c_size_t];native.NtResumeProcess.argtypes=[ctypes.c_void_p];native.NtResumeProcess.restype=ctypes.c_long
   available=ctypes.c_size_t();system=ctypes.c_size_t()
   if not kernel.GetProcessAffinityMask(handle,ctypes.byref(available),ctypes.byref(system)):child.kill();raise ctypes.WinError(ctypes.get_last_error())
   bits=[i for i in range(64) if available.value&(1<<i)];mask=sum(1<<i for i in bits[:cpus])
   if len(bits)<cpus or not kernel.SetProcessAffinityMask(handle,mask):child.kill();raise RuntimeError('CPU proxy affinity failed')
   if native.NtResumeProcess(handle)!=0:child.kill();raise RuntimeError('CPU proxy resume failed')
  code=child.wait()
 results.append({'name':name,'exit':code,'logical_cpus':cpus,'seconds':time.time()-t,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()});(out/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
 if code:raise RuntimeError((name,code))
 print('Passed',name,flush=True)
if a.phase=='focused':
 for label,exe in [('full',new),('demo',R/'build/v2200-validation/demo-driver/Release/StabilityDriver.exe')]:run(label,exe,['--village-tests','{out}'])
elif a.phase=='route':run('route',new,['--village-route','{out}',a.fps,8])
elif a.phase=='freeze24':
 for label,exe in [('full',new),('demo',R/'build/v2200-validation/demo-driver/Release/StabilityDriver.exe')]:
  run(label,exe,['--generation-freeze24','{out}'])
  import zipfile
  with zipfile.ZipFile(R/'tests/fixtures/v24-exact-v2190.zip') as z:
   files=json.loads(z.read('manifest.json'))['files'];assert len(files)==738
   for name,sha in files.items():
    reference=z.read(name);assert hashlib.sha256(reference).hexdigest()==sha
    assert reference==(out/label/name).read_bytes(),(label,name)
 (out/'comparison.json').write_text(json.dumps({'chunks_per_variant':738,'full_and_demo_byte_identical_to_v2190':True},indent=2),encoding='utf-8')
else:
 for repeat in range(a.repeats):
  for count,rd in [(10,8),(30,8),(30,16),(30,32)]:
   for label,exe in ([('old',old),('new',new)] if repeat%2==0 else [('new',new),('old',old)]):run(f'{label}-n{count}-rd{rd}-r{repeat}',exe,['--village-benchmark','{out}',count,rd,60,12])

if a.phase=="benchmark":
 for cpus in (2,4):run(f"new-cpu{cpus}-n30-rd8",new,["--village-benchmark","{out}",30,8,60,12],cpus=cpus)
