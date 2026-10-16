param([string]$Generator='Visual Studio 18 2026')
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
$directory=Join-Path $root 'build/legacy-save-race'
New-Item -ItemType Directory -Force $directory|Out-Null
foreach($name in @('SaveFile.h','SaveFile.cpp')){
 $prefix=if($name.EndsWith('.h')){'include/'}else{'src/'}
 $source=& git -C $root show ('v2.14.0:'+$prefix+$name)
 if($LASTEXITCODE){throw 'The local v2.14.0 tag is required.'}
 [IO.File]::WriteAllLines((Join-Path $directory $name),$source,[Text.UTF8Encoding]::new($false))
}
Copy-Item -LiteralPath (Join-Path $root 'docs/transportation-polish/legacy-save-race.cpp') -Destination (Join-Path $directory 'main.cpp')
[IO.File]::WriteAllText((Join-Path $directory 'CMakeLists.txt'),"cmake_minimum_required(VERSION 3.20)`nproject(OldSaveRepro LANGUAGES CXX)`nset(CMAKE_CXX_STANDARD 17)`nadd_executable(OldSaveRepro main.cpp SaveFile.cpp)`n")
$cmake=Join-Path $env:ProgramFiles 'CMake/bin/cmake.exe'
& $cmake -S $directory -B "$directory/out" -G $Generator -A x64
if($LASTEXITCODE){throw 'Reproduction configuration failed'}
& $cmake --build "$directory/out" --config Release
if($LASTEXITCODE){throw 'Reproduction build failed'}
Push-Location $directory
try {
 & "$directory/out/Release/OldSaveRepro.exe"
 if($LASTEXITCODE){throw 'The expected historical mixed-record defect was not reproduced'}
} finally {Pop-Location}
