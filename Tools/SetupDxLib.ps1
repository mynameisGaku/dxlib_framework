#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Version = '3.25a',
    [string]$ExpectedSha256 = '',
    [switch]$Force
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$ThirdParty = Join-Path $Root 'ThirdParty'
$Destination = Join-Path $ThirdParty ('DxLib-' + $Version)
$Manifest = Join-Path $ThirdParty 'dxlib-sdk.json'
if ($Version -notmatch '^\d+\.\d+[a-z]?$') { throw 'Invalid DxLib version.' }
if ($ExpectedSha256 -and $ExpectedSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'ExpectedSha256 must contain 64 hex characters.' }
if ((Test-Path $Manifest) -and -not $Force) {
    $Existing = Get-Content -Raw $Manifest | ConvertFrom-Json
    if ($Existing.version -eq $Version -and (Test-Path (Join-Path $Existing.include_directory 'DxLib.h'))) {
        if ($ExpectedSha256 -and $Existing.sha256 -ne $ExpectedSha256.ToLowerInvariant()) { throw 'Cached SDK hash differs from ExpectedSha256. Use -Force to download again.' }
        Write-Host ('Using existing SDK: ' + $Existing.include_directory)
        return
    }
}
New-Item -ItemType Directory -Force -Path $ThirdParty | Out-Null
$ArchiveName = 'DxLib_VC' + $Version.Replace('.', '_') + '.zip'
$Url = 'https://dxlib.xsrv.jp/DxLib/' + $ArchiveName
$Archive = Join-Path $ThirdParty $ArchiveName
$Partial = $Archive + '.partial'
$Stage = Join-Path $ThirdParty ('extract-' + [guid]::NewGuid().ToString('N'))
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Write-Host ('Downloading the official SDK: ' + $Url)
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Partial -TimeoutSec 600
    $Hash = (Get-FileHash -LiteralPath $Partial -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ExpectedSha256 -and $Hash -ne $ExpectedSha256.ToLowerInvariant()) { throw 'SDK SHA-256 mismatch.' }
    if ((Get-Item $Partial).Length -lt 1024) { throw 'SDK download is unexpectedly small.' }
    Move-Item -LiteralPath $Partial -Destination $Archive -Force
    Expand-Archive -LiteralPath $Archive -DestinationPath $Stage
    $Header = Get-ChildItem -LiteralPath $Stage -Filter DxLib.h -File -Recurse |
        Where-Object { @(Get-ChildItem -LiteralPath $_.DirectoryName -Filter '*.lib' -File).Count -gt 0 } |
        Select-Object -First 1
    if ($null -eq $Header) { throw 'No DxLib.h with VC libraries found in the official archive.' }
    $Relative = $Header.DirectoryName.Substring($Stage.Length).TrimStart('\', '/')
    if (Test-Path $Destination) {
        if (-not $Force) { throw ('Destination exists; use -Force explicitly: ' + $Destination) }
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    Move-Item -LiteralPath $Stage -Destination $Destination
    $Include = Join-Path $Destination $Relative
    [ordered]@{
        version = $Version
        url = $Url
        sha256 = $Hash
        hash_verified_against_expected = [bool]$ExpectedSha256
        include_directory = $Include
        downloaded_at_utc = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json | Set-Content -LiteralPath $Manifest -Encoding UTF8
    Write-Host ('SDK installed: ' + $Include)
    Write-Host ('SHA-256 recorded: ' + $Hash)
    if (-not $ExpectedSha256) {
        Write-Warning 'No expected digest was supplied. The recorded hash is a download record, not independent authenticity verification.'
    }
} finally {
    if (Test-Path $Partial) { Remove-Item -LiteralPath $Partial -Force }
    if (Test-Path $Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force }
}
