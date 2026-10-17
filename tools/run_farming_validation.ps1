param([string]$Output='build/v2150-validation/farming',[switch]$Visual,[switch]$Benchmark)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=[IO.Path]::GetFullPath((Join-Path $root $Output))
New-Item -ItemType Directory -Force $out | Out-Null
$env:MSBUILDDISABLENODEREUSE='1'
& cmake -S (Join-Path $root 'tests/stability') -B (Join-Path $root 'build/v2150-validation/driver') -G 'Visual Studio 18 2026' -A x64
if($LASTEXITCODE){throw 'Farming driver configuration failed'}
& cmake --build (Join-Path $root 'build/v2150-validation/driver') --config Release --parallel 2 -- /nodeReuse:false
if($LASTEXITCODE){throw 'Farming driver compilation failed'}
$driver=Join-Path $root 'build/v2150-validation/driver/Release/StabilityDriver.exe'
& $driver --farming *> (Join-Path $out 'functional.log')
if($LASTEXITCODE){throw 'Farming functional regression failed'}
if($Benchmark){& $driver --farming-benchmark $out *> (Join-Path $out 'cpu.log');if($LASTEXITCODE){throw 'Farming CPU/timing regression failed'}}
if($Visual){& $driver --farming-visual $out *> (Join-Path $out 'visual.log');if($LASTEXITCODE){throw 'Farming in-engine fixture failed'}}
Write-Output "Farming validation passed; evidence: $out"
