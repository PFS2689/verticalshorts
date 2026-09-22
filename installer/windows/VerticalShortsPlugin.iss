; Vertical Shorts Plugin — Inno Setup 6 (clean-machine hardened)
;
; PERMANENT AppId (never change across versions):
;   {D4336EAC-D873-4E6B-8575-07096987E0C8}
; Same GUID as buildspec.json → uuids.windowsApp
;
; Install root is the OBS Studio directory ({app} = OBS root), for example:
;   C:\Program Files\obs-studio
;
; Plugin destinations (valid only after {app} is initialized):
;   {app}\obs-plugins\64bit\obs-shorts-vertical.dll
;   {app}\data\obs-plugins\obs-shorts-vertical\
;
; IMPORTANT: Never ExpandConstant('{app}') inside InitializeSetup (or any
; pre-directory-init path). That raises:
;   Internal error: An attempt was made to expand the "{app}" constant
;   before it was initialized.
;
; Runtime dependencies: this OBS UI plugin links against OBS-provided
; libobs / obs-frontend-api / Qt6. Do NOT ship nested Qt or obs.dll copies.
; VC++ CRT is provided by OBS Studio's own install.
;
; User configuration is NOT stored under {app}. It lives in the OBS scene
; collection ("obs-shorts-vertical") + Windows Credential Manager.

#ifndef MyAppName
  #define MyAppName "Vertical Shorts Plugin"
#endif
#ifndef MyAppVersion
  #define MyAppVersion "1.0.7"
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
  #define OutputBaseFilename "Vertical-Shorts-Plugin-1.0.7-Setup"
#endif

; Permanent product identity — DO NOT regenerate when bumping MyAppVersion.
#define MyAppIdGuid "D4336EAC-D873-4E6B-8575-07096987E0C8"

