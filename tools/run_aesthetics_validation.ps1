param(
 [string]$Executable='build/installed-release/Release/VoxelFrontier.exe',
 [string]$Output='build/aesthetics-validation',
 [ValidateRange(13,17)][int]$Generator=17
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$PSScriptRoot/..").Path
$exe=(Resolve-Path $Executable).Path
$root=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force "$root/profile","$root/audit" | Out-Null
[IO.File]::WriteAllText("$root/profile/voxel_settings.cfg",'')
$previousData=$env:VOXEL_FRONTIER_DATA_DIR
try {
 $env:VOXEL_FRONTIER_DATA_DIR="$root/profile"
 $p=Start-Process $exe -ArgumentList @("--aesthetics-audit=$root/audit","--generator=$Generator",'--seed=123456789') -WorkingDirectory $repo -WindowStyle Hidden -PassThru
 $p.WaitForExit()
 Get-Content "$root/profile/logs/VoxelFrontier.log" -Tail 12
 if($p.ExitCode){throw "Aesthetics audit exit $($p.ExitCode)"}
 "Audit files: $root/audit"
} finally {
 if($null-eq$previousData){Remove-Item Env:VOXEL_FRONTIER_DATA_DIR -ErrorAction SilentlyContinue}
 else{$env:VOXEL_FRONTIER_DATA_DIR=$previousData}
}
