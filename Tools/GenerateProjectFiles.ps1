#requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$Portable,
    [switch]$Open,
    [switch]$NoPause
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot

try {
    $VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $VsWhere)) {
        throw 'Install Visual Studio with Desktop development with C++, Windows SDK, and CMake tools.'
    }
    $Installations = @((& $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json))
    if ($Installations.Count -eq 0) { throw 'Visual Studio C++ tools were not found. Install the Desktop development with C++ workload.' }
    $Installation = $Installations[0]
    $Major = ([version]$Installation.installationVersion).Major
    $BundledCMake = Join-Path $Installation.installationPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    if (Test-Path -LiteralPath $BundledCMake) {
        $CMake = $BundledCMake
    } else {
        $CMake = (Get-Command cmake.exe -ErrorAction Stop).Source
    }
    $Capabilities = & $CMake -E capabilities | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw 'Could not query CMake capabilities.' }
    $Generators = @($Capabilities.generators | Where-Object { $_.name -like "Visual Studio $Major *" })
    if ($Generators.Count -eq 0) { throw 'This CMake does not support the installed Visual Studio. Update CMake or install Visual Studio CMake tools.' }

    $Build = Join-Path $Root 'Build/VisualStudio'
    $Native = 'ON'
    $SdkRoot = $env:DXLIB_ROOT
    if ($Portable) {
        $Build = Join-Path $Root 'Build/VisualStudio-portable'
        $Native = 'OFF'
    } else {
        $Manifest = Join-Path $Root 'ThirdParty/dxlib-sdk.json'
        if (-not $SdkRoot -and (Test-Path -LiteralPath $Manifest)) {
            $SdkRoot = (Get-Content -LiteralPath $Manifest -Raw -Encoding UTF8 | ConvertFrom-Json).include_directory
        }
        if (-not $SdkRoot -or -not (Test-Path -LiteralPath $SdkRoot)) {
            throw 'DxLib SDK not found. Run Setup.cmd first, set DXLIB_ROOT, or use -Portable to generate without DxLib.'
        }
        $SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
    }
    $Arguments = @('-S', $Root, '-B', $Build, '-G', $Generators[0].name, '-A', 'x64',
        ('-DCMAKE_GENERATOR_INSTANCE=' + $Installation.installationPath),
        '-DCMAKE_CONFIGURATION_TYPES=Debug;Release', '-DDXF_BUILD_TESTS=ON',
        ('-DDXF_BUILD_NATIVE=' + $Native), ('-DDXF_BUILD_EXAMPLE=' + $Native),
        ('-DDXF_BUILD_NATIVE_SMOKE=' + $Native), '-DDXF_RUN_DEVICE_TESTS=OFF')
    if (-not $Portable) { $Arguments += '-DDXLIB_ROOT=' + $SdkRoot }
    & $CMake @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Solution generation failed. See the CMake output above.' }
    $Solution = Join-Path $Build 'dxlib_framework.sln'
    if (-not (Test-Path -LiteralPath $Solution)) { throw ('Solution was not created: ' + $Solution) }
    Write-Host ('Solution generated: ' + $Solution)
    if ($Open) { Invoke-Item -LiteralPath $Solution }
    exit 0
} catch {
    [Console]::Error.WriteLine('GenerateProjectFiles failed: ' + $_.Exception.Message)
    exit 1
}
