param(
    [Parameter(Mandatory=$true)][string]$BaselineExe,
    [string]$CurrentExe = (Join-Path $PSScriptRoot '../build/installed-release/Release/VoxelFrontier.exe'),
    [string]$Output = (Join-Path $PSScriptRoot '../build/render-optimization-benchmarks'),
    [int[]]$Distances = @(8,16,32,64),
    [string[]]$Scenes = @('forest','cave','ocean','gameplay'),
    [ValidateRange(1,5)][int]$Repeats = 2,
    [ValidateRange(0,3)][int]$Lod = 0,
    [ValidateRange(0,2)][int]$Occlusion = 0
)
# BaselineExe must be the original engine with the opt-in common foreground
# fixture hooks. An uninstrumented v2.13.0 executable supports the separate
# --streaming-benchmark fixture, not --optimization-benchmark.
$ErrorActionPreference='Stop'
$Output=[System.IO.Path]::GetFullPath($Output)
$BaselineExe=[System.IO.Path]::GetFullPath($BaselineExe)
$CurrentExe=[System.IO.Path]::GetFullPath($CurrentExe)
foreach($exe in @($BaselineExe,$CurrentExe)) {
    if(-not (Test-Path -LiteralPath $exe -PathType Leaf)){throw "Executable missing: $exe"}
}
New-Item -ItemType Directory -Force $Output | Out-Null
$previousData=$env:VOXEL_FRONTIER_DATA_DIR
try {
    foreach($rd in $Distances) {
        if($rd -lt 2 -or $rd -gt 64){throw "Invalid render distance $rd"}
        foreach($scene in $Scenes) {
            for($run=1;$run -le $Repeats;$run++) {
                foreach($variant in @('baseline','current')) {
                    $name="$variant-rd$rd-$scene-$run"
                    $dir=Join-Path $Output $name
                    $profile=Join-Path $env:TEMP ("VoxelFrontier-benchmark-"+[guid]::NewGuid().ToString())
                    New-Item -ItemType Directory -Force $dir,$profile | Out-Null
                    $env:VOXEL_FRONTIER_DATA_DIR=$profile
                    $exe=if($variant -eq 'baseline'){$BaselineExe}else{$CurrentExe}
                    $level=if($variant -eq 'baseline'){0}else{$Lod}
                    $occlusionMode=if($variant -eq 'baseline'){0}else{$Occlusion}
                    Write-Output "Starting $name; keep its game window focused"
                    $arguments=@(('"--optimization-benchmark='+$dir+'"'),"--optimization-scene=$scene",
                        "--optimization-rd=$rd","--optimization-lod=$level","--optimization-occlusion=$occlusionMode",'--seed=123456789')
                    # Visible windows are intentional for foreground gameplay measurement.
                    $process=Start-Process $exe -ArgumentList $arguments -WorkingDirectory $profile -WindowStyle Normal -PassThru
                    $process.WaitForExit()
                    if($process.ExitCode){throw "$name failed: $($process.ExitCode)"}
                    $rows=Import-Csv (Join-Path $dir 'frames.csv')
                    if(-not $rows){throw "No measurements recorded for $name"}
                    if($rows | Where-Object { [double]$_.focused_percent -lt 99 -or [double]$_.visible_percent -lt 99 }) {
                        Write-Warning "$name lost focus/visibility. Exclude those rows and repeat them."
                    }
                }
            }
        }
    }
} finally {
    $env:VOXEL_FRONTIER_DATA_DIR=$previousData
}
