; Vertical Shorts Plugin — Inno Setup 6 (clean install)
;
; Permanent AppId (do not change across versions):
;   {D4336EAC-D873-4E6B-8575-07096987E0C8}
; Same GUID as buildspec.json → uuids.windowsApp
;
; {app} = OBS Studio root, for example:
;   C:\Program Files\obs-studio
;
; Destinations:
;   {app}\obs-plugins\64bit\obs-shorts-vertical.dll
;   {app}\data\obs-plugins\obs-shorts-vertical\
;
; This script installs the current plugin only.
; There is no updater, upgrade checker, upgrade backup, or upgrade dialog.
;
; Never ExpandConstant('{app}') inside InitializeSetup.

#ifndef MyAppName
  #define MyAppName "Vertical Shorts Plugin"
#endif
#ifndef MyAppVersion
  #define MyAppVersion "1.0.10"
#endif
#ifndef MyAppPublisher
  #define MyAppPublisher "Vertical Shorts Plugin Contributors"
#endif
#ifndef MyAppURL
  #define MyAppURL "https://github.com/PFS2689/verticalshorts"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\release\staging"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\release"
#endif
#ifndef OutputBaseFilename
  #define OutputBaseFilename "Vertical-Shorts-Plugin-1.0.10-Setup"
#endif

#define MyAppIdGuid "D4336EAC-D873-4E6B-8575-07096987E0C8"

