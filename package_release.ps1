param(
    [string]$Generator = 'Visual Studio 18 2026'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmakeText = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
$match = [regex]::Match($cmakeText, 'project\(VoxelFrontier VERSION (\d+\.\d+\.\d+)')
if (-not $match.Success) { throw 'Could not read the project version from CMakeLists.txt.' }
$version = $match.Groups[1].Value
$fullBuildDir = Join-Path $root 'build/installed-release'
$demoBuildDir = Join-Path $root 'build/standalone-demo-release'
$distDir = Join-Path $root 'dist'
$icon = Join-Path $root 'packaging/VoxelFrontier.ico'
$standalone = Join-Path $distDir "VoxelFrontier-v$version-Windows-Standalone.exe"
$fullExe = Join-Path $fullBuildDir 'Release/VoxelFrontier.exe'
$setup = Join-Path $distDir "VoxelFrontier-v$version-Windows-Setup.exe"

if (-not (Test-Path -LiteralPath $icon)) { throw 'Windows icon is missing.' }
New-Item -ItemType Directory -Path $distDir -Force | Out-Null
foreach ($variant in @(
    @{ Path = $fullBuildDir; Demo = 'OFF'; Name = 'full installed game' },
    @{ Path = $demoBuildDir; Demo = 'ON'; Name = 'standalone demo' }
)) {
    $demoOption = "-DVOXELFRONTIER_STANDALONE_DEMO=$($variant.Demo)"
    & cmake -S $root -B $variant.Path -G $Generator -A x64 `
        -DVOXEL_STANDALONE=ON $demoOption
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed for $($variant.Name)." }
    & cmake --build $variant.Path --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw "Release build failed for $($variant.Name)." }
}
if (-not (Test-Path -LiteralPath $fullExe)) { throw 'Full game executable is missing.' }
$demoExe = Join-Path $demoBuildDir 'Release/VoxelFrontier.exe'
# Copy the binary as bytes without relying on the shell's native CopyFile path.
[System.IO.File]::WriteAllBytes($standalone, [System.IO.File]::ReadAllBytes($demoExe))

$compiler = @(
    (Join-Path $env:LOCALAPPDATA 'Programs/Inno Setup 7/ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 7/ISCC.exe'),
    (Get-Command ISCC.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
if (-not $compiler) {
    throw 'Inno Setup is required. Install JRSoftware.InnoSetup.7 with winget, then rerun this script.'
}
$compilerVersion = (& $compiler --version | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $compilerVersion -notmatch '^7\.') {
    throw 'Inno Setup 7 is required to compile the installer.'
}
$isccArgs = @(
    "--define=AppVersion=$version",
    "--define=SourceExe=$fullExe",
    "--define=OutputDir=$distDir",
    "--define=IconPath=$icon",
    (Join-Path $root 'packaging/VoxelFrontier.iss')
)
& $compiler @isccArgs
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $setup)) {
    throw 'Installer compilation failed.'
}

$checksums = @($standalone, $setup) | ForEach-Object {
    $hash = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $(Split-Path $_ -Leaf)"
}
Set-Content -LiteralPath (Join-Path $distDir 'SHA256SUMS.txt') -Value $checksums
Write-Output "Standalone: $standalone"
Write-Output "Installer: $setup"
Write-Output "Checksums: $(Join-Path $distDir 'SHA256SUMS.txt')"
