param([string]$Output='build/v2142-validation/installer-check')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'));$out=Join-Path $root $Output
if(Test-Path $out){$out=Join-Path $out ([guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force $out | Out-Null
$install=Join-Path $out 'installed';$testProfile=Join-Path $out 'profile';New-Item -ItemType Directory -Force $testProfile | Out-Null
New-Item -ItemType File -Path (Join-Path $testProfile '.voxel_test_sandbox') -Force | Out-Null
$id='VFStability2142.'+[guid]::NewGuid().ToString('N');$name='VFStabilityFixture'+[guid]::NewGuid().ToString('N').Substring(0,8)
$testKey="HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\${id}_is1"
if(Test-Path $testKey){throw 'Test installer ID collision'}
$productionKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\VoxelFrontier.BocchiTheTank131_is1'
function ProductionState {if(Test-Path $productionKey){Get-ItemProperty $productionKey | Select-Object DisplayVersion,InstallLocation | ConvertTo-Json -Compress}else{''}}
function ProfileState {Get-ChildItem $testProfile -File -Recurse | Where-Object {$_.Extension -in @('.vxw','.vxi','.vps','.vxm','.vtn') -or $_.Name -in @('world.info','world_seed.txt','checkpoint.manifest')} | Sort-Object FullName | ForEach-Object {($_.FullName.Substring($testProfile.Length))+':'+(Get-FileHash $_.FullName -Algorithm SHA256).Hash}}
$beforeProduction=ProductionState
$compiler='C:/Users/Win/AppData/Local/Programs/Inno Setup 7/ISCC.exe'
$prior=$env:VOXEL_FRONTIER_DATA_DIR
try {
 foreach($version in @('2.14.1','2.14.2')){
  $source=if($version -eq '2.14.1'){Join-Path $root 'build/v2142-validation/baseline/VoxelFrontier-v2.14.1.exe'}else{Join-Path $root 'build/installed-release/Release/VoxelFrontier.exe'}
  $destination=Join-Path $out $version;New-Item -ItemType Directory -Force $destination | Out-Null
  $compilerArguments=@("--define=AppVersion=$version","--define=AppDisplayVersion=$version-test","--define=SourceExe=$source","--define=OutputDir=$destination","--define=IconPath=$(Join-Path $root 'packaging/VoxelFrontier.ico')","--define=AppId=$id","--define=AppDisplayName=$name","--define=ShortcutName=$name","--define=DefaultInstallDir=$install",(Join-Path $root 'packaging/VoxelFrontier.iss'))
  & $compiler @compilerArguments *> (Join-Path $out "compile-$version.log");if($LASTEXITCODE){throw "Compile $version failed"}
  $setup=(Get-ChildItem $destination -Filter '*Setup.exe').FullName
  $p=Start-Process $setup -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-','/NOICONS',"/DIR=`"$install`"","/LOG=`"$(Join-Path $out "install-$version.log")`"") -WindowStyle Hidden -PassThru -Wait
  if($p.ExitCode){throw "Install $version failed: $($p.ExitCode)"}
  if((Get-FileHash (Join-Path $install 'VoxelFrontier.exe')).Hash -ne (Get-FileHash $source).Hash){throw 'Installer payload hash mismatch'}
  if((Get-ItemProperty $testKey).DisplayVersion -ne $version){throw 'Upgrade version registration mismatch'}
  if($version -eq '2.14.2' -and (Compare-Object $beforeUpgrade (ProfileState))){throw 'Upgrade modified isolated save files'}
  Set-Content (Join-Path $testProfile 'voxel_settings.cfg') 'render_distance=2';$env:VOXEL_FRONTIER_DATA_DIR=$testProfile
  & (Join-Path $install 'VoxelFrontier.exe') --menu-smoke | Out-Null
  if($LASTEXITCODE){throw "Installed $version menu/save regression failed"}
  $beforeUpgrade=ProfileState
 }
 $beforeUninstall=ProfileState
 $uninstaller=(Resolve-Path (Join-Path $install 'unins000.exe')).Path
 $expectedPrefix=[IO.Path]::GetFullPath((Join-Path $root 'build'))+[IO.Path]::DirectorySeparatorChar
 if(-not $uninstaller.StartsWith($expectedPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Uninstaller outside intended test workspace'}
 $p=Start-Process $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$(Join-Path $out 'uninstall.log')`"") -WindowStyle Hidden -PassThru -Wait
 if($p.ExitCode -or (Test-Path $testKey) -or (Test-Path (Join-Path $install 'VoxelFrontier.exe'))){throw 'Test installation cleanup failed'}
 if(Compare-Object $beforeUninstall (ProfileState)){throw 'Uninstall changed isolated saves'}
 if((ProductionState) -ne $beforeProduction){throw 'Production installation registration changed'}
 [pscustomobject]@{app_id=$id;upgrade_payloads_verified=$true;upgrade_preserved_saves=$true;legacy_worlds_loaded=$true;uninstall_preserved_saves=$true;production_registry_unchanged=$true;test_registration_removed=$true} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json')
 Write-Output "Isolated installer upgrade passed: $out"
} finally {$env:VOXEL_FRONTIER_DATA_DIR=$prior}
