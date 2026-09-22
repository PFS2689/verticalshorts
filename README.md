# Vertical Shorts Plugin for OBS Studio

**Version 1.0.6** — Windows

Professional Vertical Streaming Plugin for OBS Studio — vertical production dock for Shorts, TikTok, Reels, and Twitch.

## Download

- **Setup.exe:** [Vertical-Shorts-Plugin-1.0.6-Setup.exe](https://github.com/PFS2689/verticalshorts/releases/latest/download/Vertical-Shorts-Plugin-1.0.6-Setup.exe)
- **Zip:** [Vertical-Shorts-Plugin-1.0.6.zip](https://github.com/PFS2689/verticalshorts/releases/latest/download/Vertical-Shorts-Plugin-1.0.6.zip)
- **Checksums:** [SHA256SUMS.txt](https://github.com/PFS2689/verticalshorts/releases/latest/download/SHA256SUMS.txt)
- **Signing:** Production builds are Authenticode-signed (see [docs/SIGNING-WINDOWS.md](docs/SIGNING-WINDOWS.md))
- **Installer:** Standard **Inno Setup 6** (see [docs/INSTALLER-WINDOWS.md](docs/INSTALLER-WINDOWS.md))

## UI overview

Native OBS docks (View → Docks):

- **Vertical Shorts** — vertical canvas + emoji controls (🟢 ⏺️ 📸 📷 ⚙️) + Preset row
- **Vertical Production** — Vertical Scenes, Sources, Transitions, and transition duration
  - **Vertical Sources** matches the OBS Sources pattern: only **+** / **−** on the button row; Properties, Filters, Transform, Rename, Duplicate, Copy/Paste, Lock, Show/Hide, Order, and Remove are on the right-click menu; double-click opens Properties; drag to reorder. Add Source supports Create New, Add Existing, and Add Existing Scene. New cameras/video default to **Fill Vertical Canvas** (aspect-preserving cover + center crop, never auto-stretch). Vertical scene-item transforms stay independent from main OBS scene items; shared sources reuse the same OBS source (Properties may affect every scene using that source).

Settings uses categories on the left and pages on the right (OK / Cancel / Apply) with General, Vertical Canvas, Vertical Recording, Vertical Clips, Recording Automation, Vertical Streaming, Audio, and About.

- **Independent Vertical Streaming Destination** (YouTube / Twitch / TikTok / Instagram / Custom RTMP) — never inherits the main OBS stream key or service
- Clip buffer, recording automation, and hotkeys

## Install / test

1. Install the current Setup.exe (UAC / Administrator). It targets your OBS folder  
   (`C:\Program Files\obs-studio` by default):  
   `obs-plugins\64bit\obs-shorts-vertical.dll` and  
   `data\obs-plugins\obs-shorts-vertical\`.
2. Open OBS Studio **32.2.1**.
3. If needed, enable **Vertical Shorts Plugin** in **Tools → Plugin Manager**, then restart OBS.
4. Open **View → Docks** and enable Vertical Shorts / Scenes / Sources / Transitions as needed.
5. Open **Settings → Vertical Streaming** and configure a Vertical Streaming Destination.
6. Confirm the main OBS canvas and main stream settings remain unchanged.
7. Test Vertical Go Live, Vertical Record, Short Clip, Long Clip, and Settings.
8. Confirm output files save to the selected vertical recording path.

## Requirements

- Windows 10/11 64-bit  
- OBS Studio **32.2.1** (built against 32.2.1 / obs-deps 2026-07-15)

## Build

```bash
# Windows (VS 2022)
cmake --preset windows-release
cmake --build --preset windows-release

# Unit tests
cmake --preset windows-tests
cmake --build --preset windows-tests
ctest --preset windows-tests
```

CI configures tests automatically and runs `ctest --output-on-failure` before packaging. Test binaries are not included in the release zip/Setup.exe.

## License

GPL-2.0-or-later (same as OBS Studio).
