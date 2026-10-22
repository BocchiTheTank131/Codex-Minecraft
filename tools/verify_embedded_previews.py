"""Verify that Windows full/demo binaries embed the exact baked atlas (RCDATA 100)."""
import argparse,ctypes,hashlib,json,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('executables',nargs='+',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1];expected=(root/'assets/item_icons_expansion.rgba').read_bytes();k=ctypes.WinDLL('kernel32',use_last_error=True)
k.LoadLibraryExW.argtypes=[ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_uint];k.LoadLibraryExW.restype=ctypes.c_void_p
k.FindResourceW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p];k.FindResourceW.restype=ctypes.c_void_p
k.SizeofResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];k.SizeofResource.restype=ctypes.c_uint
k.LoadResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];k.LoadResource.restype=ctypes.c_void_p
k.LockResource.argtypes=[ctypes.c_void_p];k.LockResource.restype=ctypes.c_void_p
k.FreeLibrary.argtypes=[ctypes.c_void_p]
result={}
for exe in a.executables:
    module=k.LoadLibraryExW(str(exe.resolve()),None,2)
    if not module:raise ctypes.WinError(ctypes.get_last_error())
    try:
        r=k.FindResourceW(module,100,10);assert r,'Missing item atlas resource'
        size=k.SizeofResource(module,r);data=ctypes.string_at(k.LockResource(k.LoadResource(module,r)),size)
        assert data==expected, str(exe)+' has stale/incorrect embedded previews'
        result[str(exe)]={'atlas_sha256':hashlib.sha256(data).hexdigest(),'atlas_dimensions':struct.unpack('<ii',data[:8]),'resource_bytes':size,'matches_source':True,'executable_bytes':exe.stat().st_size}
    finally:k.FreeLibrary(module)
if a.output:a.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
