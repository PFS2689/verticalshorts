# Changelog — Vertical Shorts Plugin

**Official product version: 1.0.9**

## 1.0.9

- **Installer:** Rewrote Inno Setup as a clean install-only script
  - Completely removed remaining upgrade/updater flow (previous-version detection, reinstall-mode branching, upgrade messaging, Start Menu install-log shortcut, AppUpdatesURL)
  - Installs current plugin files into OBS 32 (`obs-plugins\64bit` + `data\obs-plugins\obs-shorts-vertical`) with no upgrade helper
  - Packaging refuses Setup.exe if upgrade-system markers (`ConfirmUpgrade`, `CreateUpgradeBackup`, `GIsUpgrade`, `updater.exe`, etc.) are present
- Installer filename: `Vertical-Shorts-Plugin-1.0.9-Setup.exe`
- Stable AppId unchanged: `{D4336EAC-D873-4E6B-8575-07096987E0C8}`
- Multi-canvas camera work from 1.0.8 is unchanged

## 1.0.8

- **Fix:** Vertical camera rendering on OBS multi-canvas
  - Fill mode uses native `OBS_BOUNDS_SCALE_OUTER` + `crop_to_bounds` (removed manual crop/scale that blanked the vertical camera)
  - Shared OBS Camera: `obs_scene_add` of the existing capture `obs_source_t` onto the vertical canvas scene (independent transforms; no second hardware open)
  - Removed destructive camera resolve/watch workarounds that deleted provisional sources
- Removed installer upgrade confirmation, upgrade-backup, and AppUpdatesURL (no self-updater)
- Installer filename: `Vertical-Shorts-Plugin-1.0.8-Setup.exe`
- Stable AppId unchanged: `{D4336EAC-D873-4E6B-8575-07096987E0C8}`

## 1.0.7

- **Fix:** OBS Plugin Load Error for `obs-shorts-vertical` on OBS Studio 32.0 / 32.1
  - Advertised module API **32.0.0** so OBS 32.0/32.1 accept the module
  - Defer dock/canvas construction until frontend `FINISHED_LOADING`
- Requires **OBS Studio 32.0 or newer** (64-bit)
- Installer filename: `Vertical-Shorts-Plugin-1.0.7-Setup.exe`

## 1.0.6

