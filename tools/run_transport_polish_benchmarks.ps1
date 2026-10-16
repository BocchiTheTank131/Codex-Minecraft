param(
 [string]$BaselineExe='build/v2141-baseline/VoxelFrontier.exe',
 [string]$CurrentExe='build/installed-release/Release/VoxelFrontier.exe',
 [string]$Output='build/v2141-validation/performance',
 [int]$Repeats=2
)
$ErrorActionPreference='Stop'
$baseline=(Resolve-Path $BaselineExe).Path;$current=(Resolve-Path $CurrentExe).Path;$out=[IO.Path]::GetFullPath($Output)
$previous=$env:VOXEL_FRONTIER_DATA_DIR
try {
 foreach($rd in @(8,16,32)){foreach($scene in @('ocean','gameplay')){for($run=1;$run -le $Repeats;++$run){
  $order=if($run%2){@('baseline','current')}else{@('current','baseline')}
  foreach($variant in $order){
   $name="$variant-rd$rd-$scene-$run";$directory=Join-Path $out $name;$profile=Join-Path $out ($name+'-profile')
   New-Item -ItemType Directory -Force $directory,$profile|Out-Null;$env:VOXEL_FRONTIER_DATA_DIR=$profile
   $exe=if($variant -eq 'baseline'){$baseline}else{$current}
   $arguments=@(('"--optimization-benchmark='+$directory+'"'),"--optimization-scene=$scene","--optimization-rd=$rd",'--optimization-lod=0','--optimization-occlusion=0','--seed=123456789')
   if($scene -eq 'ocean'){$arguments+='--transport-load'}
   Write-Output "Starting matched $name"
   $process=Start-Process $exe -ArgumentList $arguments -WorkingDirectory $profile -WindowStyle Normal -PassThru;$process.WaitForExit()
   if($process.ExitCode){throw "$name failed $($process.ExitCode)"}
   $rows=Import-Csv (Join-Path $directory 'frames.csv')
   if($rows|Where-Object{[double]$_.focused_percent -lt 99 -or [double]$_.visible_percent -lt 99}){Write-Warning "$name lost focus/visibility"}
  }
 }}}
} finally {$env:VOXEL_FRONTIER_DATA_DIR=$previous}
