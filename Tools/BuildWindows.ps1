#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$AllConfigurations,
    [switch]$DownloadSdk,
    [switch]$RunDeviceSmoke,
    [switch]$Audio,
    [switch]$Clean,
    [ValidateRange(1, 64)][int]$Jobs = 4
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Logs = Join-Path $Root 'Build/WindowsValidation'
New-Item -ItemType Directory -Force -Path $Logs | Out-Null
$Summary = [ordered]@{
    timestamp_utc = [DateTime]::UtcNow.ToString('o')
    real_sdk_compiled_and_linked = $false
    real_sdk_api_smoke_passed = $false
    audio_api_smoke_passed = $false
    visual_output_inspected = $false
    audible_output_inspected = $false
    profiles = @()
    passed = $false
}
function Invoke-Logged {
    param([string]$Name, [string]$Executable, [string[]]$Arguments)
    $Log = Join-Path $Logs ($Name + '.log')
    ('$ ' + $Executable + ' ' + ($Arguments -join ' ')) | Set-Content -LiteralPath $Log -Encoding UTF8
    $SavedPreference = $ErrorActionPreference
    try {
        # PS 5.1 treats native stderr as ErrorRecords. The native exit code is authoritative.
        $ErrorActionPreference = 'Continue'
        $Output = @(& $Executable @Arguments 2>&1)
        $Code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $SavedPreference
    }
    $Text = ($Output | ForEach-Object { $_.ToString() }) -join "`n"
    $Text | Add-Content -LiteralPath $Log -Encoding UTF8
    ('EXIT_CODE=' + $Code) | Add-Content -LiteralPath $Log -Encoding UTF8
    Write-Host $Text
    if ($Code -ne 0) { throw ($Name + ' failed. See ' + $Log) }
    return $Text
}
function Import-VisualStudio {
    $VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path $VsWhere)) { throw 'Install Visual Studio C++ Desktop workload, Windows SDK, and CMake tools first.' }
    $Installation = & $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $Installation) { throw 'Visual Studio x64 C++ tools were not found.' }
    $DevCmd = Join-Path $Installation 'Common7/Tools/VsDevCmd.bat'
    $Temp = Join-Path $env:TEMP ('dxf-env-' + [guid]::NewGuid().ToString('N') + '.cmd')
    try {
        @('@echo off', ('call "' + $DevCmd + '" -no_logo -arch=x64 -host_arch=x64 >nul'),
          'if errorlevel 1 exit /b 1', 'set') | Set-Content -LiteralPath $Temp -Encoding Default
        $Environment = & $env:ComSpec /d /c $Temp
        if ($LASTEXITCODE -ne 0) { throw 'VsDevCmd failed.' }
        foreach ($Line in $Environment) {
            if ($Line -match '^([^=]+)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
            }
        }
    } finally {
        if (Test-Path $Temp) { Remove-Item -LiteralPath $Temp -Force }
    }
    $CMakeTools = Join-Path $Installation 'Common7/IDE/CommonExtensions/Microsoft/CMake'
    foreach ($Subdirectory in @('CMake/bin', 'Ninja')) {
        $Candidate = Join-Path $CMakeTools $Subdirectory
        if (Test-Path $Candidate) { $env:PATH = $Candidate + ';' + $env:PATH }
    }
    foreach ($Command in @('cl.exe', 'cmake.exe', 'ctest.exe', 'ninja.exe')) {
        if (-not (Get-Command $Command -ErrorAction SilentlyContinue)) { throw ('Missing build tool: ' + $Command) }
    }
}
Push-Location $Root
try {
    if ($Audio -and -not $RunDeviceSmoke) { throw '-Audio requires -RunDeviceSmoke.' }
    Import-VisualStudio
    # 実際の表示を採取し、CMakeの文字コード推測で依存追跡が崩れるのを防ぐ。
    $ProbeDirectory = Join-Path $Logs 'IncludeProbe'
    New-Item -ItemType Directory -Force -Path $ProbeDirectory | Out-Null
    $ProbeHeader = Join-Path $ProbeDirectory 'dxf-includes.h'
    $ProbeSource = Join-Path $ProbeDirectory 'dxf-includes.cpp'
    '// include probe' | Set-Content -LiteralPath $ProbeHeader -Encoding ASCII
    '#include "dxf-includes.h"' | Set-Content -LiteralPath $ProbeSource -Encoding ASCII
    $ProbeOutput = Invoke-Logged 'include-prefix' 'cl.exe' @('/nologo', '/utf-8', '/EP', '/showIncludes', $ProbeSource)
    $IncludePrefix = $null
    foreach ($Line in ($ProbeOutput -split "`n")) {
        $HeaderOffset = $Line.IndexOf($ProbeHeader, [StringComparison]::OrdinalIgnoreCase)
        if ($HeaderOffset -gt 0) { $IncludePrefix = $Line.Substring(0, $HeaderOffset); break }
    }
    if (-not $IncludePrefix) { throw 'Could not measure the MSVC include dependency prefix.' }
    if ($DownloadSdk) { & (Join-Path $PSScriptRoot 'SetupDxLib.ps1') }
    $SdkRoot = $env:DXLIB_ROOT
    $ManifestPath = Join-Path $Root 'ThirdParty/dxlib-sdk.json'
    if (-not $SdkRoot -and (Test-Path $ManifestPath)) {
        $SdkRoot = (Get-Content -Raw $ManifestPath | ConvertFrom-Json).include_directory
    }
    if (-not $SdkRoot -or -not (Test-Path $SdkRoot)) {
        throw 'DxLib SDK not found. Run Setup.cmd, use -DownloadSdk, or set DXLIB_ROOT.'
    }
    $Summary['sdk_root'] = $SdkRoot
    $Configurations = @($Configuration)
    if ($AllConfigurations) { $Configurations = @('Debug', 'Release') }
    foreach ($Config in $Configurations) {
        $Name = $Config.ToLowerInvariant()
        $Build = Join-Path $Root ('Build/windows-' + $Name)
        $ConfigureMode = @()
        if ($Clean) {
            $ConfigureMode += '--fresh'
            # ZIP復元でソース日時が古くなった場合も、既存のオブジェクトを使い回さない。
            if (Test-Path (Join-Path $Build 'CMakeCache.txt')) {
                Invoke-Logged ($Name + '-clean') 'cmake.exe' @('--build', $Build, '--target', 'clean') | Out-Null
            }
        }
        Invoke-Logged ($Name + '-configure') 'cmake.exe' (@('-S', $Root, '-B', $Build, '-G', 'Ninja',
            '-DCMAKE_CXX_COMPILER=cl', ('-DCMAKE_BUILD_TYPE=' + $Config), ('-DDXLIB_ROOT=' + $SdkRoot),
            ('-DDXF_MSVC_INCLUDE_PREFIX=' + $IncludePrefix),
            '-DDXF_BUILD_NATIVE=ON', '-DDXF_BUILD_EXAMPLE=ON', '-DDXF_BUILD_TESTS=ON',
            '-DDXF_BUILD_NATIVE_SMOKE=ON', '-DDXF_RUN_DEVICE_TESTS=OFF', '-DDXF_INSTALL=ON') + $ConfigureMode) | Out-Null
        Invoke-Logged ($Name + '-build') 'cmake.exe' @('--build', $Build, '--parallel', "$Jobs") | Out-Null
        Invoke-Logged ($Name + '-ctest') 'ctest.exe' @('--test-dir', $Build, '--output-on-failure') | Out-Null
        $Core = Invoke-Logged ($Name + '-framework-cases') (Join-Path $Build 'dxf_tests.exe') @()
        if ($Core -notmatch '(\d+)/(\d+) passed' -or $Matches[1] -ne $Matches[2]) { throw 'Framework case count mismatch.' }
        $CoreCount = [int]$Matches[1]
        $Contract = Invoke-Logged ($Name + '-contract-cases') (Join-Path $Build 'dxf_native_contract_tests.exe') @()
        if ($Contract -notmatch '(\d+)/(\d+) passed' -or $Matches[1] -ne $Matches[2]) { throw 'Contract case count mismatch.' }
        $ContractCount = [int]$Matches[1]
        $Summary.profiles += [ordered]@{ configuration = $Config; framework_cases = $CoreCount; contract_cases = $ContractCount }
        # The real target (not just the double) must have linked before setting this flag.
        if (-not (Test-Path (Join-Path $Build 'NativeSmoke.exe')) -or -not (Test-Path (Join-Path $Build 'Sandbox.exe'))) {
            throw 'The real-SDK executables were not produced.'
        }
        if ($RunDeviceSmoke) {
            Push-Location $Build
            try {
                $SmokeArguments = @()
                if ($Audio) { $SmokeArguments += '--audio' }
                $SmokeOutput = Invoke-Logged ($Name + '-real-sdk-smoke') (Join-Path $Build 'NativeSmoke.exe') $SmokeArguments
                if ($SmokeOutput -notmatch 'REAL_SDK_API_SMOKE_PASSED') { throw 'Native smoke did not report completion.' }
            } finally { Pop-Location }
        }
        Invoke-Logged ($Name + '-install') 'cmake.exe' @('--install', $Build, '--prefix', (Join-Path $Root 'Build/Install')) | Out-Null
    }
    $Summary.real_sdk_compiled_and_linked = $true
    $Summary.real_sdk_api_smoke_passed = [bool]$RunDeviceSmoke
    $Summary.audio_api_smoke_passed = [bool]($RunDeviceSmoke -and $Audio)
    $Summary.passed = $true
    Write-Host ('Validation succeeded. Logs: ' + $Logs)
    Write-Host 'API smoke does not inspect pixels, audible output, or physical key/gamepad operation.'
} catch {
    $Summary['error'] = $_.Exception.Message
    Write-Error -ErrorAction Continue $_
    exit 1
} finally {
    $Summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $Logs 'Summary.json') -Encoding UTF8
    Pop-Location
}
