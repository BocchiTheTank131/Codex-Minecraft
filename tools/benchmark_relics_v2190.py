"""Matched v2.18.1/v2.19.0 native fixtures; isolated profiles, no installation.
Run after building; do not compile or run another benchmark concurrently.
"""
import argparse, ctypes, hashlib, json, os, subprocess, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--phase',choices=['streaming','scenes','proxies','automation','traversal'],required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--repeats',type=int,default=2);p.add_argument('--distances',type=int,nargs='+');p.add_argument('--recovery-seconds',type=int,default=15);p.add_argument('--scenes',nargs='+',choices=['forest','structures','gameplay'],default=['forest','structures','gameplay']);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
old=ROOT/'build/v2190-validation/baseline/VoxelFrontier.exe'
new=ROOT/'build/installed-release/Release/VoxelFrontier.exe'
drivers={'old':ROOT/'build/v2190-validation/baseline/StabilityDriver.exe','new':ROOT/'build/v2190-validation/driver/Release/StabilityDriver.exe'}
(out/'method.json').write_text(json.dumps({'phase':a.phase,'seed':20260917,'generator':23,'repeats':a.repeats,'distances':a.distances,'recovery_seconds':a.recovery_seconds,'scenes':a.scenes,'binaries':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in (old,new,*drivers.values())},'note':'Untouched released v2.18.1 baseline, matched V23 scenes; no concurrent benchmark or compiler workloads. No physical low-end certification.'},indent=2), encoding='utf-8')
def run(name,exe,args,cpus=None,extra=None):
 dest=out/name;dest.mkdir();profile=out/(name+'-profile');profile.mkdir()
 (profile/'voxel_settings.cfg').write_text('terrain_lod=1\nocclusion_culling=1\nmusic_volume=0\nterrain_memory_budget=1\n' if cpus else 'terrain_lod=1\nocclusion_culling=1\nmusic_volume=0\n', encoding='utf-8')
 (profile/'.voxel_test_sandbox').touch()
 env=os.environ.copy();env['VOXEL_FRONTIER_DATA_DIR']=str(profile);env.update(extra or {})
 args=[str(s).replace('{out}',str(dest)) for s in args]
 start=time.time();print('Starting',name,flush=True)
 with (dest/'run.log').open('w') as f:
  flags=subprocess.CREATE_NO_WINDOW
  if cpus:flags|=0x00000004 # CREATE_SUSPENDED is not exported by all Python builds
  child=subprocess.Popen([str(exe),*args],cwd=profile,env=env,stdout=f,stderr=subprocess.STDOUT,creationflags=flags)
  if cpus:
   try:
    # The child cannot initialize its worker pools before affinity is applied.
    kernel=ctypes.WinDLL('kernel32',use_last_error=True);native=ctypes.WinDLL('ntdll')
    kernel.GetProcessAffinityMask.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_size_t),ctypes.POINTER(ctypes.c_size_t)]
    kernel.SetProcessAffinityMask.argtypes=[ctypes.c_void_p,ctypes.c_size_t]
    native.NtResumeProcess.argtypes=[ctypes.c_void_p];native.NtResumeProcess.restype=ctypes.c_long
    available=ctypes.c_size_t();system=ctypes.c_size_t();handle=ctypes.c_void_p(int(child._handle))
    if not kernel.GetProcessAffinityMask(handle,ctypes.byref(available),ctypes.byref(system)):raise ctypes.WinError(ctypes.get_last_error())
    bits=[i for i in range(64) if available.value&(1<<i)];mask=sum(1<<i for i in bits[:cpus])
    if not kernel.SetProcessAffinityMask(handle,mask):raise ctypes.WinError(ctypes.get_last_error())
    if native.NtResumeProcess(handle)<0:raise RuntimeError('Unable to resume benchmark child')
   except BaseException:
    child.kill();child.wait();raise
  result=child.wait()
 with (out/'events.jsonl').open('a') as f:f.write(json.dumps({'name':name,'seconds':time.time()-start,'exit':result,'logical_cpus':cpus})+'\n')
 if result:raise RuntimeError((name,result))
 print('Completed',name,round(time.time()-start,2),flush=True)
def order(repeat):return [('old',old),('new',new)] if repeat%2==0 else [('new',new),('old',old)]
base=['--generator=23','--seed=20260917']
if a.phase=='streaming':
 for rd in (a.distances or (8,16,32,64)):
  for repeat in range(a.repeats if rd<64 else 1):
   for variant,exe in order(repeat):run(f'{variant}-rd{rd}-{repeat}',exe,base+['--streaming-benchmark={out}',f'--streaming-rd={rd}','--streaming-flight-seconds=8',f'--streaming-recovery-seconds={a.recovery_seconds}'])
elif a.phase=='scenes':
 for rd in (a.distances or (8,16,32)):
  for repeat in range(a.repeats):
   for scene in a.scenes:
    for variant,exe in order(repeat):run(f'{variant}-rd{rd}-{scene}-{repeat}',exe,base+['--optimization-benchmark={out}',f'--optimization-rd={rd}',f'--optimization-scene={scene}','--optimization-lod=1','--optimization-occlusion=1'])
elif a.phase=='proxies':
 for cpus in (2,4):
  for repeat in range(a.repeats):
   for variant,exe in order(repeat):run(f'{variant}-cpu{cpus}-{repeat}',exe,base+['--streaming-benchmark={out}','--streaming-rd=16','--streaming-flight-seconds=8',f'--streaming-recovery-seconds={a.recovery_seconds}'],cpus=cpus)
elif a.phase=='automation':
 for repeat in range(a.repeats):
  for variant,_ in order(repeat):
   for fixture in ('benchmark','sparse'):run(f'{variant}-{fixture}-{repeat}',drivers[variant],[f'--automation-{fixture}','{out}'])
elif a.phase=='traversal':
 for rd in (a.distances or (8,16,32)):
  for count in (100,1000,10000):
   for variant,_ in order(0):run(f'{variant}-rd{rd}-n{count}',drivers[variant],['--traversal',rd,'{out}'],extra={'VOXEL_AUTOMATION_FIXTURE':str(count)})
print('Matched phase complete:',out,flush=True)