[Setup]
; {{ escapes to a single { → AppId={GUID}
AppId={{{#MyAppIdGuid}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={code:GetDefaultDirName}
UsePreviousAppDir=no
DisableProgramGroupPage=yes
DisableDirPage=no
DirExistsWarning=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName} {#MyAppVersion}
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=Professional Vertical Streaming Plugin for OBS Studio
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCopyright=Copyright (C) Vertical Shorts Plugin Contributors
AllowNoIcons=yes
CloseApplications=no
RestartApplications=no
RestartIfNeededByRun=no
CreateUninstallRegKey=yes
AllowCancelDuringInstall=yes
UsedUserAreasWarning=no
AlwaysRestart=no
SetupLogging=yes
MinVersion=10.0
DisableWelcomePage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupWindowTitle=Setup — {#MyAppName} {#MyAppVersion}
SelectDirLabel3=Select your OBS Studio installation folder (must contain bin\64bit\obs64.exe).
SelectDirBrowseLabel=Select the OBS Studio folder, then click Next.

[Files]
; Plugin DLL → OBS obs-plugins\64bit
Source: "{#SourceDir}\obs-shorts-vertical\bin\64bit\obs-shorts-vertical.dll"; \
    DestDir: "{app}\obs-plugins\64bit"; \
    Flags: ignoreversion uninsrestartdelete
; Plugin resources → OBS data\obs-plugins\obs-shorts-vertical\
Source: "{#SourceDir}\obs-shorts-vertical\data\*"; \
    DestDir: "{app}\data\obs-plugins\obs-shorts-vertical"; \
    Flags: ignoreversion recursesubdirs createallsubdirs uninsrestartdelete
Source: "{#SourceDir}\obs-shorts-vertical\install-meta.ini"; \
    DestDir: "{app}\data\obs-plugins\obs-shorts-vertical"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\obs-shorts-vertical\INSTALL.txt"; \
    DestDir: "{app}\data\obs-plugins\obs-shorts-vertical"; \
    Flags: ignoreversion skipifsourcedoesntexist

[Run]
Filename: "{app}\bin\64bit\obs64.exe"; Description: "Launch OBS Studio"; \
    Flags: nowait postinstall skipifsilent unchecked; Check: ObsExeExists

[UninstallDelete]
; Only this plugin's files — never OBS core files.
Type: files; Name: "{app}\obs-plugins\64bit\obs-shorts-vertical.dll"
Type: files; Name: "{app}\obs-plugins\64bit\obs-shorts-vertical.pdb"
Type: filesandordirs; Name: "{app}\data\obs-plugins\obs-shorts-vertical"

[Code]
const
  OBS_WINDOW_CLASS = 'OBSWindowClass';
  WIN_GENERIC_READ = $80000000;
  WIN_GENERIC_WRITE = $40000000;
  WIN_OPEN_EXISTING = 3;
  WIN_FILE_ATTRIBUTE_NORMAL = $80;
  WIN_INVALID_HANDLE_VALUE = $FFFFFFFF;

var
  GObsInstallPath: String;
  GInstallLogPath: String;
  GInstallLogDir: String;

function CreateFileW(lpFileName: String; dwDesiredAccess, dwShareMode: Cardinal;
  lpSecurityAttributes: Cardinal; dwCreationDisposition, dwFlagsAndAttributes: Cardinal;
  hTemplateFile: Cardinal): Cardinal;
  external 'CreateFileW@kernel32.dll stdcall';

function CloseHandle(hObject: Cardinal): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function AddBackslashIfNeeded(const Path: String): String;
begin
  Result := Path;
  if (Result <> '') and (Result[Length(Result)] <> '\') then
    Result := Result + '\';
end;

function IsValidObsDir(const Dir: String): Boolean;
begin
  Result := (Dir <> '') and
            DirExists(Dir) and
            FileExists(AddBackslashIfNeeded(Dir) + 'bin\64bit\obs64.exe') and
            DirExists(AddBackslashIfNeeded(Dir) + 'obs-plugins\64bit') and
            DirExists(AddBackslashIfNeeded(Dir) + 'data\obs-plugins');
end;

function ObsExeExists: Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\bin\64bit\obs64.exe'));
end;

function QueryRegInstallLocation(const SubKey: String; var OutValue: String): Boolean;
begin
  Result := False;
  OutValue := '';
  if RegQueryStringValue(HKLM64, SubKey, 'InstallLocation', OutValue) then begin
    Result := True;
    exit;
  end;
  if RegQueryStringValue(HKLM, SubKey, 'InstallLocation', OutValue) then begin
    Result := True;
    exit;
  end;
  if RegQueryStringValue(HKCU, SubKey, 'InstallLocation', OutValue) then
    Result := True;
end;

procedure AppendInstallLog(const Line: String);
var
  Stamp: String;
begin
  if GInstallLogPath = '' then
    exit;
  Stamp := GetDateTimeString('yyyy-mm-dd hh:nn:ss', #0, #0);
  SaveStringToFile(GInstallLogPath, Stamp + '  ' + Line + #13#10, True);
end;

procedure InitInstallLog;
var
  Stamp: String;
begin
  GInstallLogDir := ExpandConstant('{localappdata}\VerticalShortsPlugin\logs');
  ForceDirectories(GInstallLogDir);
  Stamp := GetDateTimeString('yyyymmdd_hhnnss', #0, #0);
  GInstallLogPath := AddBackslashIfNeeded(GInstallLogDir) + 'install-' + Stamp + '.log';
  SaveStringToFile(GInstallLogPath,
    'Vertical Shorts Plugin installer log'#13#10 +
    '===================================='#13#10 +
    'Product={#MyAppName}'#13#10 +
    'InstallerVersion={#MyAppVersion}'#13#10 +
    'AppId={' + '{#MyAppIdGuid}' + '}'#13#10 +
    'Architecture=x64'#13#10 +
    'Mode=clean-install'#13#10 +
    'LogFile=' + GInstallLogPath + #13#10 +
    'Note=This log never contains stream keys, credentials, or passwords.'#13#10#13#10,
    False);
  AppendInstallLog('Log initialized');
end;

function DetectObsInstallPath: String;
var
  Candidate, RegVal: String;
begin
  Result := '';

  if RegQueryStringValue(HKLM64, 'Software\OBS Studio', '', RegVal) and IsValidObsDir(RegVal) then begin
    Result := RemoveBackslash(RegVal);
    exit;
  end;
  if RegQueryStringValue(HKLM, 'Software\OBS Studio', '', RegVal) and IsValidObsDir(RegVal) then begin
    Result := RemoveBackslash(RegVal);
    exit;
  end;
  if QueryRegInstallLocation('Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio', RegVal) and IsValidObsDir(RegVal) then begin
    Result := RemoveBackslash(RegVal);
    exit;
  end;

  Candidate := ExpandConstant('{autopf}\obs-studio');
  if IsValidObsDir(Candidate) then begin
    Result := Candidate;
    exit;
  end;

  Candidate := ExpandConstant('{pf}\obs-studio');
  if IsValidObsDir(Candidate) then begin
    Result := Candidate;
    exit;
  end;

  Candidate := 'C:\Program Files\obs-studio';
  if IsValidObsDir(Candidate) then
    Result := Candidate;
end;

function GetDefaultDirName(Param: String): String;
begin
  Result := DetectObsInstallPath;
  if Result = '' then
    Result := ExpandConstant('{autopf}\obs-studio');
end;

function IsOBSRunning: Boolean;
begin
  Result := (FindWindowByClassName(OBS_WINDOW_CLASS) <> 0) or
            CheckForMutexes('OBSStudioRunningMutex') or
            CheckForMutexes('OBS32RunningMutex');
end;

function IsFileLocked(const FileName: String): Boolean;
var
  H: Cardinal;
begin
  Result := False;
  if (FileName = '') or (not FileExists(FileName)) then
    exit;
  H := CreateFileW(FileName, WIN_GENERIC_READ or WIN_GENERIC_WRITE, 0, 0,
                   WIN_OPEN_EXISTING, WIN_FILE_ATTRIBUTE_NORMAL, 0);
  if H = WIN_INVALID_HANDLE_VALUE then
    Result := True
  else
    CloseHandle(H);
end;

function EnsureDllUnlocked(const DllPath: String): Boolean;
var
  Answer: Integer;
  NeedObs, NeedDll: Boolean;
  Msg: String;
begin
  Result := True;
  while True do begin
    NeedObs := IsOBSRunning and FileExists(DllPath);
    NeedDll := (DllPath <> '') and FileExists(DllPath) and IsFileLocked(DllPath);

    if (not NeedObs) and (not NeedDll) then begin
      AppendInstallLog('Ready to copy plugin files');
      exit;
    end;

    if NeedObs then
      Msg :=
        'OBS Studio must be closed before Vertical Shorts Plugin can be installed.'#13#10#13#10 +
        'Please close OBS Studio completely, then click Retry.'#13#10#13#10 +
        'Setup will not force-quit OBS.'
    else
      Msg :=
        'The Vertical Shorts plugin file is currently in use and cannot be replaced.'#13#10#13#10 +
        DllPath + #13#10#13#10 +
        'Close OBS Studio (and any tool locking the DLL), then click Retry.';

    AppendInstallLog('Waiting for unlock: OBSRunning=' + IntToStr(Ord(IsOBSRunning)) +
                     ' DllLocked=' + IntToStr(Ord(NeedDll)));

    Answer := MsgBox(Msg, mbConfirmation, MB_RETRYCANCEL);
    if Answer = IDCANCEL then begin
      AppendInstallLog('User cancelled while waiting for OBS/DLL unlock');
      Result := False;
      exit;
    end;
    Sleep(500);
  end;
end;

procedure RemoveObsoletePluginCopies;
var
  P: String;
begin
  { Only remove known obsolete copies of THIS plugin — never other OBS plugins. }
  P := ExpandConstant('{app}\obs-plugins\64bit\obs-shorts-vertical.pdb');
  if FileExists(P) then
    DeleteFile(P);

  P := ExpandConstant('{app}\bin\64bit\obs-shorts-vertical.dll');
  if FileExists(P) then
    DeleteFile(P);

  P := ExpandConstant('{commonappdata}\obs-studio\plugins\obs-shorts-vertical');
  if DirExists(P) then
    DelTree(P, True, True, True);

  P := ExpandConstant('{userappdata}\obs-studio\plugins\obs-shorts-vertical');
  if DirExists(P) then
    DelTree(P, True, True, True);
end;

function VerifyInstalledPayload: Boolean;
var
  DllPath, LocalePath, DataDir: String;
begin
  Result := True;
  DllPath := ExpandConstant('{app}\obs-plugins\64bit\obs-shorts-vertical.dll');
  DataDir := ExpandConstant('{app}\data\obs-plugins\obs-shorts-vertical');
  LocalePath := DataDir + '\locale\en-US.ini';

  if not FileExists(DllPath) then begin
    AppendInstallLog('VERIFY FAIL: missing DLL ' + DllPath);
    Result := False;
  end else
    AppendInstallLog('VERIFY OK: DLL ' + DllPath);

  if not DirExists(DataDir) then begin
    AppendInstallLog('VERIFY FAIL: missing data dir ' + DataDir);
    Result := False;
  end else
    AppendInstallLog('VERIFY OK: data dir ' + DataDir);

  if not FileExists(LocalePath) then begin
    AppendInstallLog('VERIFY FAIL: missing locale ' + LocalePath);
    Result := False;
  end else
    AppendInstallLog('VERIFY OK: locale ' + LocalePath);
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  InitInstallLog;

  if not IsWin64 then begin
    MsgBox(
      'Vertical Shorts Plugin requires 64-bit Windows and 64-bit OBS Studio.'#13#10#13#10 +
      'This installer cannot continue on a 32-bit system.',
      mbError, MB_OK);
    AppendInstallLog('ABORT: not Win64');
    Result := False;
    exit;
  end;

  GObsInstallPath := DetectObsInstallPath;
  AppendInstallLog('Detected OBS path: ' + GObsInstallPath);

  if GObsInstallPath = '' then begin
    MsgBox(
      'OBS Studio could not be found automatically.'#13#10#13#10 +
      'On the next page, select your OBS Studio installation folder.'#13#10 +
      'It must contain bin\64bit\obs64.exe (for example C:\Program Files\obs-studio).',
      mbInformation, MB_OK);
  end;

  { Never ask Upgrade/Cancel. Same AppId simply replaces plugin files. }
  if IsOBSRunning then begin
    AppendInstallLog('OBS is running at setup start');
    MsgBox(
      'OBS Studio is currently running.'#13#10#13#10 +
      'Setup can continue. Close OBS before file copy if the plugin DLL is ' +
      'locked, and restart OBS afterward for the plugin to load.'#13#10#13#10 +
      'Click OK to continue.',
      mbInformation, MB_OK);
  end;
end;

procedure InitializeWizard;
begin
  if GObsInstallPath <> '' then
    WizardForm.DirEdit.Text := GObsInstallPath
  else if not IsValidObsDir(WizardDirValue) then
    WizardForm.DirEdit.Text := ExpandConstant('{autopf}\obs-studio');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then begin
    if not IsValidObsDir(WizardDirValue) then begin
      MsgBox(
        'The selected folder does not contain a valid OBS Studio installation.'#13#10#13#10 +
        'It must contain:'#13#10 +
        '  bin\64bit\obs64.exe'#13#10 +
        '  obs-plugins\64bit\'#13#10 +
        '  data\obs-plugins\'#13#10#13#10 +
        'Example:'#13#10 +
        '  C:\Program Files\obs-studio',
        mbError, MB_OK);
      AppendInstallLog('Invalid OBS folder selected: ' + WizardDirValue);
      Result := False;
    end else begin
      GObsInstallPath := WizardDirValue;
      AppendInstallLog('User confirmed OBS folder: ' + GObsInstallPath);
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  TargetDll: String;
begin
  NeedsRestart := False;
  Result := '';

  if not IsValidObsDir(ExpandConstant('{app}')) then begin
    Result := 'The selected folder does not contain a valid OBS Studio installation (missing bin\64bit\obs64.exe).';
    AppendInstallLog('PrepareToInstall aborted: invalid OBS dir');
    exit;
  end;

  TargetDll := ExpandConstant('{app}\obs-plugins\64bit\obs-shorts-vertical.dll');
  AppendInstallLog('PrepareToInstall target DLL: ' + TargetDll);

  if not EnsureDllUnlocked(TargetDll) then begin
    Result := 'OBS Studio must be closed before Vertical Shorts Plugin can be installed. Please close OBS Studio and click Retry.';
    AppendInstallLog('PrepareToInstall aborted: DLL still locked');
    exit;
  end;

  RemoveObsoletePluginCopies;
  AppendInstallLog('PrepareToInstall ready');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  MetaPath: String;
begin
  if CurStep = ssInstall then
    AppendInstallLog('File copy starting');

  if CurStep = ssPostInstall then begin
    AppendInstallLog('Post-install: writing metadata + verifying payload');
    MetaPath := ExpandConstant('{app}\data\obs-plugins\obs-shorts-vertical\install-meta.ini');
    SetIniString('Install', 'DisplayName', '{#MyAppName}', MetaPath);
    SetIniString('Install', 'DisplayVersion', '{#MyAppVersion}', MetaPath);
    SetIniString('Install', 'AppId', '{' + '{#MyAppIdGuid}' + '}', MetaPath);
    SetIniString('Install', 'InstallDir', ExpandConstant('{app}'), MetaPath);
    SetIniString('Install', 'PluginDll',
      ExpandConstant('{app}\obs-plugins\64bit\obs-shorts-vertical.dll'), MetaPath);
    SetIniString('Install', 'PluginData',
      ExpandConstant('{app}\data\obs-plugins\obs-shorts-vertical'), MetaPath);
    SetIniString('Install', 'InstalledTimestampUtc',
      GetDateTimeString('yyyy-mm-dd"T"hh:nn:ss"Z"', #0, #0), MetaPath);
    SetIniString('Install', 'InstallLog', GInstallLogPath, MetaPath);
    SetIniString('Install', 'Notes',
      'Clean install. DLL under obs-plugins\64bit; data under data\obs-plugins\obs-shorts-vertical.', MetaPath);

    if not VerifyInstalledPayload then
      MsgBox(
        'Installation finished, but verification found missing Vertical Shorts files.'#13#10#13#10 +
        'Check the install log:'#13#10 + GInstallLogPath,
        mbError, MB_OK)
    else
      AppendInstallLog('Installation verification PASSED');

    AppendInstallLog('Final result: SUCCESS');
  end;
end;
