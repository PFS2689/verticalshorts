[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string[]] $Path,

    [string] $Description = 'Vertical Shorts Plugin',

    [string] $DescriptionUrl = 'https://github.com/PFS2689/verticalshorts',

    [switch] $Required
)

$ErrorActionPreference = 'Stop'

function Get-SignToolPath {
    $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    if (Test-Path $kitsRoot) {
        $found = Get-ChildItem -Path $kitsRoot -Recurse -Filter 'signtool.exe' -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($found) { return $found.FullName }
    }
    throw 'signtool.exe not found. Install the Windows SDK Signing Tools.'
}

function Test-HasPfxCredentials {
    return -not [string]::IsNullOrWhiteSpace($env:WINDOWS_CODESIGN_PFX_BASE64) -and
        -not [string]::IsNullOrWhiteSpace($env:WINDOWS_CODESIGN_PASSWORD)
}

function Test-HasAzureSigningCredentials {
    return -not [string]::IsNullOrWhiteSpace($env:AZURE_CLIENT_ID) -and
        -not [string]::IsNullOrWhiteSpace($env:AZURE_TENANT_ID) -and
        -not [string]::IsNullOrWhiteSpace($env:AZURE_CLIENT_SECRET) -and
        -not [string]::IsNullOrWhiteSpace($env:AZURE_TRUSTED_SIGNING_ENDPOINT) -and
        -not [string]::IsNullOrWhiteSpace($env:AZURE_TRUSTED_SIGNING_ACCOUNT) -and
        -not [string]::IsNullOrWhiteSpace($env:AZURE_TRUSTED_SIGNING_CERTIFICATE_PROFILE)
}

function Assert-AuthenticodeSignature {
    param([string] $FilePath)
    $sig = Get-AuthenticodeSignature -FilePath $FilePath
    if ($sig.Status -ne 'Valid') {
        throw "Authenticode signature invalid for ${FilePath}: $($sig.Status) $($sig.StatusMessage)"
    }
    Write-Host "Authenticode VALID: $FilePath ($($sig.SignerCertificate.Subject))"
}

function Sign-WithPfx {
    param([string[]] $Files)

    $signTool = Get-SignToolPath
    $work = Join-Path ([System.IO.Path]::GetTempPath()) ("vsp-codesign-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work | Out-Null
    $pfxPath = Join-Path $work 'codesign.pfx'
    try {
        $bytes = [Convert]::FromBase64String($env:WINDOWS_CODESIGN_PFX_BASE64)
        [IO.File]::WriteAllBytes($pfxPath, $bytes)

        foreach ($file in $Files) {
            Write-Host "Signing with PFX: $file"
            & $signTool sign `
                /fd SHA256 `
                /td SHA256 `
                /tr http://timestamp.digicert.com `
                /f $pfxPath `
                /p $env:WINDOWS_CODESIGN_PASSWORD `
                /d $Description `
                /du $DescriptionUrl `
                $file
            if ($LASTEXITCODE -ne 0) {
                throw "signtool failed for $file (exit $LASTEXITCODE)"
            }
            Assert-AuthenticodeSignature -FilePath $file
        }
    } finally {
        Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
    }
}

$resolved = @()
foreach ($p in $Path) {
    if (-not (Test-Path -LiteralPath $p)) {
        throw "File to sign not found: $p"
    }
    $resolved += (Resolve-Path -LiteralPath $p).Path
}

$hasPfx = Test-HasPfxCredentials
$hasAzure = Test-HasAzureSigningCredentials

if (-not $hasPfx -and -not $hasAzure) {
    if ($Required) {
        throw @"
Authenticode signing is required for Release artifacts, but no signing credentials were found.

Configure ONE of:
  1) Azure Artifact Signing / Trusted Signing secrets:
     AZURE_TENANT_ID, AZURE_CLIENT_ID, AZURE_CLIENT_SECRET,
     AZURE_TRUSTED_SIGNING_ENDPOINT, AZURE_TRUSTED_SIGNING_ACCOUNT,
     AZURE_TRUSTED_SIGNING_CERTIFICATE_PROFILE
  2) Traditional code-signing PFX secrets:
     WINDOWS_CODESIGN_PFX_BASE64, WINDOWS_CODESIGN_PASSWORD

See docs/SIGNING-WINDOWS.md
"@
    }
    Write-Warning 'No Windows code-signing credentials configured; leaving binaries unsigned.'
    return
}

if ($hasAzure -and -not $hasPfx) {
    # Azure Artifact Signing is performed by the GitHub Action in the workflow
    # (azure/artifact-signing-action). This script only verifies afterward when
    # called with files that should already be signed, or signs via PFX.
    Write-Host 'Azure Artifact Signing credentials detected — workflow action performs signing; verifying if already signed...'
    $allValid = $true
    foreach ($file in $resolved) {
        $sig = Get-AuthenticodeSignature -FilePath $file
        if ($sig.Status -ne 'Valid') {
            $allValid = $false
            Write-Host "Not yet signed: $file ($($sig.Status))"
        } else {
            Write-Host "Already signed: $file"
        }
    }
    if (-not $allValid -and $Required) {
        throw 'Azure signing credentials are present, but files are not signed yet. The workflow must run azure/artifact-signing-action before verification.'
    }
    if ($allValid) { return }
}

if ($hasPfx) {
    Sign-WithPfx -Files $resolved
    return
}

throw 'Unexpected signing configuration state.'
