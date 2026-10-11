#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef PayloadRoot
  #error PayloadRoot is required
#endif
#ifndef OutputRoot
  #error OutputRoot is required
#endif
[Setup]
AppId={{E9A289C9-3586-49BE-82A2-72594C56E2B8}
AppName=MadModem
AppVersion={#AppVersion}
AppPublisher=Lucian-Ioan Papadopol
AppCopyright=Copyright (C) 2026 Papadopol Lucian-Ioan
AppPublisherURL=https://www.madexp.it
DefaultDirName={localappdata}\Programs\MadModem
DefaultGroupName=MadModem
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
OutputDir={#OutputRoot}
OutputBaseFilename=MadModem-{#AppVersion}-Windows-x64-Setup
SetupIconFile=..\..\icons\madmodem.ico
UninstallDisplayIcon={app}\bin\MadModem.exe
LicenseFile={#PayloadRoot}\INSTALLER_LICENSE.txt
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PayloadRoot}\cpu-probe.exe"; Flags: dontcopy
Source: "{#PayloadRoot}\legacy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: UseLegacy
Source: "{#PayloadRoot}\avx2\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: UseAvx2

[Icons]
Name: "{group}\MadModem"; Filename: "{app}\bin\MadModem.exe"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\MadModem"; Filename: "{app}\bin\MadModem.exe"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\MadModem.exe"; Description: "{cm:LaunchProgram,MadModem}"; Flags: nowait postinstall skipifsilent

[Code]
var
  Avx2Available: Boolean;

function UseAvx2: Boolean;
begin
  Result := Avx2Available;
end;

function UseLegacy: Boolean;
begin
  Result := not Avx2Available;
end;

procedure InitializeWizard;
var
  ExitCode: Integer;
begin
  Avx2Available := False;
  ExtractTemporaryFile('cpu-probe.exe');
  if Exec(ExpandConstant('{tmp}\cpu-probe.exe'), '', '', SW_HIDE,
          ewWaitUntilTerminated, ExitCode) then
    Avx2Available := ExitCode = 0;
  if Avx2Available then Log('Selected AVX2/FMA build')
  else Log('Selected portable legacy build');
end;
