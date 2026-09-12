# Toolbox追加時の検証

2026-09-12、Windows x64 / Visual Studio 18 Community / MSVC 19.51で実施しました。

| 検証 | 結果 |
|---|---|
| 開発用Debugビルド | Frameworkテスト、Native契約テスト、Sandbox、NativeSmokeをビルド成功 |
| Debug / Releaseテスト | 各構成でFramework 175件、Native契約15件成功。STLチェックを含むCTest 3/3成功 |
| AddressSanitizer | 別のportable Debug構成でFramework・Native契約テストをビルドし、CTest 3/3成功 |
| Pythonツールテスト | 17件成功 |
| STL混入チェック | C++ 134ファイル、違反0 |
| 公開ヘッダー | インストールした84ヘッダーを個別に `/W4 /WX` でコンパイル成功 |
| 配布パッケージ | portable Debugをインストール・別パスへ移動し、外部consumerから6公開ターゲットを検出、リンク・実行成功 |
| 通常ソリューション | 7プロジェクト、21参照が解決。公開84ヘッダーを登録 |
| 開発ソリューション | 15プロジェクト。通常版とは別のBuildディレクトリを参照 |

所有権・再確保・例外安全性のほか、重複型Variant、コピー専用Vector要素、非代入型Optional、voidコールバックの回帰ケースを含みます。数学と衝突のテストでは、極端な大きさの数値、行列逆変換、Quaternion、複素数配列、各形状、木による候補絞り込みを確認しました。

Releaseでは意図的に例外を投げるテスト用コンストラクターにC4702警告が出ます。Releaseの実SDKアプリケーション実行、Linux、SIMD非対応環境、性能ベンチマークは今回の検証対象に含みません。Native契約テストはFakeDxLibを使った契約確認で、実SDK実行の代用ではありません。

再実行には`GenerateProjectFiles.bat -Development -NoPause`で開発用ソリューションを生成し、テストターゲットをビルド後、`ctest --test-dir Build/VisualStudio-development -C Debug --output-on-failure`を使用します。STLチェックは`python Tools/CheckNoStl.py`、ツールテストは`python -m unittest discover -s Tools/Tests`で実行できます。
