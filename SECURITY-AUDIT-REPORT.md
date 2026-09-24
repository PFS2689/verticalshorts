# Security, Antivirus, and Release Audit — Vertical Shorts Plugin

**Audit date:** 2026-08-08 (production release metadata refresh; product version remains 1.0.5)  
**Branch:** `cursor/production-release-1-0-5-11e0`  
**Plugin version:** 1.0.5  
**Official tag:** `v1.0.5`

This audit covers C++/Qt/OBS sources, CMake, CI/workflows, the Windows Inno Setup installer, packaging scripts, resources, and credential handling.  
**No antivirus bypass, Defender exclusions, packers, or obfuscation were added.**

---

## 1. Production posture

| Item | Value |
|------|--------|
| Product name | Vertical Shorts Plugin |
| Version (single source: `buildspec.json`) | **1.0.5** |
| File / Product version (PE) | **1.0.5** |
| FileDescription | Professional Vertical Streaming Plugin for OBS Studio |
| Public Git tag | **v1.0.5** only |
| Public GitHub Release | **Vertical Shorts Plugin 1.0.5** only |
| Release configuration | MSVC **Release** (`/O2`, `/MD`, `/GL`, `/LTCG`, `/DEBUG:NONE`) |

---

## 2. Security issues found

### Critical
- None.

### High
| ID | Issue |
|----|--------|
| H1 | ~~Windows Setup.exe / plugin DLL unsigned~~ → **Fixed in CI:** tag releases require Authenticode (Azure Artifact Signing or PFX); see `docs/SIGNING-WINDOWS.md` |

### Medium
| ID | Issue |
|----|--------|
| M1 | Setup embeds PE DLL as **RCDATA** and writes it with `CreateFileW`/`WriteFile` — legitimate dropper shape that AV ML often flags |
| M2 | Unencrypted `rtmp://` still allowed (warning only) — stream key can transit in cleartext if the user chooses it |

### Low / Info
- Secrets in process memory as `QString` while OBS runs (normal for streaming apps)
- RTMP username stored in OBS settings JSON (non-secret metadata)
- Floating GitHub Action major tags (not commit SHAs)
- Legacy plaintext credential fallback files remain readable until next save migrates them to DPAPI

### Explicitly **not** present
- Process injection, packers/UPX, Defender exclusions, hidden PowerShell/CMD in shipped code
- Runtime downloads / nested installers
- Admin elevation (`asInvoker` only)
- Persistence (Run keys / services / scheduled tasks)
- Test binaries or PDBs in shipping packages
- Unused third-party DLLs in the repository or package

---

## 3. Hardening applied (production pass)

| Change | Detail |
|--------|--------|
| Windows metadata | ProductName / FileVersion / ProductVersion **1.0.5**; FileDescription set to **Professional Vertical Streaming Plugin for OBS Studio** on DLL + Setup |
| OBS module description | Matches professional product description |
| Release link flags | `/DEBUG:NONE` for Release (no PDB / no debug directory in shipping PE) |
| CRT | Explicit `CMAKE_MSVC_RUNTIME_LIBRARY` → `/MD` (Release) / `/MDd` (Debug) |
| Setup.exe | Inno Setup 6 (`ISCC.exe`), LZMA2 solid compression; signed in CI when credentials exist |
| Package payload | Strip on-disk `data/icons` (icons already in DLL via Qt resources); keep `data/locale` only |
| Windows Defender | Tag builds run `MpCmdRun -Scan -ScanType 3` on Setup.exe, zip, and DLL before artifact upload |
| ClamAV | Release job still requires successful `freshclam` + clean scan before publish |

---

## 4. Antivirus scan results

| Scanner | Environment | Result |
|---------|-------------|--------|
| **ClamAV 1.5.3** | This Linux agent + GitHub Actions release job | **CLEAN** (0 infected) on Setup.exe, zip, SHA256SUMS.txt — signatures updated via freshclam |
| **Windows Defender** | GitHub Actions `windows-2022` on tag builds | **Gated** via `MpCmdRun.exe` — threats abort the build; no exclusions |
| **Windows Defender** | This Linux cloud agent | Not runnable locally (no Windows host) |

**Honesty statement:** This audit **cannot** claim the project is “virus-free.” Unsigned or new-publisher Setup.exe builds can still receive Defender ML / SmartScreen reputation detections. Those must be investigated per-file if reported — never suppressed.

### Why Windows may still warn (expected)

| Signal | Cause | Malware? |
|--------|-------|----------|
| SmartScreen “Unknown publisher” / “Windows protected your PC” | **No Authenticode signature** + new/low download reputation | **No** — reputation warning only |
| Occasional Defender ML / SmartScreen on Setup.exe | New publisher reputation on Inno Setup installers | Usually **false positive** if ClamAV/Defender file scan is clean; prefer Authenticode |

**Do not** disable Defender, add exclusions, or bypass SmartScreen. If a confirmed false positive remains after signing, submit to [Microsoft WDSI](https://www.microsoft.com/wdsi/filesubmission).

---

## 5. Installer review

| Check | Status |
|-------|--------|
| Per-user `%APPDATA%\obs-studio\plugins\obs-shorts-vertical\` only | Pass |
| Files: DLL + `en-US.ini` + `INSTALL.txt` only | Pass |
| No admin (`asInvoker`) | Pass |
| No network / no child processes / no Run keys | Pass |
| No Defender exclusions | Pass |
| `/DYNAMICBASE` `/NXCOMPAT` / `/DEBUG:NONE` | Pass |
| Authenticode signed | **Required on tag releases** (Azure Artifact Signing or PFX); CI verifies `Get-AuthenticodeSignature` → Valid |

---

## 6. Release package review

| Check | Status |
|-------|--------|
| One public release / one tag (`v1.0.5`) | Pass |
| Assets: Setup.exe + versioned zip + `SHA256SUMS.txt` only | Pass |
| Zip allowlist under `obs-shorts-vertical/` (+ optional INSTALL.txt) | Pass |
| No nested `.exe` / scripts / `test_*` / PDBs | Pass |
| No Qt/OBS runtime DLLs bundled | Pass |
| ClamAV + Defender before publish | Pass |

---

## 7. Remaining risks / recommendations

1. Ensure repo secrets for Azure Artifact Signing or `WINDOWS_CODESIGN_PFX_*` are configured (see `docs/SIGNING-WINDOWS.md`).  
2. After the first signed public release, allow SmartScreen reputation to accumulate; submit residual FPs to Microsoft WDSI if needed.  
3. Pin GitHub Actions to full commit SHAs.  
4. Prefer `rtmps://` (UI already warns on cleartext `rtmp://`).

---

## Verification performed

- Static review of `src/**`, installer, CMake, packaging, workflows  
- Grep for process exec, injection, AV evasion, runtime downloads — **clean in shipped code**  
- Local ClamAV scan of published `v1.0.5` assets — **CLEAN**  
- Windows Release + Defender gates enforced in GitHub Actions on tag push  
