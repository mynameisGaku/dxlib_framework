# dxlib_framework

**DxLibを、資源管理・入力・描画・Scene・GameObject・Componentから使えるようにするC++20フレームワークです。** 内部は担当処理ごとに分離し、ゲーム側は `Run<TScene>()`、`Spawn<T>()`、`AddComponent<T>()` とOn系フックを中心に実装します。Unreal Engineへの依存はありません。

## 0.3.0について

0.2.0のソースと履歴を引き継いだ、終了・再入・描画失敗を中心とする更新版です。音声ポーリング中の終了で起きるクラッシュ、資源の二重解放呼び出し、Scene差し替え時の再入、終了要求後の子Object実行を修正しました。描画失敗の扱いも統一しています。

**GCC Debug／ReleaseとClang ASan／UBSanで、138件の基盤・回帰テスト＋14件の代替DxLib契約テストが通過しました。** Pythonの配布・検証ツールは9件通過しています。ZIP展開後の新規ビルドでも同じ152件＋9件を確認しました。詳細は検証結果を参照してください。**実DxLib SDK・Windows・MSVC・実画面・音声・入力機器では未検証です。** Windows用スクリプトやCIを同梱したことを、実機での成功とは扱っていません。

**0.2からの注意：** Nativeコールバックが失敗した場合、状態を復元できても、そのフレームは表示しません。低レベルの独自ライフサイクル拡張にも変更があります。[移行時の注意](Docs/Migration_0.3.md)を確認してください。

[検証結果](Docs/ValidationReport.md) ／ [変更履歴](Docs/CHANGELOG.md) ／ [設計と責務](Docs/Architecture.md) ／ [API](Docs/API.md) ／ [命名規則](Docs/CodingStandard.md) ／ [制限事項](Docs/Limitations.md)

## Windowsで動かす

### Visual Studioのソリューションを生成する

通常の入口はルートの`dxlib_framework.slnx`（Visual Studioの世代によっては`.sln`）です。`dxf_toolbox`・`dxf_foundation`・`dxf_support`・`dxf_runtime`・`dxf_gameplay`・`dxf_native_backends`・`Sandbox`の7プロジェクトだけを表示します。Toolboxを含め、ユーザーが利用する型とヘッダーを各プロジェクトから参照できます。テストやCMake補助プロジェクトは開発用へ分離しています。

初回は `Setup.cmd` でDxLib SDKを用意し、ルートの `GenerateProjectFiles.bat` をダブルクリックします。Visual StudioのC++開発ツールとCMakeを自動検出し、`dxlib_framework.sln`（環境によっては `.slnx`）を生成します。Visual Studioで開き、Debug／Release・x64を選択してビルドできます。生成だけではビルドしません。

生成時はCMakeの構成キャッシュを作り直し、環境変数や過去のキャッシュにある外部ツールチェーン設定を無効化して、検出したMSVCを直接使います。システムの環境変数は変更しません。

```powershell
.\GenerateProjectFiles.bat -Open       # 生成後にソリューションを開く
.\GenerateProjectFiles.bat -NoPause    # キー入力を待たずに終了
.\GenerateProjectFiles.bat -Portable   # DxLib不要のソリューション
.\GenerateProjectFiles.bat -Development # テスト・補助プロジェクトを含む開発用
```

`-Portable` のソリューションはルートの `dxlib_framework-portable.sln`（または `.slnx`）です。プロジェクト・ビルド出力は通常版が `Build/VisualStudio`、Portable版が `Build/VisualStudio-portable` にまとまります。ヘッダーは各プロジェクト内にフォルダ構成に沿って表示されます。SDKは環境変数 `DXLIB_ROOT`、または `Setup.cmd` が作るマニフェストから検出します。引数なしの場合は結果を確認できるよう終了時にキー入力を待ちます。既存のNinjaビルドとは別のフォルダを使い、再実行でプロジェクトを更新できます。

`-Development`はルートの`dxlib_framework-development.slnx`（または`.sln`）へ出力し、通常版と別の`Build/VisualStudio-development`で生成します。`-Portable -Development`も組み合わせられ、`Build/VisualStudio-portable-development`を使います。通常版と開発版は互いの生成結果を上書きしません。`Build`内のソリューションはCMake管理用の原本なので、普段はルートのソリューションを開いてください。ヘッダー追加や構成変更の後は`GenerateProjectFiles.bat`を再実行します。

### コマンドからビルド・検証する

2026-09-12の追加検証では、MSVC Debugで153件のテストと実SDKの12フレーム動作確認が通過しました。MSVC Releaseの非実機テストも153件通過しています。ただし、DxLib VC 3.25aのReleaseリンクにはFBX関連の未解決参照が残っています。[現在のWindows検証結果](Docs/WindowsIntegration.md)を確認してください。

