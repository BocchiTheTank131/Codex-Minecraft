param([switch]$SkipBuild,[switch]$IncludeDemo,[string]$Output='')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version=[regex]::Match([IO.File]::ReadAllText((Join-Path $root 'CMakeLists.txt')),'project\(VoxelFrontier VERSION ([0-9.]+)').Groups[1].Value
$validation='build/v'+$version.Replace('.','')+'-validation'
if(-not $Output){$Output=$validation+'/regressions'}
$out=[IO.Path]::GetFullPath((Join-Path $root $Output))
if(Test-Path -LiteralPath $out){$out=Join-Path $out ('run-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force $out | Out-Null
Write-Output "Fresh regression evidence: $out"
$env:MSBUILDDISABLENODEREUSE='1'
if(-not $SkipBuild){
 & cmake --build (Join-Path $root 'build/installed-release') --config Release --parallel 4 -- /nodeReuse:false
 if($LASTEXITCODE){throw 'Production Release build failed'}
}
& cmake -S (Join-Path $root 'tests/stability') -B (Join-Path $root ($validation+'/driver')) -G 'Visual Studio 18 2026' -A x64
if($LASTEXITCODE){throw 'Driver configuration failed'}
& cmake --build (Join-Path $root ($validation+'/driver')) --config Release --parallel 2 -- /nodeReuse:false
if($LASTEXITCODE){throw 'Driver build failed'}
$driver=Join-Path $root ($validation+'/driver/Release/StabilityDriver.exe')
& $driver *> (Join-Path $out 'subsystems.log');if($LASTEXITCODE){throw 'Stability subsystem regression failed'}
& $driver --game *> (Join-Path $out 'game.log');if($LASTEXITCODE){throw 'Production Game stability regression failed'}
& $driver --checkpoint *> (Join-Path $out 'checkpoint.log');if($LASTEXITCODE){throw 'Metadata checkpoint regression failed'}
$prior=$env:VOXEL_FRONTIER_DATA_DIR
$results=@()
try {
 $variants=@('full');if($IncludeDemo){$variants+='demo'}
 foreach($variant in $variants){
  $exe=if($variant -eq 'full'){Join-Path $root 'build/installed-release/Release/VoxelFrontier.exe'}else{Join-Path $root "dist/VoxelFrontier-v$version-Windows-Standalone.exe"}
  $suites=if($variant -eq 'full'){@('transport','command','survival','weather','fluid','reset','spectator','menu','optimization','patch272','ui')}else{@('transport','command','survival','weather','fluid','menu','optimization','patch272','ui')}
  foreach($suite in $suites){
   $name="$variant-$suite";$testProfile=Join-Path $out ($name+'-profile');New-Item -ItemType Directory -Force $testProfile | Out-Null
   Set-Content -LiteralPath (Join-Path $testProfile 'voxel_settings.cfg') 'render_distance=2'
   New-Item -ItemType File -Path (Join-Path $testProfile '.voxel_test_sandbox') -Force | Out-Null
   $env:VOXEL_FRONTIER_DATA_DIR=$testProfile
   $flag=if($suite -eq 'optimization'){"--optimization-smoke=$(Join-Path $out ($name+'-evidence'))"}else{"--$suite-smoke"}
   Write-Output "Running $name"
   & $exe $flag --seed=991 | Out-Null
   $code=$LASTEXITCODE;$results += [pscustomobject]@{variant=$variant;suite=$suite;exit_code=$code}
   $results | Export-Csv -LiteralPath (Join-Path $out 'results.csv') -NoTypeInformation
   if($code){throw "$name failed; inspect $testProfile/logs/VoxelFrontier.log"}
   if($variant -eq 'demo'){
    $unexpected=Get-ChildItem -LiteralPath $testProfile -File -Recurse | Where-Object { $_.Extension -in @('.vxi','.vxw','.vps','.vxm','.vtn') -or $_.Name -in @('save.checkpoint','checkpoint.manifest','.session.lock','world.info','world_seed.txt') }
    if($unexpected){throw "Demo wrote persistent data: $($unexpected.FullName)"}
   }
  }
 }
} finally {$env:VOXEL_FRONTIER_DATA_DIR=$prior}
Write-Output 'All selected Release regression suites passed'
