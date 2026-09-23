#requires -Version 5.1
<#
公式DxLibソース（DxLibMake）から、x64の/MT・/MTd静的ライブラリを再現可能にビルドする。
既定はFBX SDKを使わないビルド（ThirdParty/DxLib-<版>-source）。フレームワークは.fbxをufbxで変換してから
DxLibへ渡すため、この構成でモデルを扱える。公式VCパッケージのRelease版（DxLib_vs2015_x64_MT.lib）は
MV1の読込部がFBX SDKを必要とし、FBX SDKなしではモデル機能をリンクできないため、この再ビルドを使う。
-WithFbxSdkは、DxLib自身のFBX読込を比較するための任意の構成（利用者がFBX SDKを導入・規約に同意済みの場合のみ）。
公式VC SDKは変更せず、別ディレクトリへヘッダー・ライブラリ・マニフェスト（DxLibFbx.json）を出力する。
このスクリプトはSDKのダウンロードや利用規約への同意を行わない。
#>
[CmdletBinding()]
param(
    [string]$SourceZip = '',
    [string]$ExpectedSourceSha256 = '2f09078692d3b64448c6ffe80392d77d0f413ce652115a1d7f0063baf475322a',
    [string]$FbxSdkRoot = '',
    [string]$OutDir = '',
    [switch]$WithFbxSdk
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $SourceZip) { $SourceZip = Join-Path $Root 'ThirdParty/DxLibMake3_25a.zip' }
$NoFbx = -not $WithFbxSdk
if (-not $OutDir) { $OutDir = Join-Path $Root $(if ($NoFbx) { 'ThirdParty/DxLib-3.25a-source' } else { 'ThirdParty/DxLib-3.25a-fbx' }) }
$Work = Join-Path $Root 'Build/DxLibFbx'
New-Item -ItemType Directory -Force -Path $Work | Out-Null

function Get-Sha256([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }
function Invoke-Checked([string]$Name, [string]$Command) {
    $Log = Join-Path $Work ($Name + '.log')
    & $env:ComSpec /d /c ($Command + ' > "' + $Log + '" 2>&1')
    if ($LASTEXITCODE -ne 0) { throw ($Name + ' failed (exit ' + $LASTEXITCODE + '). See ' + $Log) }
}

# 1. Visual Studio x64環境。
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$Installation = & $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $Installation) { throw 'Visual Studio x64 C++ tools were not found.' }
$Temp = Join-Path $env:TEMP ('dxf-env-' + [guid]::NewGuid().ToString('N') + '.cmd')
@('@echo off', ('call "' + (Join-Path $Installation 'Common7/Tools/VsDevCmd.bat') + '" -no_logo -arch=x64 -host_arch=x64 >nul'), 'set') |
    Set-Content -LiteralPath $Temp -Encoding Default
