param(
 [string]$Executable='build/installed-release/Release/VoxelFrontier.exe',
 [string]$Output='build/exploration-validation',
 [Parameter(Mandatory=$true)][string]$HistoricalBaseline
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$PSScriptRoot/..").Path
$exe=(Resolve-Path $Executable).Path
$baseline=(Resolve-Path $HistoricalBaseline).Path
$root=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force "$root/profile","$root/audit" | Out-Null
$destination=[IO.Path]::GetFullPath("$root/audit/baseline-historical.csv")
if($baseline -ne $destination){Copy-Item -LiteralPath $baseline -Destination $destination}
$previousData=$env:VOXEL_FRONTIER_DATA_DIR
try {
 $env:VOXEL_FRONTIER_DATA_DIR="$root/profile"
 $p=Start-Process $exe -ArgumentList @("--underground-audit=$root/audit",'--seed=123456789') -WorkingDirectory $root -WindowStyle Hidden -PassThru
 $p.WaitForExit()
 Get-Content "$root/profile/logs/VoxelFrontier.log" -Tail 8
 if($p.ExitCode){throw "Underground audit exit $($p.ExitCode)"}
 "Audit files: $root/audit"
} finally {
 if($null-eq$previousData){Remove-Item Env:VOXEL_FRONTIER_DATA_DIR -ErrorAction SilentlyContinue}
 else{$env:VOXEL_FRONTIER_DATA_DIR=$previousData}
}
