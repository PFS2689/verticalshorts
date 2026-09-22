# Windows Authenticode signing — Vertical Shorts Plugin

Production Release tags (**`vX.Y.Z`**) **should** carry a valid Authenticode signature on:

- `obs-shorts-vertical.dll`
- `Vertical-Shorts-Plugin-<version>-Setup.exe` (Inno Setup 6)

When Azure Artifact Signing or PFX secrets are present, CI signs both artifacts and verifies `Get-AuthenticodeSignature` → `Valid`.  
If secrets are missing, CI still publishes the Release (unsigned) so load-path / OBS compatibility fixes can ship; re-tag after adding secrets to publish signed builds.

Signed builds are what remove the SmartScreen **“Unknown publisher”** block for unknown publishers.  
Do **not** disable Defender or add exclusions.

See also [INSTALLER-WINDOWS.md](INSTALLER-WINDOWS.md).

---

## Choose one signing path

### Option A — Azure Artifact Signing (Trusted Signing)

Best for ongoing commercial/OSS releases you control.

1. Create an [Azure Artifact Signing / Trusted Signing](https://learn.microsoft.com/azure/trusted-signing/) account + certificate profile (identity verification required).
2. Create an App Registration with a client secret.
3. Grant **Artifact Signing Certificate Profile Signer** on the signing account.
4. Add repository secrets:

| Secret | Example |
|--------|---------|
| `AZURE_TENANT_ID` | directory GUID |
| `AZURE_CLIENT_ID` | app registration application ID |
| `AZURE_CLIENT_SECRET` | app client secret |
| `AZURE_TRUSTED_SIGNING_ENDPOINT` | `https://eus.codesigning.azure.net/` (must match account region) |
| `AZURE_TRUSTED_SIGNING_ACCOUNT` | signing account name |
| `AZURE_TRUSTED_SIGNING_CERTIFICATE_PROFILE` | certificate profile name |

### Option B — Traditional OV/EV code-signing PFX

1. Buy an Authenticode certificate from a public CA.
2. Export `.pfx` and add:

| Secret | Value |
|--------|--------|
| `WINDOWS_CODESIGN_PFX_BASE64` | `base64 -w0 cert.pfx` |
| `WINDOWS_CODESIGN_PASSWORD` | PFX password |

### Option C — SignPath Foundation (free for qualifying OSS)

1. Apply at [signpath.org](https://signpath.org/) / SignPath OSS program with this repo URL.
2. After approval, create a project + signing policy and add SignPath API secrets.
3. Tell the maintainer / open an issue — the workflow can be wired to `signpath/github-action-submit-signing-request` once the org/project slugs exist.

---

## What CI does on a tag

1. Build MSVC **Release** plugin DLL  
2. Verify DLL exports + package layout + dumpbin dependents  
3. **Sign** `obs-shorts-vertical.dll`  
4. Stage payload under `release/staging/`  
5. Package with **Inno Setup 6** (`ISCC.exe`) → `Vertical-Shorts-Plugin-<version>-Setup.exe`  
6. **Sign** the Setup.exe  
7. Verify `Get-AuthenticodeSignature` → `Valid`  
8. Windows Defender + ClamAV  
9. Publish GitHub Release + `SHA256SUMS.txt`

---

## After secrets are added

```bash
git tag -d v1.0.7
git push origin :refs/tags/v1.0.7
git tag -a v1.0.7 -m "Vertical Shorts Plugin 1.0.7"
git push origin v1.0.7
```

Or push any new `v*` tag on the signing branch/main. The Release Windows workflow will sign and publish.

## Local verification

```powershell
Get-AuthenticodeSignature '.\Vertical-Shorts-Plugin-1.0.7-Setup.exe'
Get-AuthenticodeSignature '.\obs-shorts-vertical.dll'
```

Both must report `Status : Valid`.
