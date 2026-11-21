"""Verify embedded full/demo assets without executing or installing either build."""
import ctypes,hashlib,json,re,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1];output=R/'build/v2202-validation/package-assets.json'
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.LoadLibraryExW.argtypes=[ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_uint32];k.LoadLibraryExW.restype=ctypes.c_void_p
k.FindResourceW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p];k.FindResourceW.restype=ctypes.c_void_p
k.LoadResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];k.LoadResource.restype=ctypes.c_void_p
k.SizeofResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];k.SizeofResource.restype=ctypes.c_uint32
k.LockResource.argtypes=[ctypes.c_void_p];k.LockResource.restype=ctypes.c_void_p
k.FreeLibrary.argtypes=[ctypes.c_void_p]
rc=(R/'build/installed-release/embedded/VoxelResources.rc').read_text(encoding='utf-8');entries=re.findall(r'(\d+) RCDATA "([^"]+)"',rc);assert len(entries)>100
variants={'full':R/'build/installed-release/Release/VoxelFrontier.exe','demo':R/'dist/VoxelFrontier-v2.20.2-Windows-Standalone.exe'};results={}
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
# Verify the full historical art/registry/gameplay surface against the frozen release.
commit='ed879112a157f04ab9bbbc8801fbb6141d2c1387'
listing=subprocess.check_output(['git','ls-tree','-r',commit],cwd=R,text=True)
expected={}
for line in listing.splitlines():
 meta,name=line.split('\t',1);expected[name]=meta.split()[2]
allowed={'assets/inventory_previews.json','src/Renderer.cpp','include/Renderer.h','src/Game.cpp','src/InventoryUI.cpp','src/WeatherRenderer.cpp'}
protected=[n for n in expected if n.startswith(('src/','include/','assets/','sprites/','sounds/')) and n not in allowed]
actual=subprocess.check_output(['git','hash-object','--stdin-paths'],input='\n'.join(protected)+'\n',cwd=R,text=True).splitlines()
assert all(expected[n]==h for n,h in zip(protected,actual)),[n for n,h in zip(protected,actual) if expected[n]!=h]
results['unchanged_gameplay_art_registry_files']={n:h for n,h in zip(protected,actual)}
previous=json.loads(subprocess.check_output(['git','show',commit+':assets/inventory_previews.json'],cwd=R))
current=json.loads((R/'assets/inventory_previews.json').read_text(encoding='utf-8'))
assert current['items']==365 and current['entries']==previous['entries']
results['unchanged_preview_entries']=365
output.write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
print('Verified',len(entries),'embedded resources per variant;',len(protected),'unchanged gameplay/art/registry source files; 365 original previews')
