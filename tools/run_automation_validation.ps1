param([switch]$SkipBuild,[string]$Output='build/v2170-validation/automation')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $root $Output
if(Test-Path -LiteralPath $out){$out=Join-Path $out ('run-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force $out | Out-Null
$driverDir=Join-Path $root 'build/v2170-validation/driver'
if(-not $SkipBuild){
 & cmake --build (Join-Path $root 'build/installed-release') --config Release --parallel 4 -- /nodeReuse:false
 if($LASTEXITCODE){throw 'Production build failed'}
}
& cmake -S (Join-Path $root 'tests/stability') -B $driverDir -G 'Visual Studio 18 2026' -A x64
if($LASTEXITCODE){throw 'Driver configuration failed'}
& cmake --build $driverDir --config Release --parallel 2 -- /nodeReuse:false
if($LASTEXITCODE){throw 'Driver build failed'}
$driver=Join-Path $driverDir 'Release/StabilityDriver.exe'
foreach($suite in @('automation','building','creative-ui','qol','farming')){
 & $driver "--$suite" $out *> (Join-Path $out ($suite+'.log'))
 if($LASTEXITCODE){throw "$suite regression failed; inspect $out"}
}
Write-Output "Automation and gameplay regressions passed: $out"
