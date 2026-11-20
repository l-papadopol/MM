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
; IC8TEM edition: an installation of its own, separate from the original MadModem
; (AppId {E9A289C9-3586-49BE-82A2-72594C56E2B8}), in C:\MadModem-tem; the user data
; is kept in its "data" folder (bin\local-data.flag).
AppId={{8FEB4792-929F-4406-B87A-79F263D7D1FF}
AppName=MadModem IC8TEM
AppVersion={#AppVersion}
AppPublisher=Costantino Cerrotta, IC8TEM
AppCopyright=Copyright (C) 2026 Papadopol Lucian-Ioan; IC8TEM edition: Costantino Cerrotta
AppPublisherURL=https://github.com/ic8tem/MM
DefaultDirName={sd}\MadModem-tem
DefaultGroupName=MadModem IC8TEM
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
Name: "{group}\MadModem IC8TEM"; Filename: "{app}\bin\MadModem.exe"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\MadModem IC8TEM"; Filename: "{app}\bin\MadModem.exe"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\MadModem.exe"; Description: "{cm:LaunchProgram,MadModem}"; Flags: nowait postinstall skipifsilent

[Code]
const
  // The original MadModem; builds 5 and 7 of the IC8TEM edition used its AppId too.
  OriginalUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{E9A289C9-3586-49BE-82A2-72594C56E2B8}_is1';

var
  Avx2Available: Boolean;
  OldEditionDir: String;
  OldEditionUninstaller: String;
  OldEditionRoot: Integer;

function FileNamesEdition(const FileName: String): Boolean;
var
  Text: AnsiString;
begin
  Result := LoadStringFromFile(FileName, Text) and (Pos('IC8TEM', String(Text)) > 0);
end;

// An earlier IC8TEM edition installed under the AppId of the original MadModem.
// Its README and THIRD_PARTY_NOTICES name the edition; the original is never touched.
procedure FindOldEdition;
var
  Dir, Uninstaller: String;
  Root: Integer;
begin
  OldEditionDir := '';
  for Root := 0 to 1 do begin
    if Root = 0 then OldEditionRoot := HKCU else OldEditionRoot := HKLM;
    if RegQueryStringValue(OldEditionRoot, OriginalUninstallKey, 'InstallLocation', Dir) and
       RegQueryStringValue(OldEditionRoot, OriginalUninstallKey, 'UninstallString', Uninstaller) then begin
      Dir := RemoveBackslashUnlessRoot(Dir);
      if FileNamesEdition(AddBackslash(Dir) + 'README.md') or
         FileNamesEdition(AddBackslash(Dir) + 'THIRD_PARTY_NOTICES.md') then begin
        OldEditionDir := Dir;
        OldEditionUninstaller := RemoveQuotes(Uninstaller);
        Log('Earlier IC8TEM edition found in ' + Dir);
        Exit;
      end;
    end;
  end;
end;

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
  // The updater of the earlier builds passes their folder with /DIR: the edition
  // now lives in C:\MadModem-tem, so it moves there.
  FindOldEdition;
  if (OldEditionDir <> '') and
     (CompareText(RemoveBackslashUnlessRoot(WizardForm.DirEdit.Text), OldEditionDir) = 0) then
    WizardForm.DirEdit.Text := ExpandConstant('{sd}\MadModem-tem');
end;

// Remove the earlier edition (program files, shortcuts, uninstall entry) before the
// new files are copied. Its user data stays where it is: MadModem moves it into
// the data folder at the first start.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode, Waited: Integer;
begin
  Result := '';
  if OldEditionDir = '' then Exit;
  Log('Removing the earlier IC8TEM edition in ' + OldEditionDir);
  if not Exec(OldEditionUninstaller, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_HIDE,
              ewWaitUntilTerminated, ResultCode) then begin
    Log('The old uninstaller could not start: ' + SysErrorMessage(ResultCode));
    Exit;
  end;
  Waited := 0;
  while RegKeyExists(OldEditionRoot, OriginalUninstallKey) and (Waited < 60000) do begin
    Sleep(500);
    Waited := Waited + 500;
  end;
  Log(Format('Old uninstaller finished with code %d', [ResultCode]));
end;