foreach ($Line in (& $env:ComSpec /d /c $Temp)) { if ($Line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') } }
Remove-Item -LiteralPath $Temp -Force

# 2. ソースZIPの照合と展開。以前の展開物は使わない。
if (-not (Test-Path -LiteralPath $SourceZip)) { throw ('DxLib source zip not found: ' + $SourceZip + ' (https://dxlib.xsrv.jp/DxLib/DxLibMake3_25a.zip)') }
$SourceHash = Get-Sha256 $SourceZip
if ($ExpectedSourceSha256 -and $SourceHash -ne $ExpectedSourceSha256) { throw ('DxLib source zip hash differs: ' + $SourceHash) }
$Extract = Join-Path $Work 'source'
if (Test-Path -LiteralPath $Extract) { Remove-Item -LiteralPath $Extract -Recurse -Force }
Expand-Archive -LiteralPath $SourceZip -DestinationPath $Extract
$SourceDir = Join-Path $Extract 'DxLibMake'

# 3. 公式VC SDK（ヘッダーとDxUseCLib・DxDrawFunc）。ソースと同じ版であることをヘッダーで確認する。
$SdkJson = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'ThirdParty/dxlib-sdk.json') | ConvertFrom-Json
$OfficialInclude = $SdkJson.include_directory
foreach ($Header in 'DxLib.h', 'DxCompileConfig.h', 'DxDataType.h', 'DxDataTypeWin.h', 'DxFunctionWin.h') {
    if ((Get-Sha256 (Join-Path $SourceDir $Header)) -ne (Get-Sha256 (Join-Path $OfficialInclude $Header))) {
        throw ('Source and official SDK headers differ: ' + $Header + '. Use the same DxLib version for both.')
    }
}

# 4. FBX SDK（利用者がインストール済みのもの）。
$FbxVersion = ''
$FbxLibs = @{ Debug = @(); Release = @() }
if (-not $NoFbx) {
    if (-not $FbxSdkRoot) {
        $Candidates = @(Get-ChildItem -LiteralPath 'C:\Program Files\Autodesk\FBX\FBX SDK' -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)
        if ($Candidates.Count -eq 0) { throw 'FBX SDK not found. Install it (accepting its license yourself) or pass -FbxSdkRoot.' }
        $FbxSdkRoot = $Candidates[0].FullName
    }
    $VersionHeader = Join-Path $FbxSdkRoot 'include/fbxsdk/fbxsdk_version.h'
    if (-not (Test-Path -LiteralPath $VersionHeader)) { throw ('Not an FBX SDK root: ' + $FbxSdkRoot) }
    # FBXSDK_VERSION_STRINGはマクロの組合せなので、各番号の定義から版を組み立てる。
    $Parts = foreach ($Name in 'MAJOR', 'MINOR', 'POINT') {
        (Select-String -LiteralPath $VersionHeader -Pattern ('#define FBXSDK_VERSION_' + $Name + '\s+(\d+)')).Matches[0].Groups[1].Value
    }
    $FbxVersion = $Parts -join '.'
    foreach ($Config in 'Debug', 'Release') {
        $Sub = $Config.ToLowerInvariant()
        $LibDir = @((Join-Path $FbxSdkRoot "lib/x64/$Sub"), (Join-Path $FbxSdkRoot "lib/vs2022/x64/$Sub")) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (-not $LibDir) { throw ('FBX SDK x64 ' + $Sub + ' library directory not found under ' + $FbxSdkRoot) }
        # 静的CRT版（-mt）。2020系はlibxml2・zlibも必要。存在しないものは追加しない。
        foreach ($Name in 'libfbxsdk-mt.lib', 'libxml2-mt.lib', 'zlib-mt.lib') {
            $Path = Join-Path $LibDir $Name
            if (Test-Path -LiteralPath $Path) { $FbxLibs[$Config] += (Resolve-Path -LiteralPath $Path).Path }
            elseif ($Name -eq 'libfbxsdk-mt.lib') { throw ('Missing ' + $Path) }
        }
    }
}

# 5. ビルド（Visual Studio Generator、両構成）。
$BuildDir = Join-Path $Work $(if ($NoFbx) { 'build-nofbx' } else { 'build-fbx' })
$WithFbx = if ($NoFbx) { 'OFF' } else { 'ON' }
Invoke-Checked 'configure' ('cmake -S "' + $PSScriptRoot + '" -B "' + $BuildDir + '" -A x64 "-DDXF_DXLIB_SOURCE_DIR=' + $SourceDir + '" -DDXF_DXLIB_WITH_FBX=' + $WithFbx + ' "-DDXF_FBX_SDK_ROOT=' + $FbxSdkRoot + '"')
Invoke-Checked 'build-debug' ('cmake --build "' + $BuildDir + '" --config Debug --parallel')
Invoke-Checked 'build-release' ('cmake --build "' + $BuildDir + '" --config Release --parallel')

# 6. 出力SDKの組み立て。公式SDKは変更しない。
if (Test-Path -LiteralPath $OutDir) { Remove-Item -LiteralPath $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $OutDir 'include'), (Join-Path $OutDir 'lib') | Out-Null
Get-ChildItem -LiteralPath $OfficialInclude -Filter *.h | Copy-Item -Destination (Join-Path $OutDir 'include')
$Libraries = [ordered]@{ Debug = @(); Release = @() }
# DX_LIB_NOT_DEFAULTPATHで無効になるDxDataTypeWin.hの自動リンクと同じ組（x64・VS2015以降・静的CRT・既定の反復子デバッグ水準）。
$Bundled = @('DxUseCLib', 'DxDrawFunc', 'libbulletcollision', 'libbulletdynamics', 'libbulletmath', 'libtiff', 'libpng', 'zlib', 'libjpeg',
             'ogg_static', 'vorbis_static', 'vorbisfile_static', 'libtheora_static', 'opus', 'opusfile', 'silk_common', 'celt')
$Map = @{ Debug = @('DxLib_fbx_x64_MTd.lib') + ($Bundled | ForEach-Object { $_ + '_vs2015_x64_MTd.lib' });
          Release = @('DxLib_fbx_x64_MT.lib') + ($Bundled | ForEach-Object { $_ + '_vs2015_x64_MT.lib' }) }
foreach ($Config in 'Debug', 'Release') {
    foreach ($Name in $Map[$Config]) {
        $From = if ($Name -like 'DxLib_fbx_*') { Join-Path $BuildDir ('lib/' + $Config + '/' + $Name) } else { Join-Path $OfficialInclude $Name }
        Copy-Item -LiteralPath $From -Destination (Join-Path $OutDir 'lib')
        $Libraries[$Config] += [ordered]@{ path = ('lib/' + $Name); sha256 = (Get-Sha256 (Join-Path $OutDir ('lib/' + $Name))); origin = $(if ($Name -like 'DxLib_fbx_*') { 'built from source' } else { 'official VC package' }) }
    }
    foreach ($Path in $FbxLibs[$Config]) {
        $Libraries[$Config] += [ordered]@{ path = $Path; sha256 = (Get-Sha256 $Path); origin = 'Autodesk FBX SDK (installed by the user)' }
    }
}
$Compiler = (& $env:ComSpec /d /c 'cl 2>&1' | Select-Object -First 1)
$Manifest = [ordered]@{
    dxlib_version = '3.25a'
    with_fbx = (-not $NoFbx)
    model_extension_version = 1
    model_extension_sha256 = (Get-Sha256 (Join-Path $PSScriptRoot 'DxLibModelExtension.cpp'))
    source_zip = (Split-Path -Leaf $SourceZip)
    source_zip_sha256 = $SourceHash
    official_sdk_zip_sha256 = $SdkJson.sha256
    fbx_sdk_root = $FbxSdkRoot
    fbx_sdk_version = $FbxVersion
    compiler = $Compiler
    runtime = @{ Debug = '/MTd'; Release = '/MT' }
    defines = @{ common = @('WIN32', '_LIB'); fbx = $(if ($NoFbx) { @() } else { @('DX_LOAD_FBX_MODEL') }) }
    # FBX SDK同梱のlibxml2 2.15はBCryptGenRandomを使うため、Windowsのbcrypt.libが必要（実際の未解決シンボルで確認）。
    # @()で囲み、要素が1つでもJSON配列として書き出す。
    system_libraries = @(if (-not $NoFbx) { 'bcrypt.lib' })
    built_utc = [DateTime]::UtcNow.ToString('o')
    libraries = $Libraries
}
$Manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutDir 'DxLibFbx.json') -Encoding UTF8
Write-Host ('DxLib custom build: ' + $OutDir)
Write-Host ('Configure the framework with -DDXF_DXLIB_CUSTOM_ROOT="' + $OutDir + '"')