[Setup]
; Double-brace escapes to a single brace in the compiled script → {GUID}
AppId={{{#MyAppIdGuid}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
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
UpdateUninstallLogAppName=yes
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
; DLL → OBS obs-plugins\64bit
; ignoreversion: always replace (same-version reinstall / upgrade)
; replacesameversion: also replace when PE versions match
Source: "{#SourceDir}\obs-shorts-vertical\bin\64bit\obs-shorts-vertical.dll"; \
    DestDir: "{app}\obs-plugins\64bit"; \
    Flags: ignoreversion uninsrestartdelete
; Resources → OBS data\obs-plugins\obs-shorts-vertical\
Source: "{#SourceDir}\obs-shorts-vertical\data\*"; \
    DestDir: "{app}\data\obs-plugins\obs-shorts-vertical"; \
    Flags: ignoreversion recursesubdirs createallsubdirs uninsrestartdelete
Source: "{#SourceDir}\obs-shorts-vertical\install-meta.ini"; \
    DestDir: "{app}\data\obs-plugins\obs-shorts-vertical"; \
    Flags: ignoreversion
Source: "{#SourceDir}\obs-shorts-vertical\INSTALL.txt"; \
    DestDir: "{app}\data\obs-plugins\obs-shorts-vertical"; \
    Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName} Install Log"; Filename: "{code:GetInstallLogPath}"; WorkingDir: "{code:GetInstallLogDir}"; Flags: preventpinning uninsneveruninstall

[Run]
Filename: "{app}\bin\64bit\obs64.exe"; Description: "Launch OBS Studio"; \
    Flags: nowait postinstall skipifsilent unchecked; Check: ObsExeExists

[UninstallDelete]
Type: files; Name: "{app}\obs-plugins\64bit\obs-shorts-vertical.dll"
Type: files; Name: "{app}\obs-plugins\64bit\obs-shorts-vertical.pdb"
Type: filesandordirs; Name: "{app}\data\obs-plugins\obs-shorts-vertical"

[Code]
const
  OBS_WINDOW_CLASS = 'OBSWindowClass';
  { Avoid Windows/Inno predefined names (FILE_ATTRIBUTE_NORMAL, etc.). }
  WIN_GENERIC_READ = $80000000;
  WIN_GENERIC_WRITE = $40000000;
  WIN_OPEN_EXISTING = 3;
  WIN_FILE_ATTRIBUTE_NORMAL = $80;
  WIN_INVALID_HANDLE_VALUE = $FFFFFFFF;

var
  GIsUpgrade: Boolean;
  GPreviousVersion: String;
  GUpgradeBackupDir: String;
  GObsInstallPath: String;
  GExistingPluginDll: String;
  GInstallLogPath: String;
  GInstallLogDir: String;

function CreateFileW(lpFileName: String; dwDesiredAccess, dwShareMode: Cardinal;
  lpSecurityAttributes: Cardinal; dwCreationDisposition, dwFlagsAndAttributes: Cardinal;
  hTemplateFile: Cardinal): Cardinal;
  external 'CreateFileW@kernel32.dll stdcall';

function CloseHandle(hObject: Cardinal): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function WinGetLastError: Cardinal;
  external 'GetLastError@kernel32.dll stdcall';

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

function InnoUninstallRegKey: String;
begin
  Result := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{' +
            '{#MyAppIdGuid}' + '}_is1';
end;

function LegacyUninstallRegKey: String;
begin
  Result := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{' +
            '{#MyAppIdGuid}' + '}';
end;

function QueryUninstallString(const SubKey, ValueName: String; var OutValue: String): Boolean;
begin
  Result := False;
  OutValue := '';
  if RegQueryStringValue(HKLM64, SubKey, ValueName, OutValue) then begin
    Result := True;
    exit;
  end;
  if RegQueryStringValue(HKLM, SubKey, ValueName, OutValue) then begin
    Result := True;
    exit;
  end;
  if RegQueryStringValue(HKCU, SubKey, ValueName, OutValue) then
    Result := True;
end;

function GetInstallLogDir(Param: String): String;
begin
  if GInstallLogDir <> '' then
    Result := GInstallLogDir
  else
    Result := ExpandConstant('{localappdata}\VerticalShortsPlugin\logs');
end;

function GetInstallLogPath(Param: String): String;
begin
  if GInstallLogPath <> '' then
    Result := GInstallLogPath
  else
    Result := AddBackslashIfNeeded(GetInstallLogDir('')) + 'install-latest.log';
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
    'LogFile=' + GInstallLogPath + #13#10 +
    'Note=This log never contains stream keys, credentials, or passwords.'#13#10#13#10,
    False);
  AppendInstallLog('Log initialized');
  AppendInstallLog('Windows version: ' + GetWindowsVersionString);
end;

(* OBS detection — safe before app constant is initialized *)

function DetectObsInstallPath: String;
var
  Candidate, RegVal: String;
begin
  Result := '';

  if QueryUninstallString(InnoUninstallRegKey, 'InstallLocation', RegVal) and IsValidObsDir(RegVal) then begin
    Result := RemoveBackslash(RegVal);
    exit;
  end;
  if QueryUninstallString(LegacyUninstallRegKey, 'InstallLocation', RegVal) and IsValidObsDir(RegVal) then begin
    Result := RemoveBackslash(RegVal);
    exit;
  end;

  if RegQueryStringValue(HKLM64, 'Software\OBS Studio', '', RegVal) and IsValidObsDir(RegVal) then begin
    Result := RemoveBackslash(RegVal);
    exit;
  end;
  if RegQueryStringValue(HKLM, 'Software\OBS Studio', '', RegVal) and IsValidObsDir(RegVal) then begin
    Result := RemoveBackslash(RegVal);
    exit;
  end;
  if QueryUninstallString('Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio', 'InstallLocation', RegVal) and IsValidObsDir(RegVal) then begin
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

  { Common alternate roots (portable / custom). }
  Candidate := 'C:\Program Files\obs-studio';
  if IsValidObsDir(Candidate) then begin
    Result := Candidate;
    exit;
  end;
end;

function GetDefaultDirName(Param: String): String;
begin
  Result := DetectObsInstallPath;
  if Result = '' then
    Result := ExpandConstant('{autopf}\obs-studio');
end;

function GetInstalledVersionFromRegistry: String;
var
  Ver: String;
begin
  Result := '';
  if QueryUninstallString(InnoUninstallRegKey, 'DisplayVersion', Ver) then
    Result := Ver
  else if QueryUninstallString(LegacyUninstallRegKey, 'DisplayVersion', Ver) then
    Result := Ver;
end;

function GetInstalledVersionFromMetaFile(const MetaPath: String): String;
begin
  Result := '';
  if FileExists(MetaPath) then
    Result := GetIniString('Install', 'DisplayVersion', '', MetaPath);
end;

function FindExistingPluginDll: String;
var
  ObsRoot, Candidate, RegVal: String;
begin
  Result := '';

  ObsRoot := GObsInstallPath;
  if ObsRoot = '' then
    ObsRoot := ExpandConstant('{autopf}\obs-studio');
  Candidate := AddBackslashIfNeeded(ObsRoot) + 'obs-plugins\64bit\obs-shorts-vertical.dll';
  if FileExists(Candidate) then begin
    Result := Candidate;
    exit;
  end;

  Candidate := ExpandConstant('{autopf}\obs-studio\obs-plugins\64bit\obs-shorts-vertical.dll');
  if FileExists(Candidate) then begin
    Result := Candidate;
    exit;
  end;

  if QueryUninstallString(InnoUninstallRegKey, 'InstallLocation', RegVal) then begin
    Candidate := AddBackslashIfNeeded(RegVal) + 'obs-plugins\64bit\obs-shorts-vertical.dll';
    if FileExists(Candidate) then begin
      Result := Candidate;
      exit;
    end;
    Candidate := AddBackslashIfNeeded(RegVal) + 'bin\64bit\obs-shorts-vertical.dll';
    if FileExists(Candidate) then begin
      Result := Candidate;
      exit;
    end;
  end;

  { Obsolete paths from OUR earlier installer layouts only. }
  Candidate := ExpandConstant('{commonappdata}\obs-studio\plugins\obs-shorts-vertical\bin\64bit\obs-shorts-vertical.dll');
  if FileExists(Candidate) then
    Result := Candidate;
end;

function DetectPreviousVersion: String;
var
  Ver, DllPath, MetaPath, Root: String;
begin
  Ver := GetInstalledVersionFromRegistry;

  DllPath := FindExistingPluginDll;
  if (Ver = '') and (DllPath <> '') then begin
    if Pos('\obs-plugins\', LowerCase(DllPath)) > 0 then begin
      Root := ExtractFileDir(ExtractFileDir(ExtractFileDir(DllPath)));
      MetaPath := AddBackslashIfNeeded(Root) + 'data\obs-plugins\obs-shorts-vertical\install-meta.ini';
    end else begin
      Root := ExtractFileDir(ExtractFileDir(ExtractFileDir(DllPath)));
      MetaPath := AddBackslashIfNeeded(Root) + 'install-meta.ini';
    end;
    Ver := GetInstalledVersionFromMetaFile(MetaPath);
  end;

  Result := Ver;
end;

function IsUpgradeInstall: Boolean;
begin
  Result := (DetectPreviousVersion <> '') or (FindExistingPluginDll <> '');
end;

function CompareVersionParts(const A, B: String): Integer;
var
  AMaj, AMin, APat, BMaj, BMin, BPat: Int64;
  ARest, BRest: String;
begin
  ARest := A;
  BRest := B;
  AMaj := StrToIntDef(Copy(ARest, 1, Pos('.', ARest + '.') - 1), 0);
  Delete(ARest, 1, Pos('.', ARest + '.'));
  AMin := StrToIntDef(Copy(ARest, 1, Pos('.', ARest + '.') - 1), 0);
  Delete(ARest, 1, Pos('.', ARest + '.'));
  APat := StrToIntDef(Copy(ARest, 1, Pos('.', ARest + '.') - 1), 0);

  BMaj := StrToIntDef(Copy(BRest, 1, Pos('.', BRest + '.') - 1), 0);
  Delete(BRest, 1, Pos('.', BRest + '.'));
  BMin := StrToIntDef(Copy(BRest, 1, Pos('.', BRest + '.') - 1), 0);
  Delete(BRest, 1, Pos('.', BRest + '.'));
  BPat := StrToIntDef(Copy(BRest, 1, Pos('.', BRest + '.') - 1), 0);

  if AMaj <> BMaj then begin Result := AMaj - BMaj; exit; end;
  if AMin <> BMin then begin Result := AMin - BMin; exit; end;
  Result := APat - BPat;
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
  { Request exclusive read/write — fails if OBS (or anything) has the DLL loaded. }
  H := CreateFileW(FileName, WIN_GENERIC_READ or WIN_GENERIC_WRITE, 0, 0, WIN_OPEN_EXISTING, WIN_FILE_ATTRIBUTE_NORMAL, 0);
  if H = WIN_INVALID_HANDLE_VALUE then
    Result := True
  else
    CloseHandle(H);
end;

function ConfirmUpgrade(const PrevVer, NewVer: String): Boolean;
var
  Body: String;
begin
  Body :=
    'Vertical Shorts Plugin ' + PrevVer + ' is currently installed.'#13#10#13#10 +
    'Setup will upgrade it to Vertical Shorts Plugin ' + NewVer + '.'#13#10#13#10 +
    'Your scenes, sources, destinations, credentials, schedules, and settings will be preserved.'#13#10 +
    'You do not need to uninstall first.';
  Result := TaskDialogMsgBox('Upgrade Vertical Shorts Plugin', Body, mbInformation,
    MB_OKCANCEL, ['&Upgrade', 'Cancel'], 0) = IDOK;
end;

(* Ask the user to close OBS / unlock the DLL. Never force-kill OBS. *)
function EnsureObsClosedAndDllUnlocked(const DllPath: String): Boolean;
var
  Answer: Integer;
  NeedObs, NeedDll: Boolean;
  Msg: String;
begin
  Result := True;
  while True do begin
    NeedObs := IsOBSRunning;
    NeedDll := (DllPath <> '') and FileExists(DllPath) and IsFileLocked(DllPath);

    if (not NeedObs) and (not NeedDll) then begin
      AppendInstallLog('OBS closed and plugin DLL unlocked');
      exit;
    end;

    if NeedObs then
      Msg :=
        'OBS Studio must be closed before Vertical Shorts Plugin can be installed or updated.'#13#10#13#10 +
        'Please close OBS Studio completely, then click Retry.'#13#10#13#10 +
        'Setup will not force-quit OBS.'
    else
      Msg :=
        'The Vertical Shorts plugin file is currently in use and cannot be replaced.'#13#10#13#10 +
        DllPath + #13#10#13#10 +
        'Close OBS Studio (and any tool locking the DLL), then click Retry.';

    if NeedObs then
      AppendInstallLog('Waiting: OBSRunning=1 DllLocked=' + IntToStr(Ord(NeedDll)) + ' Path=' + DllPath)
    else
      AppendInstallLog('Waiting: OBSRunning=0 DllLocked=' + IntToStr(Ord(NeedDll)) + ' Path=' + DllPath);

    Answer := MsgBox(Msg, mbConfirmation, MB_RETRYCANCEL);
    if Answer = IDCANCEL then begin
      AppendInstallLog('User cancelled while waiting for OBS/DLL unlock');
      Result := False;
      exit;
    end;
    Sleep(500);
  end;
end;

function CopyFileIfExists(const Src, Dest: String): Boolean;
begin
  Result := False;
  if FileExists(Src) then
    Result := FileCopy(Src, Dest, False);
end;

function CreateUpgradeBackup: Boolean;
var
  Stamp, Dest, MetaPath, PluginCfg: String;
begin
  Result := True;
  Stamp := GetDateTimeString('yyyymmdd_hhnnss', #0, #0);
  Dest := ExpandConstant('{localappdata}\VerticalShortsPlugin\upgrade-backups\' + Stamp);
  GUpgradeBackupDir := Dest;
  if not ForceDirectories(Dest) then begin
    Result := False;
    exit;
  end;

  MetaPath := ExpandConstant('{app}\data\obs-plugins\obs-shorts-vertical\install-meta.ini');
  CopyFileIfExists(MetaPath, Dest + '\install-meta.ini');
  if (GExistingPluginDll <> '') and FileExists(GExistingPluginDll) then
    SaveStringToFile(Dest + '\previous-dll-path.txt', GExistingPluginDll + #13#10, False);

  PluginCfg := ExpandConstant('{userappdata}\obs-studio\plugin_config\obs-shorts-vertical');
  if DirExists(PluginCfg) then begin
    ForceDirectories(Dest + '\plugin_config');
    CopyFileIfExists(PluginCfg + '\config.json', Dest + '\plugin_config\config.json');
    CopyFileIfExists(PluginCfg + '\settings.json', Dest + '\plugin_config\settings.json');
  end;

  SaveStringToFile(Dest + '\upgrade-info.txt',
    'Product={#MyAppName}'#13#10 +
    'PreviousVersion=' + GPreviousVersion + #13#10 +
    'NewVersion={#MyAppVersion}'#13#10 +
    'AppId={' + '{#MyAppIdGuid}' + '}'#13#10 +
    'ObsInstallPath=' + GObsInstallPath + #13#10 +
    'AppDir=' + ExpandConstant('{app}') + #13#10 +
    'PreviousDll=' + GExistingPluginDll + #13#10 +
    'UserConfig=OBS scene collection key obs-shorts-vertical (not overwritten)'#13#10 +
    'Credentials=Windows Credential Manager (not overwritten)'#13#10 +
    'Note=Backup excludes recordings and large media files.'#13#10,
    False);
  AppendInstallLog('Upgrade backup created: ' + Dest);
end;

function RemoveObsoletePluginBins: Boolean;
var
  P: String;
begin
  Result := True;
  P := ExpandConstant('{app}\obs-plugins\64bit\obs-shorts-vertical.pdb');
  if FileExists(P) then begin
    if not DeleteFile(P) then
      AppendInstallLog('WARNING: could not delete obsolete PDB: ' + P + ' err=' + IntToStr(WinGetLastError))
    else
      AppendInstallLog('Removed obsolete PDB: ' + P);
  end;

  { Only OUR obsolete third-party plugin trees — never unrelated OBS plugins. }
  P := ExpandConstant('{commonappdata}\obs-studio\plugins\obs-shorts-vertical');
  if DirExists(P) then begin
    DelTree(P, True, True, True);
    AppendInstallLog('Removed obsolete ProgramData plugin tree: ' + P);
  end;

  P := ExpandConstant('{userappdata}\obs-studio\plugins\obs-shorts-vertical');
  if DirExists(P) then begin
    DelTree(P, True, True, True);
    AppendInstallLog('Removed obsolete AppData plugin tree: ' + P);
  end;

  { Earlier mistaken bin\64bit copies of OUR DLL under OBS root (not OBS core). }
  P := ExpandConstant('{app}\bin\64bit\obs-shorts-vertical.dll');
  if FileExists(P) then begin
    if not DeleteFile(P) then
      AppendInstallLog('WARNING: could not delete obsolete DLL copy: ' + P + ' err=' + IntToStr(WinGetLastError))
    else
      AppendInstallLog('Removed obsolete DLL copy: ' + P);
  end;
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

  if not FileExists(ExpandConstant('{app}\bin\64bit\obs64.exe')) then begin
    AppendInstallLog('VERIFY FAIL: OBS exe missing after install');
    Result := False;
  end;
end;

function InitializeSetup: Boolean;
var
  Answer: Integer;
  Prev, Cur: String;
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
  GExistingPluginDll := FindExistingPluginDll;
  GUpgradeBackupDir := '';

  AppendInstallLog('Detected OBS path: ' + GObsInstallPath);
  AppendInstallLog('Existing plugin DLL: ' + GExistingPluginDll);
  if GObsInstallPath <> '' then
    AppendInstallLog('OBS validation: bin\64bit\obs64.exe present')
  else
    AppendInstallLog('OBS not auto-detected; user must select a valid OBS Studio folder');

  if GObsInstallPath = '' then begin
    MsgBox(
      'OBS Studio could not be found automatically.'#13#10#13#10 +
      'On the next page, select your OBS Studio installation folder.'#13#10 +
      'It must contain bin\64bit\obs64.exe (for example C:\Program Files\obs-studio).'#13#10#13#10 +
      'Do not select an OBS profile, AppData, Documents, or source-code folder.',
      mbInformation, MB_OK);
  end;

  Cur := '{#MyAppVersion}';
  Prev := DetectPreviousVersion;
  GPreviousVersion := Prev;
  GIsUpgrade := IsUpgradeInstall;
  if GIsUpgrade then
    AppendInstallLog('Mode=upgrade PreviousVersion=' + Prev)
  else
    AppendInstallLog('Mode=fresh PreviousVersion=(none)');

  if GIsUpgrade then begin
    if Prev = '' then
      Prev := '(unknown)';

    if not ConfirmUpgrade(Prev, Cur) then begin
      AppendInstallLog('User cancelled upgrade confirmation');
      Result := False;
      exit;
    end;

    if (GPreviousVersion <> '') and (CompareVersionParts(GPreviousVersion, Cur) > 0) then begin
      Answer := MsgBox(
        'A newer version (' + GPreviousVersion + ') appears to be installed than this package (' + Cur + ').'#13#10#13#10 +
        'Installing an older package is not recommended and will not downgrade your configuration schema.'#13#10#13#10 +
        'Continue anyway?',
        mbConfirmation, MB_YESNO);
      if Answer <> IDYES then begin
        AppendInstallLog('User cancelled downgrade warning');
        Result := False;
        exit;
      end;
    end;
  end;

  { Always block while OBS holds our DLL. Fresh installs with no prior DLL can
    copy while OBS is open; the plugin loads after OBS is restarted. }
  if (GExistingPluginDll <> '') or GIsUpgrade then begin
    if not EnsureObsClosedAndDllUnlocked(GExistingPluginDll) then
      Result := False;
  end else if IsOBSRunning then begin
    AppendInstallLog('OBS is running during fresh install (no existing DLL to replace)');
    MsgBox(
      'OBS Studio is currently running.'#13#10#13#10 +
      'Setup can install Vertical Shorts Plugin while OBS is open, but you must ' +
      'restart OBS Studio afterward for the plugin to load.'#13#10#13#10 +
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
        '  C:\Program Files\obs-studio'#13#10#13#10 +
        'Do not select an OBS profile, AppData, Documents, or source-code folder.',
        mbError, MB_OK);
      AppendInstallLog('Invalid OBS folder selected: ' + WizardDirValue);
      Result := False;
    end else begin
      GObsInstallPath := WizardDirValue;
      AppendInstallLog('User confirmed OBS folder: ' + GObsInstallPath);
    end;
  end;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result :=
    'Installer version: {#MyAppVersion}' + NewLine +
    'Architecture: x64' + NewLine + NewLine +
    'OBS Studio folder:' + NewLine +
    Space + WizardDirValue + NewLine + NewLine +
    'Plugin DLL:' + NewLine +
    Space + WizardDirValue + '\obs-plugins\64bit\obs-shorts-vertical.dll' + NewLine + NewLine +
    'Plugin data:' + NewLine +
    Space + WizardDirValue + '\data\obs-plugins\obs-shorts-vertical\' + NewLine + NewLine +
    'Install log:' + NewLine +
    Space + GetInstallLogPath('') + NewLine;
  if GIsUpgrade then
    Result := Result + NewLine + 'Mode: Upgrade (settings preserved)' + NewLine
  else
    Result := Result + NewLine + 'Mode: Fresh install' + NewLine;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  TargetDll: String;
begin
  NeedsRestart := False;
  Result := '';

  TargetDll := ExpandConstant('{app}\obs-plugins\64bit\obs-shorts-vertical.dll');
  AppendInstallLog('PrepareToInstall target DLL: ' + TargetDll);

  if not EnsureObsClosedAndDllUnlocked(TargetDll) then begin
    Result := 'OBS Studio must be closed before Vertical Shorts Plugin can be installed or updated. Please close OBS Studio and click Retry.';
    AppendInstallLog('PrepareToInstall aborted: OBS/DLL still locked');
    exit;
  end;

  if not IsValidObsDir(ExpandConstant('{app}')) then begin
    Result := 'The selected folder does not contain a valid OBS Studio installation (missing bin\64bit\obs64.exe).';
    AppendInstallLog('PrepareToInstall aborted: invalid OBS dir');
    exit;
  end;

  { OBS ships the VC++ runtime it needs; warn only if clearly absent. }
  if not FileExists(ExpandConstant('{app}\bin\64bit\obs.dll')) then
    AppendInstallLog('WARNING: obs.dll not found next to obs64.exe — OBS install may be incomplete');

  if GIsUpgrade then begin
    if not CreateUpgradeBackup then begin
      Result := 'Could not create a lightweight upgrade backup under LocalAppData. Administrator permission may be required.';
      AppendInstallLog('PrepareToInstall aborted: backup failed');
      exit;
    end;
  end;

  RemoveObsoletePluginBins;
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
    SetIniString('Install', 'UpgradeBackup', GUpgradeBackupDir, MetaPath);
    SetIniString('Install', 'InstallLog', GInstallLogPath, MetaPath);
    SetIniString('Install', 'ConfigLocation',
      'OBS scene collection key obs-shorts-vertical + Windows Credential Manager', MetaPath);
    SetIniString('Install', 'Notes',
      'DLL under obs-plugins\64bit; data under data\obs-plugins\obs-shorts-vertical. User config is never overwritten.', MetaPath);

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

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then begin
    { Binaries removed by [Files]/[UninstallDelete]. Scene collections and
      Credential Manager secrets are intentionally left intact. }
  end;
end;