Visual Studioの「C++によるデスクトップ開発」、x64ツール、Windows SDK、CMake Toolsが必要です。通常のPowerShellで、展開したフォルダーへ移動して実行できます。スクリプトが `vswhere` と `VsDevCmd` でx64環境を設定し、Visual Studio同梱のCMake／Ninjaも探索します。

```powershell
# 初回だけ。公式DxLib VC 3.25aをThirdPartyへダウンロード・展開します。
.\Setup.cmd

# Debugの本体、Sandbox、NativeSmokeをビルドし、非実機テストを実行します。
.\Tools\Build.cmd

# サンプルを起動します。
.\Build\windows-debug\Sandbox.exe
```

すでにSDKがある場合、Setupは不要です。`$env:DXLIB_ROOT` を展開済みの公式VC版SDKルート、または `DxLib.h` と `.lib` があるディレクトリに設定してからBuildを実行してください。SDK・フォント・Windows実行ファイルは配布ZIPに含めていません。

Releaseは `Tools\Build.cmd -Configuration Release`。両構成と、実機用の自動終了するAPIスモークテストを実行する入口は次です。

```powershell
.\Tools\Validate.cmd
```

Validateはウィンドウを開き、画像・日本語パス・文字・RenderTarget・入力取得・独立した音声再生・終了処理を確認します。結果と失敗理由は `Build/WindowsValidation/Summary.json` と同ディレクトリのログに残します。**このプログラムは画素の正しさ・実際に聞こえた音・物理キー操作を自動判定するものではありません。** 詳細は[Windowsでの確認](Docs/WindowsValidation.md)を参照してください。

音声の再生確認を省く場合は `Tools\Build.cmd -AllConfigurations -RunDeviceSmoke`。ただしDxLib自体の初期化設定から音声機能を無効化するわけではありません。ヘッドレス環境では実機テストを省き、`Tools\Build.cmd -AllConfigurations` を使います。

Sandboxは **WASDで移動、Spaceで効果音、Pでポーズ、EnterでScene切り替え、Escで終了**。Sceneを変更しても、GameInstanceの訪問回数が残ります。サンプルのゲーム側コード自体も回帰テストでコンパイル・実行しています。

## 小さなSceneから始める

GameObjectを使うことは必須ではありません。Sceneへ直接処理を書いて始められます。

```cpp
#include "Dxf/NativeRun.h"

class DMyScene final : public Dxf::DScene
{
protected:
	void OnTick(const Dxf::FTickContext& Context) override
	{
		if (Context.Input.WasPressed(Dxf::EKey::Escape))
		{
			Context.Scenes->RequestQuit();
		}
	}
};

// WinMainなどから呼び、失敗時はResult.Error()を表示します。
Dxf::TResult<void> RunMyGame()
{
	Dxf::FApplicationSettings Settings;
	Settings.Window.Title = "My Game";
	return Dxf::Run<DMyScene>(Settings);
}
```

独自のGameInstanceが必要な場合は、[WindowsMain.cpp](Examples/Sandbox/WindowsMain.cpp)の `FDxLibBackends` → `FApplication` → `FAppRunner` の構成を使用します。[SandboxGame.cpp](Examples/Sandbox/SandboxGame.cpp)にはScene／Player／Componentの例があります。

## 実装範囲

| 分野 | 内容 |
|---|---|
| Toolbox | STL非依存の配列・文字列・所有ポインタ、SIMD数学、行列・複素数・Quaternion、形状判定と自動四分木／八分木 |
| Foundation | RTTI、結果型、世代付き非所有ハンドル、所有ストレージ、時間、RAII補助、UTF-8検証 |
| 入力 | Snapshot、押下／保持／解放、マウス、最大4Pad、キーボード／マウス／PadのAction割り当て・変更・解除 |
| 資源 | 画像・音・フォント、専任Loader、弱参照Cache、共有参照、終了時の強制無効化、RenderTarget |
| 描画 | 画像・文字・矩形、安定した描画順、色／透明度、Target切り替え、明示的なNative区間 |
| 音 | 音データと再生の分離、独立した停止／音量、Scene単位の停止 |
| 実行基盤 | メインループ、GameInstance、Scene切り替え、失敗時の後始末、終了順序 |
| Gameplay | 遅延生成・破棄、自動ライフサイクル、更新順、ポーズ、型付き検索、任意利用のSpriteRendererComponent |

