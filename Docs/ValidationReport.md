# 検証結果 — dxlib_framework 0.1.0

記録日: **2026年9月12日**。このZIPに収録する実装に対して実際に実行した結果です。取得できなかった以前のLibrary実装の検証結果を転記したものではありません。

## 結果

| 検証 | 結果 |
|---|---|
| 基盤・ライフサイクル・Application・Sandbox | **84 / 84 通過** |
| 代替DxLibヘッダーによるNative接続契約 | **11 / 11 通過** |
| GCC Debug / Release | 両方でコンパイル・CTest・全95ケース通過 |
| Clang Debug + AddressSanitizer + UndefinedBehaviorSanitizer | コンパイル・CTest・全95ケース通過 |
| AddressSanitizerのリーク検査 | 実行経路で報告なし（detect_leaks=1） |
| 公開ヘッダーの単独コンパイル | **63 / 63 通過** |
| Sandbox公開ヘッダーの単独コンパイル | **1 / 1 通過** |
| 別CMakeプロジェクトからの組み込み | add_subdirectory・コンパイル・リンク・実行に成功 |
| 外部組み込みの既定値 | テスト・Native・サンプルがOFFであることを確認 |
| サンプルアセット | BMP 64×64 / 24bit、WAV mono / 16bit / 22,050Hzを構造検査 |
| 実DxLib SDK / Windows / MSVC | **未検証** |
| WindowsMain.cpp・実画面・実音声・実入力機器 | **未検証** |

CTestは2実行ファイルとして登録しているため、集計表示は「2 / 2」です。上記95件は実行ファイル内部のケース数であり、95個のCTestターゲットではありません。

この検査は実行された経路を対象とし、すべての条件で不具合がないことを保証しません。

## 環境

```text
OS: Linux x86_64
c++ (Debian 14.2.0-19) 14.2.0
clang version 17.0.0 (https://github.com/swiftlang/llvm-project.git 10999b6d034fe318f3d56c83bddb6572593a8bb0)
CMake 3.31.6 / Ninja
C++20 / RTTI enabled / C++ exceptions enabled
```

## 生ログ

[機械可読の集計](Validation/Summary.json)

[GCC Debugの全ケース](Validation/debug-cases-framework.log) / [Nativeの全ケース](Validation/debug-cases-native.log)

[Debug CTest](Validation/debug-ctest.log) / [Release CTest](Validation/release-ctest.log) / [ClangサニタイザーCTest](Validation/sanitized-ctest.log)

[公開ヘッダー一覧](Validation/headers.log) / [外部組み込みの構成](Validation/consumer-configure.log) / [外部組み込みとヘッダー検査のビルド](Validation/consumer-build.log) / [外部実行](Validation/consumer-run.log)

対応するコンパイルログとコマンド・終了コードもValidationに含めています。Debug／Releaseの最初の全コンパイル出力は `debug-build-full.log`・`release-build-full.log` に保存しています。

## TDDと配布物

[実装順とRed／Greenの説明](Testing.md)を参照してください。Tddディレクトリに先行テストの失敗、修正後の結果、追加の回帰検査を区別して記録しています。

フレームワーク本体はC++ヘッダー・ソース **80ファイル**です。サンプル、テスト、CMake、規約設定、検証・生成スクリプト、文書を含めて配布します。実SDK、生成済みWindows実行ファイル、フォントファイルは配布しません。

[Windows側で残っている確認](WindowsValidation.md)を実行するまで、実DxLib動作を確認済みとは扱わないでください。
