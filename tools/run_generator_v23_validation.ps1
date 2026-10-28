param(
    [switch]$SkipBuild,
    [string]$Output='build/v23-validation/final',
    [ValidateSet('validate','maps','spawns','ecosystems')][string[]]$Phases=@('validate','maps','spawns','ecosystems')
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=[IO.Path]::GetFullPath((Join-Path $repo $Output))
if(Test-Path -LiteralPath $out){$out=Join-Path $out ('run-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force $out | Out-Null
if(-not $SkipBuild){
    & cmake --build (Join-Path $repo 'build/installed-release') --config Release --parallel 4 -- /nodeReuse:false
    if($LASTEXITCODE){throw 'Full Release build failed'}
}
& cmake -S (Join-Path $repo 'tests/stability') -B (Join-Path $repo 'build/v23-validation/driver') -G 'Visual Studio 18 2026' -A x64
if($LASTEXITCODE){throw 'Driver configuration failed'}
& cmake --build (Join-Path $repo 'build/v23-validation/driver') --config Release --parallel 2 -- /nodeReuse:false
if($LASTEXITCODE){throw 'Driver build failed'}
$driver=Join-Path $repo 'build/v23-validation/driver/Release/StabilityDriver.exe'
foreach($phase in $Phases){
    Write-Output "Generator audit: $phase"
    & $driver "--generation-$phase" (Join-Path $out $phase) *> (Join-Path $out "$phase.log")
    Get-Content (Join-Path $out "$phase.log") -Tail 4
    if($LASTEXITCODE){throw "Generator $phase failed"}
}
Write-Output "Isolated generator evidence: $out"
