"""Verify embedded full/demo assets without executing or installing either build."""
import ctypes,hashlib,json,re,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1];output=R/'build/v2181-validation/package-assets.json'
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.LoadLibraryExW.argtypes=[ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_uint32];k.LoadLibraryExW.restype=ctypes.c_void_p
k.FindResourceW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p];k.FindResourceW.restype=ctypes.c_void_p
k.LoadResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];k.LoadResource.restype=ctypes.c_void_p
k.SizeofResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];k.SizeofResource.restype=ctypes.c_uint32
k.LockResource.argtypes=[ctypes.c_void_p];k.LockResource.restype=ctypes.c_void_p
k.FreeLibrary.argtypes=[ctypes.c_void_p]
rc=(R/'build/installed-release/embedded/VoxelResources.rc').read_text(encoding='utf-8');entries=re.findall(r'(\d+) RCDATA "([^"]+)"',rc);assert len(entries)>100
variants={'full':R/'build/installed-release/Release/VoxelFrontier.exe','demo':R/'dist/VoxelFrontier-v2.18.1-Windows-Standalone.exe'};results={}
for label,path in variants.items():
 module=k.LoadLibraryExW(str(path),None,0x22);assert module,ctypes.get_last_error()
 results[label]={}
 try:
  for number,filename in entries:
   resource=k.FindResourceW(module,ctypes.c_void_p(int(number)),ctypes.c_void_p(10));assert resource,(label,number)
   n=k.SizeofResource(module,resource);address=k.LockResource(k.LoadResource(module,resource));assert address and n
   data=ctypes.string_at(address,n);expected=Path(filename).read_bytes();assert data==expected,(label,filename)
   results[label][str(Path(filename).relative_to(R)).replace('\\','/')]={'bytes':n,'sha256':hashlib.sha256(data).hexdigest()}
 finally:k.FreeLibrary(module)
# Preserve art/IDs/generation modules and all hostile/Survival logic byte-for-byte.
files=subprocess.check_output(['git','ls-tree','-r','--name-only','HEAD'],cwd=R,text=True).splitlines()
protected=[f for f in files if f.startswith(('sprites/','sounds/','assets/')) and f!='assets/inventory_previews.json' or re.search(r'(^|/)(Terrain[^/]*|RegionalClimate[^/]*|GeographicDiversity[^/]*|Biome[^/]*|RiverNetwork[^/]*|Block.h|Definitions[^/]*|.*Content.inc)$',f) or f in ['src/Survival.cpp','src/Player.cpp','src/Automation.cpp','src/Farming.cpp','src/Navigation.cpp','src/Transportation.cpp','src/Equipment.cpp','src/Renderer.cpp','src/WorldGeography.cpp','src/WorldDiversity.cpp']]
expected={}
listing=subprocess.check_output(['git','ls-tree','-r','HEAD'],cwd=R,text=True)
for line in listing.splitlines():meta,name=line.split('\t',1);expected[name]=meta.split()[2]
actual=subprocess.check_output(['git','hash-object','--stdin-paths'],input='\n'.join(protected)+'\n',cwd=R,text=True).splitlines()
assert len(actual)==len(protected)
assert all(expected[f]==sha for f,sha in zip(protected,actual)),[f for f,sha in zip(protected,actual) if expected[f]!=sha]
results['protected_files']={f:sha for f,sha in zip(protected,actual)}
output.write_text(json.dumps(results,indent=2)+'\n', encoding='utf-8');print('Verified embedded assets per variant:',len(entries),'and protected source/assets:',len(protected))