- Hardened Inno Setup installer for clean-machine OBS deployments (fresh install + upgrade)
- Clear Retry/Cancel when OBS is running or the plugin DLL is locked (no force-kill)
- Install logging under `%LOCALAPPDATA%\VerticalShortsPlugin\logs\`
- Stronger OBS folder validation and post-install payload verification
- Installer filename: `Vertical-Shorts-Plugin-1.0.6-Setup.exe`
- Note: 1.0.6 could fail to load on OBS 32.0/32.1 due to advertised module API 32.2 — fixed in 1.0.8

---

## 1.0.5 (previous)

All shipping artifacts for 1.0.5 used product version **1.0.5**.

Earlier intermediate development labels (1.1.x–1.4.x) used during feature work are **not** separate official releases. Their notes are preserved below as development history only.

---

## 1.0.5 details

Current official release of Vertical Shorts Plugin for OBS Studio (Windows).

**Product description:** Professional Vertical Streaming Plugin for OBS Studio

Includes:

- Vertical-only production dock (scenes, sources, mixer, transitions)
- Independent Vertical Streaming Destination (YouTube / Twitch / TikTok / Instagram / Custom RTMP)
- Custom platform selector with bundled SVG logos (embedded in the DLL)
- Secure credential storage (Windows Credential Manager + DPAPI fallback)
- Vertical recording, short/long clips, clip buffer readiness
- Optional recording automation and hotkeys
- **Inno Setup 6** installer installing into the OBS Studio folder (`obs-plugins\64bit` + `data\obs-plugins\obs-shorts-vertical`; UAC required)
- Built against **OBS Studio 32.2.1** / obs-deps **2026-07-15**
- MSVC Release build (`/MD`, `/DEBUG:NONE`), Windows Defender + ClamAV gates, SHA-256 checksums
- Authenticode signing when CI credentials are configured (Azure Artifact Signing or OV/EV PFX)
- **Fix:** Video Capture Device sources now render on the Vertical Shorts canvas (OBS 32 PROGRAM canvas with ACTIVATE so cameras receive `activate_refs`; preview uses `obs_canvas_render`)
- Replaced the former custom MSVC Win32 Setup stub with a standard Inno Setup pipeline (stage Release payload → ISCC)
- Vertical Shorts dock: dark OBS-style preview (never white), emoji-only controls
- Combined Vertical Scenes / Sources / Transitions into **Vertical Production** dock
- **Vertical Scenes are independent** from main OBS scenes (separate vertical scene collection on the vertical canvas; not a projection of the main Scenes list)
- Vertical Sources: OBS-style panel with only **+** / **−**; Properties, Filters, Transform, Rename, Duplicate, Copy/Paste, Lock, Visibility, Order, and Remove via right-click; double-click opens Properties; drag-and-drop reorder; Edit Transform popup affects only the vertical scene item; Properties/Filters use real OBS dialogs (`obs_frontend_open_source_properties` / `obs_frontend_open_source_filters`)
- Vertical Shorts dock: compact centered emoji toolbar; canvas Preset row directly underneath; short/long clip length presets on the Preset row (canvas keeps stretch)
- **Fix:** Vertical Shorts preview blank when a Video Capture Device (or other source) was listed but not drawn — keep PROGRAM channel 0 bound to the active vertical scene (ACTIVATE/MAIN_VIEW), render via `obs_canvas_render`, OBS-style dark backdrop, and fit new items with centered `OBS_BOUNDS_SCALE_INNER` (shared existing sources; no second camera open)
- **Fix:** Corner/edge resize handles on the vertical preview (bounds-aware stretch, HiDPI mouse mapping, larger hit targets); Add Source defaults to sharing an existing OBS camera so one USB device can be in horizontal and vertical with independent transforms
- **Fix:** Newly added cameras/video sources default to **Fill Vertical Canvas** (preserve aspect ratio, cover 9:16, center-crop — no stretch). Transform menu adds Fill / Fit Inside / Original Size / Stretch plus Vertical Fill Position (Left/Center/Right); canvas preset changes reapply the stored fit mode
- **Fix:** Live Video Capture Device rendering on the Vertical Shorts canvas — preview draws via `obs_source_video_render` of PROGRAM channel 0 (OBS studio-mode path), force-enables the `obs_display`, holds preview `show_refs` on the active vertical scene, force-rebinds ACTIVATE after add/fit, and validates transforms so new cameras are visible and on-canvas
- **Fix:** Solid white Vertical Shorts canvas — restore OBS `GS_BGRA` display format, harden `obs_display_create` (log failures, force attach on show/timers), never fall back to a white Qt HWND (dark paint until display owns the surface)
- **Camera sharing:** When a Video Capture Device is already open in Main OBS, Vertical Shorts reuses that `obs_source_t` (matched by stable device id: DShow `video_device_id`, AVFoundation `device`, V4L2 `device_id`) so one physical capture session feeds both. Vertical scene items keep independent Fill/Fit transforms; device properties remain shared. Create New offers Share Existing Camera; Duplicate/Paste will not open a second hardware session for the same device.
- **UI:** Vertical Scenes shows only **+** / **−**; Rename/Duplicate/Remove/Order via right-click (F2 / double-click rename). Vertical Sources row layout fixed (no text/icon overlap; elided names). Settings **Apply** dirty-tracks edits, applies without closing, disables when clean; OK applies+closes; Cancel discards unapplied edits.
- Settings: category list on the left; removed Hotkeys and Advanced tabs; repeat-by-day schedule day checkboxes
- Vertical canvas audio no longer uses `MIX_AUDIO` (independent from main program mix)
- **In-place upgrades:** permanent Inno Setup AppId; Setup detects an existing install, offers Upgrade/Cancel, requires OBS closed, backs up lightweight metadata, replaces binaries only, and preserves destinations/credentials/scenes/settings
- Configuration `config_schema` (independent from plugin version) with forward migration + backup; never auto-downgrades
- Release metadata uses a fresh UTC build/publish stamp (About dialog, `install-meta.ini`, GitHub Release notes) so “Last Updated” is never inherited from an older 1.0.5 artifact

---

## Development history (pre-official consolidation)

These entries document work that was folded into official **1.0.5**. They are not alternate product versions.

### Feature work later labeled 1.4.x during development

- Independent Vertical Streaming Destination (never inherits main OBS stream)
- Platform presets, status card, Test Configuration, masked stream keys
- Polished custom logo selector and hero header
- Security/release audit hardening (DPAPI fallback, scrub legacy keys, release allowlists)

### Feature work later labeled 1.3.x during development

- Dual-stream conflict handling (superseded by fully independent destinations)
- Real OBS volmeter mixer
- Clip buffer auto-start / readiness status
- CTest integration in CI

### Feature work later labeled 1.2.x during development

- Long clips, recording automation, vertical-only workspace
- Go Live / Record / Short Clip / Long Clip / Settings controls

### Feature work later labeled 1.1.x / 1.0.x during development

- Vertical Shorts dock UI redesign
- (Historical) Temporary custom MSVC Setup stub — **retired**; shipping installer is Inno Setup 6 again
