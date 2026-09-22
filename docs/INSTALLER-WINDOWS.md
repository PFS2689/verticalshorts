# Windows installer (Inno Setup)

Vertical Shorts Plugin ships a **standard Inno Setup 6** installer with a permanent
in-place upgrade identity, hardened for clean-machine OBS installs.

## Final installer name

```
Vertical-Shorts-Plugin-1.0.6-Setup.exe
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

## Install log

Every Setup run writes:

```
%LOCALAPPDATA%\VerticalShortsPlugin\logs\install-YYYYMMDD_HHMMSS.log
```

The log includes installer version, detected OBS path, architecture, destination
paths, OBS-running / DLL-lock waits, copy/verify results, and Windows error
codes. It never records stream keys, credentials, or passwords.

## OBS detection

Before the wizard starts, Setup looks for a valid OBS root
(`bin\64bit\obs64.exe` + `obs-plugins\64bit` + `data\obs-plugins`) via:

1. Previous Vertical Shorts uninstall `InstallLocation` (if it is a valid OBS root)
2. `HKLM\Software\OBS Studio`
3. OBS Studio uninstall `InstallLocation`
4. `{autopf}\obs-studio` / `{pf}\obs-studio` / `C:\Program Files\obs-studio`

If OBS is not found, the directory page lets the user browse manually.

## Locked DLL / OBS running

If OBS is running or `obs-shorts-vertical.dll` is locked, Setup shows Retry/Cancel
and **does not force-kill OBS**. File copy proceeds only after the DLL can be
opened exclusively.

## In-place upgrades

When Vertical Shorts Plugin is already installed, running a newer Setup.exe:

1. Detects the previous version (uninstall registry + known DLL paths — not `{app}`)
2. Shows an **Upgrade** / **Cancel** confirmation
3. Requires OBS Studio to be closed / DLL unlocked (Retry loop)
4. Creates a lightweight backup under `%LOCALAPPDATA%\VerticalShortsPlugin\upgrade-backups\`
5. Replaces plugin binaries/resources under the OBS tree
6. Removes obsolete Vertical Shorts copies from known legacy paths only
7. Preserves user configuration (scene collection + Credential Manager)
8. Verifies DLL + locale data after install

## Build order (required)

1. Build the plugin (**Release** on tags, RelWithDebInfo on PRs)
2. Verify `obs-shorts-vertical.dll` + locale + `INSTALL.txt`
3. Stage payload under `release/staging/obs-shorts-vertical/` (includes `install-meta.ini`)
4. Compile `installer/windows/VerticalShortsPlugin.iss` with **ISCC.exe**
5. Scan / sign / publish the resulting Setup.exe

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
