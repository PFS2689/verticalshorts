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
    throw "Build-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "A 64-bit system is required to build the project."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The obs-studio PowerShell build script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Build {
    trap {
        Pop-Location -Stack BuildTemp -ErrorAction 'SilentlyContinue'
        Write-Error $_
        Log-Group
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach($Utility in $UtilityFunctions) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    Push-Location -Stack BuildTemp
    Ensure-Location $ProjectRoot

    $CmakeArgs = @('--preset', "windows-ci-${Target}")
    $CmakeBuildArgs = @('--build')
    $CmakeInstallArgs = @()

    if ( $DebugPreference -eq 'Continue' ) {
        $CmakeArgs += ('--debug-output')
        $CmakeBuildArgs += ('--verbose')
        $CmakeInstallArgs += ('--verbose')
    }

    $CmakeBuildArgs += @(
        '--preset', "windows-${Target}"
        '--config', $Configuration
        '--parallel'
        '--', '/consoleLoggerParameters:Summary', '/noLogo'
    )

    $CmakeInstallArgs += @(
        '--install', "build_${Target}"
        '--prefix', "${ProjectRoot}/release/${Configuration}"
        '--config', $Configuration
    )

    # Fresh UTC stamp for this CI configure — avoids inheriting stale "Last Updated" metadata.
    $BuildStamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    $CmakeArgs += "-DPLUGIN_BUILD_TIMESTAMP=$BuildStamp"
    Write-Host "PLUGIN_BUILD_TIMESTAMP=$BuildStamp"

    Log-Group "Configuring ${ProductName}..."
    Invoke-External cmake @CmakeArgs

    Log-Group "Building unit tests..."
    Invoke-External cmake --build "build_${Target}" --config $Configuration --target vsp_tests

    Log-Group "Running ctest..."
    $QtBin = Get-ChildItem -Path "$ProjectRoot/.deps" -Directory -Filter "obs-deps-qt6-*-${Target}" |
        Select-Object -First 1 -ExpandProperty FullName
    if ($QtBin) {
        $env:PATH = "$(Join-Path $QtBin 'bin');$env:PATH"
        Write-Information "Added Qt bin to PATH: $(Join-Path $QtBin 'bin')"
    }
    Push-Location "build_${Target}"
    Invoke-External ctest -C $Configuration --output-on-failure --timeout 120
    Pop-Location

    Log-Group "Building ${ProductName}..."
    Invoke-External cmake @CmakeBuildArgs

    Log-Group "Installing ${ProductName}..."
    Invoke-External cmake @CmakeInstallArgs

    # Guard: test binaries must never land in the plugin package tree
    $TestLeak = Get-ChildItem -Path "${ProjectRoot}/release" -Recurse -Filter "test_*" -ErrorAction SilentlyContinue
    if ($TestLeak) {
        throw "Test binaries leaked into release tree: $($TestLeak.FullName -join ', ')"
    }

    Pop-Location -Stack BuildTemp
    Log-Group
}

Build
