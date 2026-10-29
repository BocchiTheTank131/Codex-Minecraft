"""Verify embedded full/demo assets without executing or installing either build."""
import ctypes,hashlib,json,re,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1];output=R/'build/v2200-validation/package-assets.json'
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.LoadLibraryExW.argtypes=[ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_uint32];k.LoadLibraryExW.restype=ctypes.c_void_p
k.FindResourceW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p];k.FindResourceW.restype=ctypes.c_void_p
k.LoadResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];k.LoadResource.restype=ctypes.c_void_p
k.SizeofResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];k.SizeofResource.restype=ctypes.c_uint32
k.LockResource.argtypes=[ctypes.c_void_p];k.LockResource.restype=ctypes.c_void_p
k.FreeLibrary.argtypes=[ctypes.c_void_p]
rc=(R/'build/installed-release/embedded/VoxelResources.rc').read_text(encoding='utf-8');entries=re.findall(r'(\d+) RCDATA "([^"]+)"',rc);assert len(entries)>100
variants={'full':R/'build/installed-release/Release/VoxelFrontier.exe','demo':R/'dist/VoxelFrontier-v2.20.0-Windows-Standalone.exe'};results={}
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
# Freeze all historical art except the four intentional appended atlas outputs.
commit='656763c9974b670374e7044cd65d57a9c04d91cd'
files=subprocess.check_output(['git','ls-tree','-r','--name-only',commit],cwd=R,text=True).splitlines()
allowed={'assets/inventory_previews.json'}
protected=[f for f in files if f.startswith(('sprites/','sounds/','assets/')) and f not in allowed or f.startswith(('src/Terrain','include/Terrain','src/Biome','include/Biome','src/RegionalClimate','src/GeographicDiversity','src/RiverNetwork')) or f in ['include/Block.h','src/Player.cpp','src/Farming.cpp','src/Navigation.cpp','src/Transportation.cpp','src/WorldGeography.cpp','src/WorldDensity.cpp','src/WorldDiversity.cpp','src/WorldAesthetics.cpp']]
listing=subprocess.check_output(['git','ls-tree','-r',commit],cwd=R,text=True);expected={}
for line in listing.splitlines():meta,name=line.split('\t',1);expected[name]=meta.split()[2]
actual=subprocess.check_output(['git','hash-object','--stdin-paths'],input='\n'.join(protected)+'\n',cwd=R,text=True).splitlines()
assert all(expected[f]==sha for f,sha in zip(protected,actual)),[f for f,sha in zip(protected,actual) if expected[f]!=sha]
results['protected_files']={f:sha for f,sha in zip(protected,actual)}
def old(name):return subprocess.check_output(['git','show',commit+':'+name],cwd=R).decode('utf-8').replace('\r\n','\n')
def section(text,start,end):return text[text.index(start):text.index(end)].strip()
regions=[('src/Survival.cpp','bool billboardTerrainLoaded(', 'using Recipe = RecipeInfo;'),('src/Survival.cpp','glm::vec2 SurvivalWorld::chooseNavigationHeading(', 'void SurvivalWorld::updateAnimal('),('src/Definitions.cpp','using Blocks =','const ItemDefinition& itemDefinition'),('src/World.cpp','World::GeneratedChunk World::generateChunkData(','bool World::runCraftingContentSmokeTest'),('src/Survival.cpp','void SurvivalWorld::renderBillboards(std::vector<RenderBillboard>& out) const {','std::vector<RenderItemSprite> SurvivalWorld::renderItemSprites() const')]
results['unchanged_source_regions']={}
for name,start,end in regions:
 before=section(old(name),start,end);after=section((R/name).read_text(encoding='utf-8'),start,end);assert before==after,(name,start)
 results['unchanged_source_regions'][name+':'+start]=hashlib.sha256(after.encode()).hexdigest()
for name in ['src/Adventure.cpp','src/InventoryUI.cpp','src/ExplorationLoot.cpp','src/UndergroundStructures.cpp']:
 assert (R/name).read_text(encoding='utf-8').strip()==old(name).strip(),name
renderer=(R/'src/Renderer.cpp').read_text(encoding='utf-8');original=old('src/Renderer.cpp')
assert renderer[:renderer.index('void Renderer::renderAdventure')].replace('#include "Villages.h"','').replace('#include "VillageUi.h"','').replace('\n\n','\n').strip()==original[:original.index('void Renderer::renderAdventure')].replace('\n\n','\n').strip()
assert renderer[renderer.index('void Renderer::renderNavigation'):renderer.index('void Renderer::renderVillage')].strip()==original[original.index('void Renderer::renderNavigation'):].strip()
results['original_renderer_bodies_unchanged']=True
previous=json.loads(old('assets/inventory_previews.json'));current=json.loads((R/'assets/inventory_previews.json').read_text(encoding='utf-8'))
assert current['items']==365 and current['entries']==previous['entries']
results['historical_preview_entries_unchanged']=365
output.write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8');print('Verified embedded resources:',len(entries),'per variant; protected files:',len(protected),'; 365 original icons, generation/loot and original billboard rendering unchanged')

