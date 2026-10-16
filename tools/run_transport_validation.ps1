param(
 [string]$FullExe='build/installed-release/Release/VoxelFrontier.exe',
 [string]$DemoExe='build/standalone-demo-release/Release/VoxelFrontier.exe',
 [string]$Output='build/transport-validation'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
$full=(Resolve-Path $FullExe).Path;$demo=(Resolve-Path $DemoExe).Path
$out=[IO.Path]::GetFullPath($Output)
$previous=$env:VOXEL_FRONTIER_DATA_DIR
try {
 $cases=@(
  @('full-transport',$full,'--transport-smoke'),@('full-commands',$full,'--command-smoke'),@('full-survival',$full,'--survival-smoke'),@('full-weather',$full,'--weather-smoke'),@('full-fluids',$full,'--fluid-smoke'),@('full-reset',$full,'--reset-smoke'),@('full-spectator',$full,'--spectator-smoke'),
  @('full-optimization',$full,('--optimization-smoke='+$out+'/full-optimization')),
  @('demo-transport',$demo,'--transport-smoke'),@('demo-commands',$demo,'--command-smoke'),@('demo-survival',$demo,'--survival-smoke'),@('demo-fluids',$demo,'--fluid-smoke'),@('demo-optimization',$demo,('--optimization-smoke='+$out+'/demo-optimization')))
 foreach($case in $cases){
  $profile=Join-Path $out ($case[0]+'-profile');New-Item -ItemType Directory -Force $profile|Out-Null
  New-Item -ItemType File -Force (Join-Path $profile '.voxel_test_sandbox')|Out-Null
  $env:VOXEL_FRONTIER_DATA_DIR=$profile
  Write-Output ('Starting '+$case[0])
  $process=Start-Process $case[1] -ArgumentList ('"'+$case[2]+'"') -WorkingDirectory $profile -WindowStyle Hidden -PassThru
  $process.WaitForExit()
  if($process.ExitCode){Get-Content "$profile/logs/VoxelFrontier.log" -Tail 12;throw ($case[0]+' failed '+$process.ExitCode)}
  Write-Output ('Passed '+$case[0])
 }
} finally {$env:VOXEL_FRONTIER_DATA_DIR=$previous}
