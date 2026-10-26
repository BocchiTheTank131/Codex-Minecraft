param([string]$Output='build/v2170-validation/traversal',[switch]$AllSizes,[switch]$ReuseBaseline)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $root $Output
New-Item -ItemType Directory -Force $out | Out-Null
$baseline=Join-Path $root 'build/v2162-validation/driver/Release/StabilityDriver.exe'
$current=Join-Path $root 'build/v2170-validation/driver/Release/StabilityDriver.exe'
if(-not (Test-Path $baseline)){throw 'Archived v2.16.2 driver is required for a matched baseline'}
$oldFixture=$env:VOXEL_AUTOMATION_FIXTURE
try {
 foreach($rd in @(8,16,32)){
  $env:VOXEL_AUTOMATION_FIXTURE=$null
  $dest=Join-Path $out ('v2162-rd'+$rd);New-Item -ItemType Directory -Force $dest | Out-Null
  if(-not ($ReuseBaseline -and (Test-Path (Join-Path $dest ('survival-rd'+$rd+'.csv'))))){
   & $baseline --traversal $rd $dest *> (Join-Path $dest 'run.log')
   if($LASTEXITCODE){throw "Baseline traversal failed RD$rd"}
  }
  Write-Output "Baseline RD$rd passed"
  $counts=if($AllSizes){@(100,1000,10000)}else{@(1000)}
  foreach($count in $counts){
   $env:VOXEL_AUTOMATION_FIXTURE=[string]$count
   $dest=Join-Path $out ('v2170-rd'+$rd+'-n'+$count);New-Item -ItemType Directory -Force $dest | Out-Null
   & $current --traversal $rd $dest *> (Join-Path $dest 'run.log')
   if($LASTEXITCODE){throw "Automation traversal failed RD$rd count$count"}
   Write-Output "Automation RD$rd with $count connected components passed"
  }
 }
}finally{$env:VOXEL_AUTOMATION_FIXTURE=$oldFixture}
