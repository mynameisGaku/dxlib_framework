# 検証結果 — dxlib_framework 0.2.0

記録日: **2026年9月12日**。前回のソースとGit履歴を復元し、この版へ修正したコードで再実行した結果です。0.1.0の件数を転記したものではありません。

## 実行結果

| 検証 | 結果 |
|---|---|
| 基盤・ライフサイクル・Application・Sandbox・追加回帰 | **113 / 113 通過** |
| 代替DxLibヘッダーによる接続契約 | **14 / 14 通過** |
| GCC Debug | 全127ケース通過 |
| GCC Release | 全127ケース通過 |
| Clang Debug + AddressSanitizer + UndefinedBehaviorSanitizer | 全127ケース通過、実行経路で指摘なし |
| AddressSanitizerのリーク検査 | detect_leaks=1、実行経路で報告なし |
| 警告をエラーにする設定 | 上記3構成で有効、ビルド成功 |
| 公開ヘッダー単独コンパイル | **65 / 65 通過** |
| Sandboxヘッダー単独コンパイル | **1 / 1 通過** |
| 外部CMakeプロジェクトへのadd_subdirectory | コンパイル・リンク・実行成功 |
| install → 空白を含む別ディレクトリへ移動 → find_package | コンパイル・リンク・実行成功 |
| インストールしたSupport層のみの利用 | Runtime・Gameplayなしでリンク・実行成功 |
| 配布スクリプト | **6 / 6 テスト通過** |
| 配布候補ZIPを展開し直したソース | 全ファイルのSHA-256照合・新規ビルド・全127ケース通過 |
| CIのYAML／PowerShellファイル | YAML読み込み・静的検査のみ。PowerShell実行ではない |
| 実DxLib SDK / Windows / MSVC | **未検証** |
| 実画面・音声・入力機器／GitHub Actions実行 | **未検証・未実行** |

CTestは2つの実行ファイルなので、集計表示は2 / 2です。127件は内部のC++テストケース数であり、127個のCTestターゲットではありません。Pythonの6件は別集計です。

NativeSmokeの共通検査コードも、代替ヘッダーを使ったケース内で動かしています。**これは実SDKでNativeSmoke.exeを実行した結果ではありません。**

## 反復・網羅的な入力検査

独立したテストケースとして、世代付きハンドルの生成・削除・再利用を固定シードで**20,000操作**、資源の読み込み・参照解放・キャッシュ掃除を**1,000周**行いました。

UTF-8検査ではU+0000～U+10FFFFの**1,114,112コードポイント値**を走査し、Unicodeスカラー値は受理、サロゲート範囲2,048値のUTF-8風エンコードは拒否されることを確認しました。これとは別に、過長・切り詰め・上限超過・埋め込みNULの回帰検査があります。全バイト列の全組み合わせを試したという意味ではありません。

## 検証環境とログ

[機械可読の結果](Validation/Summary.json) ／ [ツールチェーン](Validation/toolchain.log)

[Debug全ケース](Validation/debug-cases-framework.log) ／ [接続部全ケース](Validation/debug-cases-native.log)

[Debug CTest](Validation/debug-ctest.log) ／ [Release CTest](Validation/release-ctest.log) ／ [サニタイザーCTest](Validation/sanitized-ctest.log)

[ヘッダー一覧](Validation/headers.log) ／ [外部組み込み](Validation/consumer-build.log) ／ [外部実行](Validation/consumer-run.log)

[インストール・移動後の検査](Validation/Package/Summary.json) ／ [Support層のみの実行](Validation/Package/support-only-run.log)

[配布候補の展開・再ビルド](Validation/ReleaseArchive/Summary.json) ／ [そのCTest結果](Validation/ReleaseArchive/ctest.log)

[配布ツールの6テスト](Validation/python-tools.log) ／ [CI・スクリプトの静的検査](Validation/workflow-static.log)

各検証コマンドと終了コードをログへ保存しています。テストの実行成功は、未実行の条件・デバイス・すべての経路で不具合がないことの証明ではありません。カバレッジ100%とは主張していません。

## 残る確認の境界

作成環境はLinuxで、Windows SDK・MSVCの標準ライブラリ・実デバイスを持ちません。公式DxLib SDKのダウンロードもこの環境から成功せず、実SDKコンパイルを行っていません。接続先の宣言・自動リンク・実ランタイム互換性まで確認済みとは扱えません。

[WindowsValidation.md](WindowsValidation.md)に実行入口と合否の記録方式を用意しています。**手順を同梱したことと、実機確認を済ませたことは別です。**

追加開発のRed／Greenと回帰検査は[Testing.md](Testing.md)および `Tdd/Completion/` に記録しています。

配布候補の再ビルド後は検証文書とログのみ追加し、最終ZIPと検査済み候補ZIPのSource・Tests・Examples・CMake・Tools・Assetsおよびビルド設定が同一であることを配布時に照合しています。ZIP全体のハッシュはログ追加により異なります。
