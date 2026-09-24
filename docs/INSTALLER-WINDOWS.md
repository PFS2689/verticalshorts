# Windows installer (Inno Setup)

Vertical Shorts Plugin ships a **standard Inno Setup 6** clean installer that
copies the plugin into an existing OBS Studio 32 (x64) installation.

There is **no updater**, **no upgrade checker**, **no upgrade backup**, and
**no AppUpdatesURL**. Setup only installs the current plugin files.

## Final installer name

```
Vertical-Shorts-Plugin-1.0.9-Setup.exe
```

(Product name and version come from `buildspec.json`.)

## Permanent AppId (do not change)

```
{D4336EAC-D873-4E6B-8575-07096987E0C8}
```

Source of truth: `buildspec.json` → `uuids.windowsApp` → `installer/windows/VerticalShortsPlugin.iss` → `AppId`.

## Install location (OBS root)

`{app}` is the **OBS Studio installation directory**, defaulting to:

```
{autopf}\obs-studio
→ C:\Program Files\obs-studio
```

Plugin destinations:

```
{app}\obs-plugins\64bit\obs-shorts-vertical.dll
{app}\data\obs-plugins\obs-shorts-vertical\
```

Administrator (UAC) is required.

### Runtime dependencies

This plugin links against **OBS-provided** `libobs` / `obs-frontend-api` / Qt6.
The installer must **not** bundle nested Qt or `obs.dll` copies.
The Visual C++ runtime is provided by OBS Studio itself.

### OBS version compatibility

Vertical Shorts requires **OBS Studio 32.0+ (x64)**.

The Windows build compiles against OBS 32.2.1 headers, but `obs_module_ver()`
advertises **API 32.0.0** so OBS 32.0 and 32.1 will load the module.

## Install log

Every Setup run writes:

```
%LOCALAPPDATA%\VerticalShortsPlugin\logs\install-YYYYMMDD_HHMMSS.log
```

The log never records stream keys, credentials, or passwords.

## OBS detection

Before the wizard starts, Setup looks for a valid OBS root
(`bin\64bit\obs64.exe` + `obs-plugins\64bit` + `data\obs-plugins`) via:

1. `HKLM\Software\OBS Studio`
2. OBS Studio uninstall `InstallLocation`
3. `{autopf}\obs-studio` / `{pf}\obs-studio` / `C:\Program Files\obs-studio`

If OBS is not found, the directory page lets the user browse manually.

## Locked DLL / OBS running

If an existing `obs-shorts-vertical.dll` is locked (OBS running), Setup shows
Retry/Cancel and **does not force-kill OBS**.

## Uninstall

Uninstall removes only:

- `{app}\obs-plugins\64bit\obs-shorts-vertical.dll`
- `{app}\data\obs-plugins\obs-shorts-vertical\`

OBS core files, unrelated plugins, scene collections, and Credential Manager
secrets are never deleted.

## Build order (required)

1. Build the plugin (**Release** on tags, RelWithDebInfo on PRs)
2. Verify `obs-shorts-vertical.dll` + locale + `INSTALL.txt`
3. Stage payload under `release/staging/obs-shorts-vertical/` (includes `install-meta.ini`)
4. Compile `installer/windows/VerticalShortsPlugin.iss` with **ISCC.exe**
5. Scan Setup.exe for removed upgrade-system markers; refuse to package if found
6. Scan / sign / publish the resulting Setup.exe

## Local packaging (Windows)

```powershell
choco install innosetup --no-progress -y
$env:CI = '1'
.\.github\scripts\Package-Windows.ps1 -Target x64 -Configuration Release
```

Output:

- `release\Vertical-Shorts-Plugin-<version>-Setup.exe`
- `release\Vertical-Shorts-Plugin-<version>.zip`

## Script

- `installer/windows/VerticalShortsPlugin.iss` — Inno Setup 6 project
- AppId GUID: `buildspec.json` → `uuids.windowsApp`
- Compression: `lzma2/max`, solid
