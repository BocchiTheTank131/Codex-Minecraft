param(
 [Parameter(Mandatory=$true)][string]$BaselineDriver,
 [string]$Output='build/v2170-stabilization/benchmarks',
 [ValidateRange(1,5)][int]$Runs=3
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$baseline=[IO.Path]::GetFullPath((Join-Path $root $BaselineDriver))
$current=Join-Path $root 'build/v2170-validation/driver/Release/StabilityDriver.exe'
if(-not (Test-Path -LiteralPath $baseline) -or -not (Test-Path -LiteralPath $current)){throw 'Build the original v2.17.0 and current native drivers first; do not use v2.16.2.'}
$out=Join-Path $root $Output
if(Test-Path -LiteralPath $out){$out=Join-Path $out ('run-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force $out | Out-Null
@{baseline_driver=$baseline;baseline_sha256=(Get-FileHash $baseline).Hash;current_driver=$current;current_sha256=(Get-FileHash $current).Hash;runs=$Runs;cpu_samples=300;sparse_samples=220;baseline_commit='396edb629a947097ed372d6b49e0d6ef28fa8a22';note='Use an archived original-candidate driver with the same test-only sparse fixture.'} | ConvertTo-Json | Set-Content (Join-Path $out 'method.json')
for($run=1;$run -le $Runs;$run++){
 foreach($variant in $(if($run%2){@('original','current')}else{@('current','original')})){
  $exe=if($variant -eq 'original'){$baseline}else{$current}
  $dest=Join-Path $out "$variant-$run";New-Item -ItemType Directory -Force $dest | Out-Null
  foreach($mode in @('benchmark','sparse')){
   & $exe "--automation-$mode" $dest *> (Join-Path $dest ($mode+'.log'))
   if($LASTEXITCODE){throw "$variant $mode run $run failed"}
  }
  Write-Output "$variant automation measurements run $run passed"
 }
}
Write-Output "Matched original-v2.17.0 evidence: $out"
