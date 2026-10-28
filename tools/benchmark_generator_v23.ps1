param(
    [string]$BaselineExe='build/v23-validation/baseline/VoxelFrontier-v2.17.0-full.exe',
    [string]$CurrentExe='build/installed-release/Release/VoxelFrontier.exe',
    [string]$Output='build/v23-validation/performance',
    [int[]]$Distances=@(8,16,32),
    [ValidateRange(1,3)][int]$Repeats=2,
    [switch]$ReverseOrder,
    [ValidateSet('streaming','gameplay')][string]$Fixture='streaming'
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$old=[IO.Path]::GetFullPath((Join-Path $repo $BaselineExe))
$new=[IO.Path]::GetFullPath((Join-Path $repo $CurrentExe))
$out=[IO.Path]::GetFullPath((Join-Path $repo $Output))
if(Test-Path -LiteralPath $out){$out=Join-Path $out ('run-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force $out | Out-Null
Get-FileHash -Algorithm SHA256 -LiteralPath $old,$new | Export-Csv (Join-Path $out 'binaries.csv') -NoTypeInformation
$prior=$env:VOXEL_FRONTIER_DATA_DIR
try {
    foreach($rd in $Distances){
        if($rd -lt 2 -or $rd -gt 64){throw 'Invalid distance'}
        for($repeat=1;$repeat -le $Repeats;++$repeat){
            $baselineFirst=$repeat%2 -eq 1
            if($ReverseOrder){$baselineFirst=-not $baselineFirst}
            $order=if($baselineFirst){@('v22','v23')}else{@('v23','v22')}
            foreach($variant in $order){
                $name="$Fixture-$variant-rd$rd-$repeat"
                $profile=Join-Path $out ($name+'-profile');$evidence=Join-Path $out $name
                New-Item -ItemType Directory -Force $profile,$evidence | Out-Null
                $env:VOXEL_FRONTIER_DATA_DIR=$profile
                # Both variants receive identical quality options; the common
                # native fixture sets uncapped/VSync-off/AA-off and SD8 itself.
                [IO.File]::WriteAllText((Join-Path $profile 'voxel_settings.cfg'),"terrain_lod=1`nocclusion_culling=1`nmusic_volume=0`n")
                $exe=if($variant -eq 'v22'){$old}else{$new}
                $version=if($variant -eq 'v22'){22}else{23}
                $arguments=@("--generator=$version",'--seed=20260917')
                if($Fixture -eq 'streaming'){
                    $arguments+=@(('--streaming-benchmark='+$evidence),"--streaming-rd=$rd",'--streaming-flight-seconds=8','--streaming-recovery-seconds=12')
                } else {
                    $arguments+=@(('--optimization-benchmark='+$evidence),'--optimization-scene=gameplay',"--optimization-rd=$rd",'--optimization-lod=1','--optimization-occlusion=1')
                }
                Write-Output "Native benchmark: $name"
                # Native foreground fixtures explicitly show/focus their window.
                # No other benchmarks or compilation should run concurrently.
                $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $profile -WindowStyle Hidden -PassThru
                $process.WaitForExit()
                if($process.ExitCode){throw "Benchmark failed: $name"}
            }
        }
    }
} finally {$env:VOXEL_FRONTIER_DATA_DIR=$prior}
Write-Output "Matched generator benchmark evidence: $out"
