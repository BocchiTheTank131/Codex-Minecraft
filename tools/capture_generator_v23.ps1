param(
    [string]$Executable='build/installed-release/Release/VoxelFrontier.exe',
    [string]$Output='build/v23-validation/captures',
    [Parameter(Mandatory=$true)][string]$Sites
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$exe=[IO.Path]::GetFullPath((Join-Path $repo $Executable))
$out=[IO.Path]::GetFullPath((Join-Path $repo $Output))
if(Test-Path -LiteralPath $out){$out=Join-Path $out ('run-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force $out | Out-Null
$records=Import-Csv -LiteralPath $Sites
$provenance=@()
$prior=$env:VOXEL_FRONTIER_DATA_DIR
try {
    foreach($site in $records){
        $profile=Join-Path $out $site.name
        New-Item -ItemType Directory -Force $profile | Out-Null
        $env:VOXEL_FRONTIER_DATA_DIR=$profile
        $rd=if($site.rd){[int]$site.rd}else{12}
        [IO.File]::WriteAllText((Join-Path $profile 'voxel_settings.cfg'),"render_distance=$rd`nsimulation_distance=8`nvsync=0`nshow_fps=0`nshow_coordinates=0`nmusic_volume=0`n")
        $arguments=@('--worldgen-test','--preview-clean',"--generator=$($site.version)","--seed=$($site.seed)",
            "--preview-x=$($site.x)","--preview-y=$($site.y)","--preview-z=$($site.z)",
            "--preview-yaw=$($site.yaw)","--preview-pitch=$($site.pitch)","--preview-brightness=$($site.brightness)")
        Write-Output "Capturing native terrain: $($site.name)"
        $captureExe=$exe
        if($site.fullbright -eq '1') {
            $captureExe=Join-Path $repo 'build/v23-validation/driver/Release/StabilityDriver.exe'
            $arguments=@('--generation-lit-capture',('"'+(Join-Path $profile 'screenshots')+'"'))+$arguments
        }
        $process=Start-Process -FilePath $captureExe -ArgumentList $arguments -WorkingDirectory $profile -WindowStyle Hidden -PassThru
        $process.WaitForExit()
        if($process.ExitCode){throw "Native capture failed: $($site.name)"}
        $image=Get-ChildItem -LiteralPath (Join-Path $profile 'screenshots') -Filter '*.bmp' | Select-Object -Last 1
        if(-not $image){throw "Native capture missing: $($site.name)"}
        Copy-Item -LiteralPath $image.FullName -Destination (Join-Path $out ($site.name+'.bmp'))
        $provenance += [pscustomobject]@{site=$site;executable=$captureExe;binary_sha256=(Get-FileHash -LiteralPath $captureExe -Algorithm SHA256).Hash;image_sha256=(Get-FileHash -LiteralPath $image.FullName -Algorithm SHA256).Hash;arguments=$arguments}
        $provenance | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $out 'provenance.json')
    }
} finally {$env:VOXEL_FRONTIER_DATA_DIR=$prior}
Write-Output "Native captures: $out"
