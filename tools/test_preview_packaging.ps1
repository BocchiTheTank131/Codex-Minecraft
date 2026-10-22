param([string]$Output='build/v2161-validation/preview-packaging.json')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
# Run the real packaging entry point in a disposable checkout-shaped fixture.
# These failures must happen before CMake, compilation or installer creation.
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('VoxelFrontier-preview-packaging-'+[guid]::NewGuid())
New-Item -ItemType Directory -Path (Join-Path $fixture 'assets') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'package_release.ps1') -Destination $fixture
Copy-Item -LiteralPath (Join-Path $root 'CMakeLists.txt') -Destination $fixture
$manifest=Join-Path $fixture 'assets/inventory_previews.json'
$results=@()
foreach($case in @('missing-manifest','stale-input','missing-output')){
    if($case -ne 'missing-manifest'){
        $path=if($case -eq 'stale-input'){'assets/stale.txt'}else{'assets/missing.txt'}
        if($case -eq 'stale-input'){Set-Content -LiteralPath (Join-Path $fixture $path) -Value 'changed'}
        $data=@{inputs=@{};outputs=@{}}
        $group=if($case -eq 'stale-input'){'inputs'}else{'outputs'}
        $data[$group][$path]='0000000000000000000000000000000000000000000000000000000000000000'
        $data | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifest
    }
    $failure=''
    try{& (Join-Path $fixture 'package_release.ps1') -Generator 'MUST-NOT-REACH-CMAKE'}catch{$failure=$_.Exception.Message}
    $expected=if($case -eq 'missing-manifest'){'Required inventory preview manifest is missing'}else{'Stale inventory preview input/output'}
    if(-not $failure.Contains($expected)){throw "Packaging preflight case $case failed: $failure"}
    if(Test-Path (Join-Path $fixture 'build')){throw 'Rejected previews reached build configuration'}
    $results+=[pscustomobject]@{case=$case;passed=$true;error=$failure}
}
$destination=Join-Path $root $Output
New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $destination
Write-Output "Preview packaging checks passed: $($results.Count). Isolated fixture retained: $fixture"
