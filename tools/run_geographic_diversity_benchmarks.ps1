param(
    [Parameter(Mandatory=$true)][string]$BaselineExe,
    [string]$CurrentExe=(Join-Path $PSScriptRoot '../build/installed-release/Release/VoxelFrontier.exe'),
    [string]$Output=(Join-Path $PSScriptRoot '../build/v22-validation/performance'),
    [int[]]$Distances=@(8,16,32,64),
    [uint32]$Seed=20260917,
    [switch]$OnlyCurrent
)
$ErrorActionPreference='Stop'
$Output=[System.IO.Path]::GetFullPath($Output)
$BaselineExe=[System.IO.Path]::GetFullPath($BaselineExe)
$CurrentExe=[System.IO.Path]::GetFullPath($CurrentExe)
$previousData=$env:VOXEL_FRONTIER_DATA_DIR
try {
    foreach($rd in $Distances) {
        $repeats=if($rd -eq 32){2}else{1}
        for($run=1;$run -le $repeats;++$run) {
            $order=if($run%2){@('baseline','current')}else{@('current','baseline')}
            if($OnlyCurrent){$order=@('current')}
            foreach($variant in $order) {
                $name="$variant-rd$rd-$run"
                $directory=Join-Path $Output $name
                $profile=Join-Path $Output ($name+'-profile')
                New-Item -ItemType Directory -Force $directory,$profile | Out-Null
                $env:VOXEL_FRONTIER_DATA_DIR=$profile
                $exe=if($variant -eq 'baseline'){$BaselineExe}else{$CurrentExe}
                $version=if($variant -eq 'baseline'){21}else{22}
                Write-Output "Foreground streaming $name"
                # Native fixture: identical absolute coordinates, camera path,
                # 8x flight and teleport; only the generator differs.
                $process=Start-Process $exe -ArgumentList @(
                    ('"--streaming-benchmark='+$directory+'"'),"--generator=$version",
                    "--seed=$Seed","--streaming-rd=$rd",'--streaming-flight-seconds=8',
                    '--streaming-recovery-seconds=12') -WorkingDirectory $profile -WindowStyle Normal -PassThru
                $process.WaitForExit()
                if($process.ExitCode){throw "Failed $name"}
            }
        }
    }
} finally {$env:VOXEL_FRONTIER_DATA_DIR=$previousData}