ToolboxにはAABB・OBB・Sphere・Cube・Convex・Meshの衝突判定と、自動選択する四分木／八分木を追加しています。剛体の物理応答・3Dモデル描画・エディター・非同期ロードは実装範囲に含みません。[Toolboxの使い方と制約](Docs/Toolbox.md)を参照してください。

フレームワーク内とサンプル・テストのSTL依存はToolboxへ移行しています。公開APIもToolbox型に変わるため、既存利用コードの所有ポインタ・文字列・パス・コールバックを更新してください。数値型とコメント形式は[コーディング規則](Docs/CodingStandard.md)にまとめています。`python Tools/CheckNoStl.py`でSTL再混入を検査できます。

## 既存CMakeプロジェクトへ組み込む

```cmake
add_subdirectory(ThirdParty/dxlib_framework)
target_link_libraries(MyGame PRIVATE dxf::framework)
```

実DxLibを使う場合は、追加前に `DXF_BUILD_NATIVE=ON` と `DXLIB_ROOT` を設定し、`dxf::native` へリンクします。利用側も `/MTd`・`/MT` に揃えます。

```cmake
set(DXF_BUILD_NATIVE ON CACHE BOOL "" FORCE)
set(DXLIB_ROOT "C:/Libraries/DxLib_VC" CACHE PATH "" FORCE)
add_subdirectory(ThirdParty/dxlib_framework)
add_executable(MyGame WIN32 Main.cpp)
target_link_libraries(MyGame PRIVATE dxf::native)
set_property(TARGET MyGame PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

必要な層だけを選べます。`dxf::foundation` → `dxf::support` → `dxf::runtime` → `dxf::gameplay` の順に依存し、`dxf::framework` は全体の入口です。`dxf::native_backends` はSupportと実SDKのみへ依存するため、Sceneを使わずにDxLibラッパーだけを利用できます。

通常の外部組み込みではテスト・サンプル・インストールルールは既定でOFFです。Nativeを明示的にONにするとNativeSmokeも既定でONになります。不要なら `DXF_BUILD_NATIVE_SMOKE=OFF` に設定します。

### インストールしたパッケージを使う

```powershell
cmake --install Build/windows-release --prefix C:/Libraries/dxlib_framework
```

```cmake
find_package(dxlib_framework 0.3 CONFIG REQUIRED)
add_executable(MyGame WIN32 Main.cpp)
target_link_libraries(MyGame PRIVATE dxf::native)
dxf_use_static_runtime(MyGame)
```

利用側のCMakeに `CMAKE_PREFIX_PATH` と `DXLIB_ROOT` を渡します。DxLib SDKをパッケージ内へコピーすることはありません。Nativeなしでインストールしたパッケージでは `dxf::framework` などを利用し、`dxf::native` は存在しません。

## DxLibなしで検証する

```sh
cmake --preset portable-debug
cmake --build --preset portable-debug
ctest --preset portable-debug

# Linux: Debug / Release / ASan / UBSan / 公開ヘッダー単独 / 外部組み込み
python Tools/Validate.py --with-sanitizers

# インストール → 別の場所へ移動 → find_package → リンク・実行
python Tools/ValidatePackage.py

# 配布・検証スクリプトの9テスト
python -m unittest discover -s Tools/Tests -v
```

C++は138件の基盤・回帰テストと14件の接続部契約テストを、2つのCTest実行ファイルへ収録しています。接続部契約テストに使う `Tests/FakeDxLib/DxLib.h` は公式SDKではありません。

Pythonは通常のWindowsビルドには不要です。`.github/workflows/ci.yml` はLinux検証とWindows／実SDKのコンパイル・リンクを行う設定です。この配布の作成時点ではリモートのCIは実行していません。

## ソース構成と配布

```text
Source/Foundation/        型・所有・時間・UTF-8
Source/DxLibSupport/      資源・入力・描画・音
Source/Runtime/           Application・Scene・ライフサイクル
Source/Gameplay/          GameObject・Component・GameScene
Source/Native/            実DxLibへの接続
Examples/Sandbox/        操作可能なサンプル
Tests/                   基盤・回帰・接続契約・実機Smoke
Docs/                    API・規約・検証結果・Red/Greenログ
Tools/                   セットアップ・検証・配布
Assets/                  再生成可能なBMP・WAVのみ
```

`python Tools/PackageRelease.py --output ../dxlib_framework_0.3.0.zip` で再配布用ZIPを生成できます。各ファイルのSHA-256はZIP内の `DistributionManifest.json` に、ZIP全体の値は隣の `.sha256` に記録されます。SDK・ビルド出力・Git内部情報・フォントファイルは含めません。
