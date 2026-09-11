# dxlib_framework

**DxLibを、資源管理・入力・描画・Scene・GameObject・Componentから使えるようにするC++20フレームワークです。** 内部は担当処理ごとに分け、ゲーム側は `Run<TScene>()`、`Spawn<T>()`、`AddComponent<T>()` とOn系フックを中心に実装します。

## 今回の配布について

設計に基づいた実装初版（0.1.0）です。**Linuxでの基盤・回帰テスト84件と、手書きの代替DxLibヘッダーを使った接続部テスト11件が通過しています。実DxLib SDK・Windows・MSVC・実機の画面／音声／入力は未検証です。** 接続部のテスト成功を、Windows実機動作の保証とは扱わないでください。

前回のLibraryに保存されていたソースZIPは取得に失敗したため、この配布は提示された設計から構築したコードです。取得できなかった旧実装との差分やAPI互換性は検証していません。

[検証結果](Docs/ValidationReport.md) / [設計と責務](Docs/Architecture.md) / [APIの契約](Docs/API.md) / [命名規則](Docs/CodingStandard.md) / [制限事項](Docs/Limitations.md)

## WindowsでSandboxをビルドする

C++20を扱えるMSVC x64、CMake 3.24以上、Ninja、公式のDxLib VC版が必要です。SDK・生成済み実行ファイル・フォントファイルは同梱していません。

C++のx64環境が設定された **Developer PowerShell for VS** で、展開した `dxlib_framework` ディレクトリに移動して実行します。`DXLIB_ROOT` は自分の環境に合わせて変更してください。

```powershell
$env:DXLIB_ROOT = 'C:\Libraries\DxLib_VC\プロジェクトに追加すべきファイル_VC用'
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
.\Build\windows-debug\Sandbox.exe
```

`DxLib.h` と公式SDKの `.lib` が必要です。SDKルート直下に上記の日本語名ディレクトリがある構成も探索します。DxLibのMSVC自動リンク設定を利用し、Native有効時はDebugを `/MTd`、それ以外を `/MT` に揃えます。別のランタイムで作られたSDKを使用する構成は対象にしていません。

`Assets` はビルド時に実行ファイルの隣へコピーします。Sandboxは実行ファイルの場所からアセットを探すため、起動時のカレントディレクトリに依存しません。ただし、このWindows専用の起動処理も実機では未確認です。

Sandboxでは **WASDで移動、Spaceで効果音、Pでポーズ、EnterでScene切り替え、Escで終了**できます。Sceneが変わっても、GameInstanceが保持する訪問回数は残る設計です。サンプルのゲーム側コードは自動テストでも実際にコンパイル・実行しています。

## DxLibなしでテストする

```sh
cmake --preset portable-debug
cmake --build --preset portable-debug
ctest --preset portable-debug
```

GCC／Clang／MSVCで扱える基盤を、実SDKなしでビルドする設定です。接続部テストは `Tests/FakeDxLib/DxLib.h` を使います。CTestの表示は2実行ファイルですが、その内部に95テストケースがあります。

Debug、Release、Clangのサニタイザー、公開ヘッダー単独、別プロジェクトからの組み込みまで再実行する場合は、Linuxで次を使います。

```sh
python Tools/Validate.py --with-sanitizers
```

Pythonは通常のフレームワーク／Sandboxのビルドには不要です。上の検証スクリプトと、同梱アセットの再生成に使用します。

## 小さなSceneから始める

ゲームの構成にGameObjectを使うことは必須ではありません。Sceneに直接処理を書いて始められます。

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

// WinMainなど、アプリケーションの入口から呼びます。
Dxf::TResult<void> RunMyGame()
{
    Dxf::FApplicationSettings Settings;
    Settings.Window.Title = "My Game";
    return Dxf::Run<DMyScene>(Settings);
}
```

独自のGameInstanceや初期設定が必要な場合は、[SandboxのWindowsMain.cpp](Examples/Sandbox/WindowsMain.cpp)のように `FDxLibBackends` → `FApplication` → `FAppRunner` を組み合わせます。Scene／Player／描画Componentの実装例は [SandboxGame.cpp](Examples/Sandbox/SandboxGame.cpp) です。

## 実装しているもの

| 分野 | 内容 |
|---|---|
| Foundation | RTTI、結果型、世代付き非所有ハンドル、所有ストレージ、時間、RAII補助 |
| 入力 | フレーム単位のSnapshot、押下／保持／解放、マウス、基本的なゲームパッド入力、キーのAction割り当て |
| 資源 | 画像・音・フォントのLoader、弱参照キャッシュ、共有参照、終了時の強制無効化、RenderTarget |
| 描画 | 画像・文字・矩形、安定した描画順、色／透明度、Target切り替え、明示的なNative区間 |
| 音 | 音データと再生インスタンスの分離、独立した停止／音量、Scene単位の停止 |
| 実行基盤 | メインループ、GameInstance、Scene切り替え、失敗時の巻き戻し、順序を守った終了 |
| Gameplay | GameObject／Component、遅延生成・破棄、自動ライフサイクル実行、更新順、ポーズ |

衝突、物理、セーブ、エディター、3Dモデル、非同期ロードなどは含んでいません。何でも揃ったゲームエンジンではなく、DxLib上で繰り返し書く基盤をまとめた初版です。

## ディレクトリ

```text
Source/
  Foundation/Public/Dxf/       型・所有・時間
  DxLibSupport/Public/Dxf/     資源・入力・描画・音のAPI
  DxLibSupport/Private/        担当クラスごとの実装
  Runtime/Public/Dxf/          Application・Scene・汎用ライフサイクル
  Runtime/Private/             実行とScene遷移
  Gameplay/Public/Dxf/         GameObject・Component・GameScene
  Native/Public/Dxf/           Windowsヘッダーを含まないDxLib接続API
  Native/Private/              実際のDxLib呼び出し
Examples/Sandbox/              操作できるサンプル
Tests/                        基盤・接続部・サンプルのテスト
Assets/                       再生成可能なBMP・WAVのみ
Docs/                         設計・規約・検証結果・TDDログ
Tools/                        検証・アセット生成
```

## 既存CMakeプロジェクトに組み込む

```cmake
add_subdirectory(ThirdParty/dxlib_framework)
target_link_libraries(MyGame PRIVATE dxf::framework)
```

実DxLib接続が必要な場合は、追加前に `DXF_BUILD_NATIVE=ON` と `DXLIB_ROOT` を設定し、`dxf::native` へリンクします。Native利用時は利用側ターゲットもSDKと同じ `/MTd`・`/MT` に揃えてください。`DXF_BUILD_TESTS` と `DXF_BUILD_EXAMPLE` は、外部プロジェクトに組み込むだけでは有効になりません。

この版では `add_subdirectory` が配布・組み込み方法です。`install()`／`find_package()` 用のパッケージは未提供です。
