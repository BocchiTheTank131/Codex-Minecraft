param([string]$Output='build/v2160-validation/building',[switch]$SkipBuild,[switch]$Visual,[switch]$Benchmark)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=[IO.Path]::GetFullPath((Join-Path $root $Output))
New-Item -ItemType Directory -Force $out | Out-Null
$env:MSBUILDDISABLENODEREUSE='1'
if(-not $SkipBuild){
 & cmake --build (Join-Path $root 'build/installed-release') --config Release --parallel 4 -- /nodeReuse:false
 if($LASTEXITCODE){throw 'Production Release compilation failed'}
}
& cmake -S (Join-Path $root 'tests/stability') -B (Join-Path $root 'build/v2160-validation/driver') -G 'Visual Studio 18 2026' -A x64
if($LASTEXITCODE){throw 'Building driver configuration failed'}
& cmake --build (Join-Path $root 'build/v2160-validation/driver') --config Release --parallel 2 -- /nodeReuse:false
if($LASTEXITCODE){throw 'Building driver compilation failed'}
$driver=Join-Path $root 'build/v2160-validation/driver/Release/StabilityDriver.exe'
$fixture=Join-Path $root 'tests/fixtures/building-v2151'
& $driver --building-foundation $fixture *> (Join-Path $out 'foundation.log')
if($LASTEXITCODE){throw 'Legacy building foundation regression failed'}
& $driver --building *> (Join-Path $out 'functional.log')
if($LASTEXITCODE){throw 'Building functional regression failed'}
$line=Get-Content (Join-Path $out 'functional.log') | Where-Object {$_ -like 'Isolated fixture: *'} | Select-Object -First 1
if($line){$profile=$line.Substring('Isolated fixture: '.Length).Trim('"').Replace('\\','\');foreach($file in @('building-ids.csv','building-recipes.csv')){Copy-Item -LiteralPath (Join-Path $profile $file) -Destination $out}}
& $driver --farming *> (Join-Path $out 'farming.log')
if($LASTEXITCODE){throw 'Existing farming regression failed'}
& $driver --creative-ui $out *> (Join-Path $out 'creative-ui.log')
if($LASTEXITCODE){throw 'Creative production click/paging regression failed'}
if($Visual){& $driver --building-visual $out *> (Join-Path $out 'visual.log');if($LASTEXITCODE){throw 'Building in-engine visual fixture failed'}}
if($Benchmark){& $driver --building-benchmark $out *> (Join-Path $out 'dense.log');if($LASTEXITCODE){throw 'Dense building/timing fixture failed'}}
Write-Output "Building validation passed; evidence: $out"
