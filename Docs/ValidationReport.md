# 検証結果 — dxlib_framework 0.3.0

以下は受領した0.3.0 ZIPに収録された検証記録です。この作業環境で実施したMSVC検証と追加修正は[Windows統合検証](WindowsIntegration.md)へ分けて記録しています。

記録日: **2026年9月12日**。0.2.0のソース・48コミットの履歴を復元して継続開発し、今回の修正後のコードで再実行した結果です。

## 実測した結果

| 検証 | 結果 |
|---|---|
| 基盤・ライフサイクル・Application・Sandbox・追加回帰 | **138 / 138 通過** |
| 代替DxLibヘッダーによる接続契約 | **14 / 14 通過** |
| GCC Debug | 全152ケース通過 |
| GCC Release | 全152ケース通過 |
| Clang Debug + AddressSanitizer + UndefinedBehaviorSanitizer | 全152ケース通過、実行経路で指摘なし |
| ASanリーク検査 | detect_leaks=1、実行経路で報告なし |
| 警告をエラーにする設定 | 上記3構成で有効、ビルド成功 |
| 公開ヘッダーの単独コンパイル | **65 / 65 通過** |
| Sandboxヘッダーの単独コンパイル | **1 / 1 通過** |
| 外部CMakeプロジェクトへのadd_subdirectory | コンパイル・リンク・実行成功 |
| install → 空白を含む別ディレクトリへ移動 → find_package | コンパイル・リンク・実行成功 |
| インストールしたSupport層だけの利用 | Runtime／Gameplayなしでリンク・実行成功 |
| Pythonの配布・検証ツール | **9 / 9 通過** |
| 配布候補ZIPの展開・新規ビルド | 全274ファイルのSHA-256照合、C++152件・Python9件通過 |
| 実DxLib SDK / Windows / MSVC | **未検証** |
| WindowsMain.cpp / 実画面・音声・入力機器 / GitHub Actions | **未実行・未検証** |

C++152件は、2つのCTest実行ファイル（FrameworkとNativeContract）に収録した内部ケースの合計です。CTestの表示は2 / 2であり、152個のCTestターゲットではありません。Python9件は別集計です。

NativeContractは実装した接続コードを手書きの代替ヘッダーとつないで確認したものです。**実SDKのヘッダー・ABI・自動リンク・MSVC・デバイス検証の代わりにはなりません。**

## 今回のRed／Green

新規25件のC++回帰テストを先行して追加し、描画8件、Scene／終了11件、資源／音声6件の問題を再現してから修正しました。音声のRedでは、最後のテストが実際にクラッシュし、プロセスの終了コードは139でした。最終件数が出なかったRedを「全件実行済み」としては数えていません。

修正後は上記の全構成で最後まで実行しています。Pythonは先行する3件の失敗テストを追加し、古い成功Summaryとタイムアウト時の診断欠落を修正しました。

Nativeコールバックが失敗しても復元できればPresentしてよい、という旧テストの期待値は今回変更しています。[移行時の注意](Migration_0.3.md)に記載した意図的な動作変更です。

[実装順とRed／Greenの詳細](Testing.md) ／ [描画のRed](Tdd/Continuation/01-red.log) ／ [SceneのRed](Tdd/Continuation/02-red.log) ／ [音声クラッシュを含むRed](Tdd/Continuation/03-red.log) ／ [修正後の138ケース](Tdd/Continuation/03-green.log)

## 反復検査

前版からの、世代付きハンドルの生成・削除・再利用20,000操作、資源の読み込み・解放・キャッシュ掃除1,000周、Unicodeコードポイント値1,114,112通りの検査も今回の実行に含みます。サロゲート2,048値を拒否し、有効なスカラー値を受理する検査であり、すべてのバイト列を試すものではありません。

## ログと再現

[全構成の機械可読Summary](Validation/Summary.json) ／ [ツールチェーン](Validation/toolchain.log)

[Debugの基盤ケース](Validation/debug-cases-framework.log) ／ [接続契約ケース](Validation/debug-cases-native.log) ／ [Releaseの基盤ケース](Validation/release-cases-framework.log) ／ [サニタイザーの基盤ケース](Validation/sanitized-cases-framework.log)

[Debug CTest](Validation/debug-ctest.log) ／ [Release CTest](Validation/release-ctest.log) ／ [サニタイザーCTest](Validation/sanitized-ctest.log)

[公開ヘッダー](Validation/headers.log) ／ [外部利用](Validation/consumer-build.log) ／ [移動後のパッケージ検証](Validation/Package/Summary.json) ／ [Python9件](Validation/python-tools.log)

```sh
python Tools/Validate.py --with-sanitizers --jobs 4
python Tools/ValidatePackage.py --jobs 4
python -m unittest discover -s Tools/Tests -v
```

最新のSummaryはstatusと実行日時を確認してください。失敗時に古い成功結果を残さないよう、実行開始時にrunningへ更新します。同じログ出力先へ並行実行しないでください。

## 配布物

配布候補ZIPを新しいディレクトリに展開し、274ファイルのSHA-256とサイズを照合しました。そこからビルドし直し、C++152件とPython9件が通過しました。[ReleaseArchive/Summary.json](Validation/ReleaseArchive/Summary.json)に結果、[CTestログ](Validation/ReleaseArchive/ctest.log)に実行出力を保存しています。

この検査後に最終ログ・文書を追加するため、最終ZIP全体のハッシュは候補と異なります。Source・Tests・Examples・Assets・CMake・Tools・ビルド設定は検査済み候補と同一であることを配布時に照合します。候補ZIPを最終ZIPだと取り違えないよう、候補のハッシュもSummaryに記録しています。

## 実機で残る確認

作成環境はLinuxです。この作業でも公式SDKの取得を試しましたが、コンテナの名前解決が失敗し、SDK取得・実SDKコンパイルは行えていません。[取得試行の実測ログ](Validation/sdk-availability.log)に失敗を保存しています。これはDxLib配布サイト自体が停止しているという意味ではありません。

Windowsでの入口はBuild.cmd／Validate.cmdで、結果はBuild/WindowsValidationへ出力します。[WindowsValidation.md](WindowsValidation.md)を参照してください。手順の同梱、代替ヘッダーによるテスト、Linuxサニタイザーの成功を、実機動作の確認済みとして読み替えないでください。

本検査は実行した経路を対象とし、すべての条件での正しさ・性能・不具合不存在や、カバレッジ100%を保証するものではありません。
