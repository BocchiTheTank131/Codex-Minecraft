param(
 [string]$BaselineExe='build/v2150-validation/baseline/VoxelFrontier.exe',
 [string]$CurrentExe='build/installed-release/Release/VoxelFrontier.exe',
 [string]$Output='build/v2150-validation/matched',
 [int[]]$Distances=@(8,16,32),
 [int]$Repeats=2
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$baseline=(Resolve-Path (Join-Path $root $BaselineExe)).Path
$current=(Resolve-Path (Join-Path $root $CurrentExe)).Path
$out=[IO.Path]::GetFullPath((Join-Path $root $Output))
if(Test-Path -LiteralPath $out){throw 'Use a fresh benchmark output directory'}
New-Item -ItemType Directory -Path $out | Out-Null
$previous=$env:VOXEL_FRONTIER_DATA_DIR
try {
 foreach($rd in $Distances){for($repeat=1;$repeat -le $Repeats;++$repeat){
  $order=if($repeat%2){@('baseline','current')}else{@('current','baseline')}
  foreach($variant in $order){
   $name="$variant-rd$rd-$repeat";$directory=Join-Path $out $name;$testProfile=Join-Path $out ($name+'-profile')
   New-Item -ItemType Directory -Path $directory,$testProfile | Out-Null
   Set-Content -LiteralPath (Join-Path $testProfile 'voxel_settings.cfg') "render_distance=$rd`nsimulation_distance=8`nfov=74`nvsync=0`nanti_aliasing=0"
   $env:VOXEL_FRONTIER_DATA_DIR=$testProfile
   $exe=if($variant -eq 'baseline'){$baseline}else{$current}
   Write-Output "Matched foreground Survival/weather/equipment benchmark: $name"
   & $exe "--optimization-benchmark=$directory" --optimization-scene=gameplay "--optimization-rd=$rd" --optimization-lod=1 --optimization-occlusion=1 --seed=123456789 | Out-Null
   if($LASTEXITCODE){throw "$name failed with exit code $LASTEXITCODE"}
   $rows=Import-Csv (Join-Path $directory 'frames.csv')
   if($rows | Where-Object{[double]$_.focused_percent -lt 99 -or [double]$_.visible_percent -lt 99}){Write-Warning "$name lost focus; review evidence"}
  }
 }}
} finally {$env:VOXEL_FRONTIER_DATA_DIR=$previous}
Write-Output "Matched benchmark evidence: $out"
