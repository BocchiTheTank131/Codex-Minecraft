param([switch]$SkipBuild,[string]$Output='build/v2162-validation/qol-regressions')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version=[regex]::Match([IO.File]::ReadAllText((Join-Path $root 'CMakeLists.txt')),'project\(VoxelFrontier VERSION ([0-9.]+)').Groups[1].Value
$driverDir=Join-Path $root ('build/v'+$version.Replace('.','')+'-validation/driver')
$out=Join-Path $root $Output
if(Test-Path -LiteralPath $out){$out=Join-Path $out ('run-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force $out | Out-Null
$env:MSBUILDDISABLENODEREUSE='1'
if(-not $SkipBuild){
    & cmake --build (Join-Path $root 'build/installed-release') --config Release --parallel 4 -- /nodeReuse:false
    if($LASTEXITCODE){throw 'Production Release build failed'}
}
& cmake -S (Join-Path $root 'tests/stability') -B $driverDir -G 'Visual Studio 18 2026' -A x64
if($LASTEXITCODE){throw 'QOL driver configuration failed'}
& cmake --build $driverDir --config Release --parallel 2 -- /nodeReuse:false
if($LASTEXITCODE){throw 'QOL driver build failed'}
$driver=Join-Path $driverDir 'Release/StabilityDriver.exe'
foreach($suite in @('qol','building','creative-ui','farming')){
    & $driver "--$suite" $out *> (Join-Path $out ($suite+'.log'))
    if($LASTEXITCODE){throw "QOL $suite regression failed; inspect $out"}
}
Write-Output "QOL validation passed. Isolated profiles and evidence retained: $out"
