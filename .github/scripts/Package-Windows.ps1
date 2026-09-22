[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Find-ISCC {
    $candidates = @(
        "${Env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        "${Env:ProgramFiles}\Inno Setup 6\ISCC.exe"
        "${Env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
    )
    foreach ($p in $candidates) {
        if ( Test-Path $p ) { return $p }
    }
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ( $cmd ) { return $cmd.Source }
    return $null
}

function Assert-PluginPayload {
    param(
        [string] $PluginRoot,
        [string] $InstallTxt
    )

    $dll = Join-Path $PluginRoot 'bin\64bit\obs-shorts-vertical.dll'
    $locale = Join-Path $PluginRoot 'data\locale\en-US.ini'

    foreach ($p in @($dll, $locale, $InstallTxt)) {
        if ( ! ( Test-Path $p ) ) {
            throw "Required plugin payload missing (build plugin before packaging): $p"
        }
        if ( (Get-Item $p).Length -lt 1 ) {
            throw "Required plugin payload is empty: $p"
        }
    }

    # OBS module must not ship nested runtimes or scripts
    $unexpected = Get-ChildItem -Recurse $PluginRoot -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Extension -match '\.(exe|bat|cmd|ps1|vbs)$' -or
        $_.Name -match '^(Qt6|obs\.dll|obs-frontend-api)'
    }
    if ( $unexpected ) {
        $unexpected | ForEach-Object { Write-Host "UNEXPECTED: $($_.FullName)" }
        throw 'Unexpected executables or runtime DLLs in plugin payload — refusing to package'
    }

    Write-Host "Verified payload DLL: $dll ($((Get-Item $dll).Length) bytes)"
    Write-Host "Verified locale: $locale"
    Write-Host "Verified INSTALL.txt: $InstallTxt"
}

function New-InstallerStaging {
    param(
        [string] $ProjectRoot,
        [string] $ReleaseDir,
        [string] $StageRoot
    )

    if ( Test-Path $StageRoot ) {
        Remove-Item -Recurse -Force $StageRoot
    }

    $srcPlugin = Join-Path $ReleaseDir 'obs-shorts-vertical'
    $dstPlugin = Join-Path $StageRoot 'obs-shorts-vertical'
    if ( ! ( Test-Path $srcPlugin ) ) {
        throw "Plugin output folder missing (build Release/RelWithDebInfo first): $srcPlugin"
    }

    New-Item -ItemType Directory -Path $StageRoot | Out-Null
    Copy-Item -Recurse -Force $srcPlugin $dstPlugin

    $installSrc = Join-Path $ReleaseDir 'INSTALL.txt'
    if ( Test-Path $installSrc ) {
        Copy-Item -Force $installSrc (Join-Path $dstPlugin 'INSTALL.txt')
        Copy-Item -Force $installSrc (Join-Path $StageRoot 'INSTALL.txt')
    }

    # Version identity for in-place upgrades (AppId must never change across releases).
    $buildSpec = Get-Content -Path (Join-Path $ProjectRoot 'buildspec.json') -Raw | ConvertFrom-Json
    $appId = [string]$buildSpec.uuids.windowsApp
    $displayName = if ($buildSpec.displayName) { [string]$buildSpec.displayName } else { 'Vertical Shorts Plugin' }
    $ver = [string]$buildSpec.version
    $packageStamp = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    $metaPath = Join-Path $dstPlugin 'install-meta.ini'
    @"
[Install]
DisplayName=$displayName
DisplayVersion=$ver
AppId={$appId}
InstallDir={autopf}\obs-studio
PluginDll={autopf}\obs-studio\obs-plugins\64bit\obs-shorts-vertical.dll
PluginData={autopf}\obs-studio\data\obs-plugins\obs-shorts-vertical
PackageTimestampUtc=$packageStamp
ConfigLocation=OBS scene collection key obs-shorts-vertical + Windows Credential Manager
Notes=DLL under obs-plugins\64bit; data under data\obs-plugins\obs-shorts-vertical. User config is never overwritten.
"@ | Set-Content -Path $metaPath -Encoding UTF8
    Write-Host "Wrote install-meta.ini (AppId={$appId}, version=$ver, PackageTimestampUtc=$packageStamp)"

    return $dstPlugin
}

