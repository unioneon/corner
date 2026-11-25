#ifndef AppVersion
  #define AppVersion "1.2.0"
#endif

[Setup]
AppId={{4F7E218A-79F3-4B07-925B-237C269278AE}
AppName=corner
AppVersion={#AppVersion}
AppPublisher=unioneon
AppPublisherURL=https://github.com/unioneon/corner
AppSupportURL=https://github.com/unioneon/corner/issues
AppUpdatesURL=https://github.com/unioneon/corner/releases
DefaultDirName={localappdata}\corner
DefaultGroupName=corner
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
OutputDir=..\dist
OutputBaseFilename=corner-{#AppVersion}-Setup-x64
SetupIconFile=..\assets\corner-black.ico
UninstallDisplayIcon={app}\corner.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
WizardImageFile=..\docs\images\installer-side.bmp
WizardSmallImageFile=..\docs\images\installer-small.bmp
WizardImageStretch=yes
LicenseFile=..\LICENSE
InfoBeforeFile=welcome.txt
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "..\bin\corner.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\manage.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "ownership.marker"; DestDir: "{app}"; DestName: ".corner-install"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\MinGW-RUNTIME.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\corner"; Filename: "{app}\corner.exe"; Parameters: "--settings"; WorkingDir: "{app}"
Name: "{autodesktop}\corner"; Filename: "{app}\corner.exe"; Parameters: "--settings"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "{code:InstallParameters}"; Flags: runhidden waituntilterminated
Filename: "{app}\corner.exe"; Parameters: "--settings"; Description: "Open corner settings"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\manage.ps1"" uninstall -InstallDir ""{app}"""; Flags: runhidden waituntilterminated; RunOnceId: "RemoveCornerTask"; Check: HasManager

[Code]
function HasManager(): Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\manage.ps1'));
end;

function InstallParameters(Param: String): String;
begin
  Result := '-NoProfile -ExecutionPolicy Bypass -File "' + ExpandConstant('{app}\manage.ps1') + '" install -InstallDir "' + ExpandConstant('{app}') + '"';
  if not WizardIsTaskSelected('desktopicon') then Result := Result + ' -NoDesktopShortcut';
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Code: Integer;
begin
  Result := '';
  if FileExists(ExpandConstant('{app}\corner.exe')) then
    if not Exec(ExpandConstant('{app}\corner.exe'), '--stop',
      '', SW_HIDE, ewWaitUntilTerminated, Code) then
      Result := 'Please close corner before continuing.'
    else if Code <> 0 then
      Result := 'corner could not be stopped. Please close it and try again.'
    else Sleep(750);
end;
