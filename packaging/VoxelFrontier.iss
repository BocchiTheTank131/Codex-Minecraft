#ifndef AppVersion
  #error AppVersion must be supplied by package_release.ps1
#endif
#ifndef SourceExe
  #error SourceExe must be supplied by package_release.ps1
#endif
#ifndef OutputDir
  #error OutputDir must be supplied by package_release.ps1
#endif
#ifndef IconPath
  #error IconPath must be supplied by package_release.ps1
#endif
#ifndef AppId
  #define AppId "VoxelFrontier.BocchiTheTank131"
#endif
#ifndef AppDisplayName
  #define AppDisplayName "Voxel Frontier"
#endif
#ifndef ShortcutName
  #define ShortcutName "Voxel Frontier"
#endif
#ifndef DefaultInstallDir
  #define DefaultInstallDir "{localappdata}\Programs\Voxel Frontier"
#endif

[Setup]
AppId={#AppId}
AppName={#AppDisplayName}
AppVersion={#AppVersion}
AppVerName={#AppDisplayName} {#AppVersion}
AppPublisher=Voxel Frontier Project
AppPublisherURL=https://github.com/BocchiTheTank131/Codex-Minecraft
AppSupportURL=https://github.com/BocchiTheTank131/Codex-Minecraft/issues
AppUpdatesURL=https://github.com/BocchiTheTank131/Codex-Minecraft/releases
AppCopyright=Copyright (c) 2026 Voxel Frontier Project
DefaultDirName={#DefaultInstallDir}
DefaultGroupName={#ShortcutName}
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
UsePreviousTasks=yes
PrivilegesRequired=lowest
SetupArchitecture=x64
ArchitecturesAllowed=x64compatible
MinVersion=10.0
WizardStyle=modern
SetupIconFile={#IconPath}
UninstallDisplayName={#AppDisplayName}
UninstallDisplayIcon={app}\VoxelFrontier.exe
CloseApplications=yes
RestartApplications=no
SetupMutex=VoxelFrontierSetupMutex
Compression=lzma2
SolidCompression=yes
OutputDir={#OutputDir}
OutputBaseFilename=VoxelFrontier-v{#AppVersion}-Windows-Setup
VersionInfoVersion={#AppVersion}
VersionInfoProductVersion={#AppVersion}
VersionInfoProductName=Voxel Frontier
VersionInfoCompany=Voxel Frontier Project
VersionInfoDescription=Voxel Frontier Setup

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "VoxelFrontier.exe"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#ShortcutName}"; Filename: "{app}\VoxelFrontier.exe"; WorkingDir: "{app}"; IconFilename: "{app}\VoxelFrontier.exe"
Name: "{autodesktop}\{#ShortcutName}"; Filename: "{app}\VoxelFrontier.exe"; WorkingDir: "{app}"; IconFilename: "{app}\VoxelFrontier.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\VoxelFrontier.exe"; Description: "Launch Voxel Frontier"; Flags: nowait postinstall skipifsilent

[Code]
function InitializeSetup: Boolean;
var
  Key, InstalledText, UninstallCommand: String;
  Installed, Incoming: Int64;
  Choice, ResultCode: Integer;
begin
  Result := True;
  Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppId}_is1';
  if not RegQueryStringValue(HKCU, Key, 'DisplayVersion', InstalledText) then
    Exit;
  if not StrToVersion(InstalledText, Installed) or
     not StrToVersion('{#AppVersion}', Incoming) then
  begin
    if not WizardSilent then
      MsgBox('An existing Voxel Frontier installation has an unreadable version. ' +
             'Please uninstall it before continuing.', mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if ComparePackedVersion(Installed, Incoming) > 0 then
  begin
    if not WizardSilent then
      MsgBox('Voxel Frontier ' + InstalledText + ' is already installed. ' +
             'This older setup cannot downgrade it.', mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if ComparePackedVersion(Installed, Incoming) = 0 then
  begin
    if WizardSilent then Exit;  { Silent reruns repair the current installation. }
    Choice := MsgBox('Voxel Frontier ' + InstalledText + ' is already installed.' + #13#10 +
                     'Yes: Repair / Reinstall' + #13#10 +
                     'No: Uninstall' + #13#10 +
                     'Cancel: Leave the installation unchanged',
                     mbConfirmation, MB_YESNOCANCEL);
    if Choice = IDYES then Exit;
    if Choice = IDNO then
    begin
      if RegQueryStringValue(HKCU, Key, 'UninstallString', UninstallCommand) then
        Exec(ExpandConstant('{cmd}'), '/C ' + UninstallCommand, '', SW_SHOW,
             ewNoWait, ResultCode)
      else
        MsgBox('The existing uninstaller could not be found.', mbError, MB_OK);
    end;
    Result := False;
  end;
end;