function Build-InnoSetupInstaller {
    param(
        [string] $ProjectRoot,
        [string] $StageRoot,
        [string] $ProductVersion,
        [string] $SetupBaseName,
        [string] $OutputDir
    )

    $iss = Join-Path $ProjectRoot 'installer\windows\VerticalShortsPlugin.iss'
    if ( ! ( Test-Path $iss ) ) {
        throw "Inno Setup script missing: $iss"
    }

    $iscc = Find-ISCC
    if ( ! $iscc ) {
        throw @"
ISCC.exe (Inno Setup 6 compiler) not found.
Install Inno Setup 6 (https://jrsoftware.org/isinfo.php) or run:
  choco install innosetup --no-progress -y
"@
    }

    $outExe = Join-Path $OutputDir "${SetupBaseName}.exe"
    if ( Test-Path $outExe ) {
        Remove-Item -Force $outExe
    }

    Log-Group "Compiling Inno Setup installer with $iscc ..."
    Write-Host "SourceDir (staged): $StageRoot"
    Write-Host "Output: $outExe"

    # ISCC requires quoted /D values when paths or filenames contain spaces.
    $argList = @(
        "/DMyAppVersion=$ProductVersion"
        "/DSourceDir=$StageRoot"
        "/DOutputDir=$OutputDir"
        "/DOutputBaseFilename=$SetupBaseName"
        $iss
    )
    Write-Host ("ISCC args: " + ($argList -join ' '))
    & $iscc @argList
    if ( $LASTEXITCODE -ne 0 ) {
        throw "ISCC.exe failed with exit code $LASTEXITCODE"
    }
    if ( ! ( Test-Path $outExe ) ) {
        throw "Inno Setup did not produce expected installer: $outExe"
    }

    Write-Host "Inno Setup installer ready: $outExe ($((Get-Item $outExe).Length) bytes)"
    Log-Group
    return $outExe
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse
    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version
    $DisplayName = if ($BuildSpec.displayName) { [string]$BuildSpec.displayName } else { 'Vertical Shorts Plugin' }

    # Official public artifact names:
    #   Vertical-Shorts-Plugin-1.0.6.zip
    #   Vertical-Shorts-Plugin-1.0.6-Setup.exe
    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"
    $OfficialZipBase = "Vertical-Shorts-Plugin-${ProductVersion}"
    $SetupBaseName = "Vertical-Shorts-Plugin-${ProductVersion}-Setup"

    $ReleaseDir = "${ProjectRoot}/release/${Configuration}"
    $StageRoot = "${ProjectRoot}/release/staging"
    $ReleaseOut = "${ProjectRoot}/release"

    if (Test-Path "${ProjectRoot}/INSTALL-WINDOWS.txt") {
        Copy-Item -Force "${ProjectRoot}/INSTALL-WINDOWS.txt" "${ReleaseDir}/INSTALL.txt"
        $pluginInstall = Join-Path $ReleaseDir 'obs-shorts-vertical\INSTALL.txt'
        if (Test-Path (Split-Path -Parent $pluginInstall)) {
            Copy-Item -Force "${ProjectRoot}/INSTALL-WINDOWS.txt" $pluginInstall
        }
    }

    Get-ChildItem -Path $ReleaseDir -Recurse -Filter *.pdb -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue

    # Platform icons are embedded in the DLL via Qt resources (vsp-resources.qrc).
    $iconsDir = Join-Path $ReleaseDir 'obs-shorts-vertical\data\icons'
    if (Test-Path $iconsDir) {
        Remove-Item -Recurse -Force $iconsDir
    }

    # Keep only locale data next to the DLL (OBS module locale).
    $dataDir = Join-Path $ReleaseDir 'obs-shorts-vertical\data'
    if (Test-Path $dataDir) {
        Get-ChildItem -Path $dataDir -Force -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ne 'locale' } |
            Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    }

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
            "${ProjectRoot}/release/Vertical-Shorts-Plugin*.zip"
            "${ProjectRoot}/release/Vertical-Shorts-Plugin-Setup.exe"
            "${ProjectRoot}/release/Vertical-Shorts-Plugin-*-Setup.exe"
            "${ProjectRoot}/release/Vertical Shorts Plugin*.zip"
            "${ProjectRoot}/release/Vertical Shorts Plugin*Setup.exe"
            "${ProjectRoot}/release/VerticalShortsPlugin-*"
            "${ProjectRoot}/release/ShortsVertical-*"
            "${ProjectRoot}/release/Package"
            "${ProjectRoot}/release/setup-build"
            "${ProjectRoot}/release/staging"
        )
    }
    Remove-Item @RemoveArgs -Recurse

    # --- 1) Verify built plugin payload ---
    Log-Group "Verifying built plugin payload..."
    $pluginRoot = Join-Path $ReleaseDir 'obs-shorts-vertical'
    $installTxt = Join-Path $ReleaseDir 'INSTALL.txt'
    Assert-PluginPayload -PluginRoot $pluginRoot -InstallTxt $installTxt
    Log-Group

    # --- 2) Stage final payload (never package from source tree) ---
    Log-Group "Staging installer payload..."
    $stagedPlugin = New-InstallerStaging -ProjectRoot $ProjectRoot -ReleaseDir $ReleaseDir -StageRoot $StageRoot
    Assert-PluginPayload -PluginRoot $stagedPlugin -InstallTxt (Join-Path $stagedPlugin 'INSTALL.txt')
    Write-Host "Staged plugin tree: $stagedPlugin"
    Log-Group

    # --- 3) Zip from staged payload ---
    Log-Group "Archiving ${DisplayName} zip package from staging..."
    $zipStaging = Join-Path $ReleaseOut 'zip-staging'
    if ( Test-Path $zipStaging ) { Remove-Item -Recurse -Force $zipStaging }
    New-Item -ItemType Directory -Path $zipStaging | Out-Null
    Copy-Item -Recurse -Force $stagedPlugin (Join-Path $zipStaging 'obs-shorts-vertical')
    Copy-Item -Force (Join-Path $StageRoot 'INSTALL.txt') (Join-Path $zipStaging 'INSTALL.txt')

    $CompressArgs = @{
        Path = (Get-ChildItem -Path $zipStaging)
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Copy-Item -Force "${ProjectRoot}/release/${OutputName}.zip" "${ProjectRoot}/release/${OfficialZipBase}.zip"
    Remove-Item -Recurse -Force $zipStaging -ErrorAction SilentlyContinue
    Log-Group

    # --- 4) Package staged payload with Inno Setup ---
    Build-InnoSetupInstaller `
        -ProjectRoot $ProjectRoot `
        -StageRoot $StageRoot `
        -ProductVersion $ProductVersion `
        -SetupBaseName $SetupBaseName `
        -OutputDir $ReleaseOut | Out-Null
}

Package
