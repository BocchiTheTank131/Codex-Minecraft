"""Prepare an archived v2.18.0 tree with common timing/test hooks only.

Never edits or resets the active checkout. Build this tree separately and retain
the archive, patch manifest and untouched public binaries as the baseline proof.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,zipfile

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--archive',type=Path,default=ROOT/'build/v2181-validation/baseline/source-v2.18.0.zip')
p.add_argument('--output',type=Path,default=ROOT/'build/v2181-validation/baseline-source')
a=p.parse_args();out=a.output.resolve()
if out.exists():raise SystemExit('Preserve existing baseline tree; choose a new output directory')
out.mkdir(parents=True)
with zipfile.ZipFile(a.archive) as archive:
    for name in archive.namelist():
        if not (out/name).resolve().is_relative_to(out):raise ValueError('Archive escapes the baseline directory')
    archive.extractall(out)
changed=[]
def write(name,text):
    (out/name).write_bytes(text.encode('utf-8'));changed.append(name)
def replace(name,old,new):
    text=(out/name).read_text(encoding='utf-8');assert old in text,(name,old)
    write(name,text.replace(old,new))
for name,cls in [('include/Game.h','Game'),('include/World.h','World'),('include/Player.h','Player'),
                 ('include/Survival.h','Inventory'),('include/Survival.h','SurvivalWorld'),
                 ('include/Transportation.h','Transportation'),('include/Automation.h','Automation')]:
    replace(name,'class '+cls+' {','class '+cls+' {\n    friend class EngineOptimizationTest;')
for name in ('include/ChunkStreaming.h','src/StreamingValidation.cpp','src/OptimizationBenchmark.cpp'):
    write(name,(ROOT/name).read_text(encoding='utf-8'))
# Diagnostic fields alone do not change the original worker plan or page policy.
replace('src/WorldStreaming.cpp','    streamingStats_.unloadMilliseconds=0;',
    '    streamingStats_.unloadMilliseconds=0;\n    streamingStats_.bufferAllocationMilliseconds=streamingStats_.bufferUploadMilliseconds=0;\n'
    '    streamingStats_.pageVboDeleteMilliseconds=streamingStats_.pageEboDeleteMilliseconds=streamingStats_.pageVaoDeleteMilliseconds=0;\n'
    '    streamingStats_.pageDeleteCalls=streamingStats_.pageAllocations=0;')
replace('src/World.cpp','            destination=std::make_shared<TerrainBufferPage>(std::max(capacity,needed),indexBytes);',
    '            const auto allocationStart=std::chrono::steady_clock::now();\n'
    '            destination=std::make_shared<TerrainBufferPage>(std::max(capacity,needed),indexBytes);\n'
    '            streamingStats_.bufferAllocationMilliseconds+=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-allocationStart).count();\n'
    '            ++streamingStats_.pageAllocations;')
replace('src/World.cpp','    glBufferSubData(GL_ARRAY_BUFFER,first*sizeof(PackedTerrainVertex),bytes,vertices->data());',
    '    const auto vertexUploadStart=std::chrono::steady_clock::now();\n'
    '    glBufferSubData(GL_ARRAY_BUFFER,first*sizeof(PackedTerrainVertex),bytes,vertices->data());\n'
    '    streamingStats_.bufferUploadMilliseconds+=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-vertexUploadStart).count();')
replace('src/World.cpp','            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,first*2*indexBytes,indices.size()*indexBytes,shortIndices.data());',
    '            const auto indexUploadStart=std::chrono::steady_clock::now();\n'
    '            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,first*2*indexBytes,indices.size()*indexBytes,shortIndices.data());\n'
    '            streamingStats_.bufferUploadMilliseconds+=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-indexUploadStart).count();')
replace('src/World.cpp','        } else glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,first*2*indexBytes,indices.size()*indexBytes,indices.data());',
    '        } else {const auto indexUploadStart=std::chrono::steady_clock::now();\n'
    '            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,first*2*indexBytes,indices.size()*indexBytes,indices.data());\n'
    '            streamingStats_.bufferUploadMilliseconds+=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-indexUploadStart).count();}')
for name in ('tests/stability/main.cpp','tests/stability/CMakeLists.txt','tests/stability/engine.cpp'):
    write(name,(ROOT/name).read_text(encoding='utf-8'))
with (out/'tests/stability/CMakeLists.txt').open('a',encoding='utf-8') as f:
    f.write('\ntarget_compile_definitions(StabilityDriver PRIVATE ENGINE_BASELINE)\n')
manifest={'archive_sha256':hashlib.sha256(a.archive.read_bytes()).hexdigest(),
          'source_commit':'507fefdec29f5b2848d1344b520d94a34fe8df95',
          'changed_files':{name:hashlib.sha256((out/name).read_bytes()).hexdigest() for name in sorted(set(changed))},
          'purpose':'Common timing and private test access only; original generation, saving, allocator and worker algorithms retained.'}
(out/'INSTRUMENTATION.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('Prepared original-source instrumentation:',out)
