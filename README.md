# dxlib_framework

DxLibを描画・入力・音声の実装として使う、C++20のゲームフレームワークです。
Windows / Visual Studio / x64を対象にしています。Source・Examples・TestsではSTLを使わず、共通機能はToolboxを使います。

ゲーム固有の処理に集中できることを主目的とし、内部の責務分離と利用側の単純な公開APIを両立します。
描画・音声だけ、Sceneまで、GameObject／Componentまで、必要な層を選んで利用できます。

## 開発を始める

Visual Studioの「C++によるデスクトップ開発」（Windows SDKとCMakeを含む）を導入し、ルートで実行します。

```bat
Setup.cmd
GenerateProjectFiles.bat
```

`dxlib_framework.slnx`を開いてDebugまたはReleaseでビルドします。
通常のソリューションはToolbox・フレームワーク・Sandbox・Starterを対象とします。
`Setup.cmd`はDxLibの取得とソースビルドを行います。モデルの読み込みにAutodesk FBX SDKは不要です。

空の開始点はStarter、小さいゲームの流れはSandboxを起動してください。Sandboxはタイトル→プレイ→結果→リトライを公開APIだけで構成しています。
[小さいゲームを組む](Docs/SmallGame.md)で、処理を置く場所と操作を確認できます。

テストやModelViewerを使う場合は、開発用ソリューションを生成します。

```bat
GenerateProjectFiles.bat -Development
```

`dxlib_framework-development.slnx`を開き、ModelViewerを起動対象にするとFBXモデルとアニメーションを確認できます。Vキーで[左右2ビュー](Docs/Rendering/Viewports.md)、Pキーで[球・箱のクリック選択](Docs/Rendering/ViewCoordinates.md)、Oキーで正射影を切り替えられます。
生成物は`Build/`、ダウンロードしたSDKは`ThirdParty/`へ置きます。

## アセットとモデル

相対アセットパスは、ルートのソリューションを配置したフォルダーが基準です。
実行ファイル横の`.dxfpaths`はビルド時に生成され、起動時の作業ディレクトリには依存しません。
配布時は`.dxfpaths`を外し、実行ファイルと`Assets`を同じフォルダーへ配置します。

- [アセットのパス規則](Docs/Assets/Paths.md)
- [FBXモデル・アニメーションの利用手順](Docs/DxLibFbx.md)
- [モーフ・追加UV・頂点色・基本PBR・カメラ／ライトの対応範囲](Docs/FbxSupport.md)
- [APIとライフサイクル](Docs/API.md)
- [設計](Docs/Architecture.md) / [C++規約](Docs/CodingStandard.md)
- [検証手順](Docs/Testing.md) / [基本PBRの検証記録](Docs/Development/FbxBasicPbr-2026-09-23.md)

## 検証

```powershell
python Tools/CheckNoStl.py
python -m unittest discover -s Tools/Tests -v
cmake --build Build/VisualStudio-development --config Debug
ctest --test-dir Build/VisualStudio-development -C Debug --output-on-failure
```

実際の描画を伴う試験は明示的に`DXF_RUN_DEVICE_TESTS=ON`を指定した構成で実行します。
配布用ZIPは`Tools/PackageRelease.py`で作成します。ufbxのソースとライセンスを含め、SDK・ビルド生成物は含めません。

## 過去の統合資料

旧パッチ適用スクリプト、差分、旧配布README、当時の状態記録は[履歴保存先](Docs/Archive/IntegrationPackages/README.md)に移しました。
この保存先はGitリポジトリ内の履歴で、ソースZIPには同梱しません。現在のソースへ旧パッチを再適用する必要はありません。履歴の検証結果は当時のものとして保存しています。
