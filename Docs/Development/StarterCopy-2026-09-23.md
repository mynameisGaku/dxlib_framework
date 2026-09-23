# Starter作業用コピーへの移植確認（2026-09-23）

## 開始状態と結論

開始時のmainは`1ec349fc24e14eae5946077b4f13b9a68c000e4d`。fetch後のorigin/mainも一致し、未コミット・未追跡変更はありませんでした。最新mainからファイルコピーし、別ブランチ・worktree・過去ZIP・Archive適用器は使っていません。

Docs/SmallGame.mdの4ファイルコピー、includeと初期Sceneの変更、Starterへの2つのcpp登録で、生成・Debug／Releaseビルド・通常起動まで成功しました。ゲームやフレームワーク本体への修正は不要でした。元のStarter／Source／Assets／Sandboxは変更していません。

元の文書は生成コマンドと既存SDK再利用の指定が省略されていたため、作業用コピーの範囲、正確なCMake登録、SDK指定、両構成の実行、.dxfpathsを残す理由を追記しました。移植時のコンパイル不具合を修正した記録ではありません。

## 実際の対象

- 最初の手順確認：`C:/dev/dxf-starter-copy-initial-27_u_lzb`。通常入口で両構成をビルド・起動・正常終了。その後Debug固定入力の試験を実行。
- 最終再実行：`C:/dev/dxf-starter-copy-qfd7ip06`。最終版の検証器を使い、独立した新規コピーで全工程を再実行。
- 生成先：上記コピーの`Build/Starter`（Visual Studio x64、マルチ構成）。以前のBuildはコピーしていません。
- 通常実行ファイル：`Build/Starter/Debug/Starter.exe`、`Build/Starter/Release/Starter.exe`。
- 固定入力版：同じ構成ディレクトリの`Starter-probe.exe`。通常Starterのビルドと起動を先に確認した後、コピー側の入口だけを計測用に変更してビルドしました。完了後は通常の入口・CMake登録へ戻し、両構成を再ビルドしています。
- 使用SDK：`C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework/ThirdParty/DxLib-3.25a-source`。同じPCの既存ビルドを`DXF_DXLIB_CUSTOM_ROOT`で明示参照。SDK自体を新環境で構築した検証ではありません。

Starter.vcxprojのClCompileを照合し、通常版の登録がコピー先の以下4ファイルだけであることを確認しました。

```text
Examples/Starter/Source/WindowsMain.cpp
Examples/Starter/Source/BootScene.cpp
Examples/Starter/Source/SandboxMenuScene.cpp
Examples/Starter/Source/SandboxGame.cpp
```

固定入力版ではこれにコピー側のStarterCopyProbe.cppだけを追加しています。Sandbox側のcppや元リポジトリのオブジェクトをStarterへリンクしていません。ゲーム4ファイルのハッシュは移植元と一致し、Privateヘッダーや内部実行入口への依存は追加していません。フレームワークライブラリもコピー側のSourceからビルドしています。

## 実行結果

| 対象 | Debug | Release |
|---|---|---|
| コピー先CMake生成 | 共通生成1回、終了コード0 | 同じマルチ構成生成 |
| 文書どおりのStarterビルド・通常起動 | ビルド0、起動後の通常ウィンドウ終了0 | ビルド0、起動後の通常ウィンドウ終了0 |
| 固定入力版Starter | ビルド0、2回の実行とも0 | ビルド0、2回の実行とも0 |
| 通常入口へ復元後のStarter再ビルド | 0 | 0 |
| 元リポジトリ再生成・ビルド | 0 | 0 |
| 元リポジトリCTest | 登録24／実行24／成功24、11.68秒、0 | 登録24／実行24／成功24、9.42秒、0 |

コピー先ではゲームのcppを実Starterターゲットから実行しています。コピー先のCTestは無効にしており、0件のCTest成功を検証の代用にはしていません。元リポジトリは実デバイス試験を明示有効化し、`ctest --no-tests=error`で実行しました。登録数は前回と同じ24群です。元の空Starter、Sandbox、モデル・描画の試験は削除・変更していません。

固定入力ではIInputSourceからキーを送り、本物のInputSystemがSnapshotへ変換します。実Application／実Scene／実DxLibで次を確認しました。

- コピー側のplayer.bmpだけを保存・一時退避して、タイトル→開始要求が実ロード失敗すること。元のタイトルを保持し、公開GetLastTransitionErrorに理由が入り、理由の文字領域に画素があること。
- 同じApplicationで画像を戻してEnterを再入力し、プレイへ進みエラーが解消すること。元Assetsは触らず、コピーの画像も最後に保存値とバイト一致で復元。
- プレイ→ゴール→結果→リトライを通し、プレイヤー開始位置が320へ戻り、旧ハンドルが失効すること。
- 2周目だけポーズを2秒長く保持し、その間に移動入力を送っても位置が不変であること。両周の結果表示領域に文字画素があり、領域ハッシュが完全一致すること。保存画像の結果は3500 ms。
- 既存のゴール帯RGB（40,160,80）とメニュー背景の判定を保持。文字領域は一括GPU読戻し後にCPU上で確認し、画素ごとのGPU転送は行っていません。
- 結果からタイトルへ戻ると古い結果とエラーの文字が残らないこと。Escによる正常終了、その後の新プロセスでも同じ試験が成功すること。
- 各構成をコピーのルートと`unrelated-cwd`から実行。変更していないResolveEntryProjectRoot("Starter.dxfpaths")がコピーのルートを選び、ゲームはAssets/...のまま動くこと。

タイトル・プレイ・結果・リトライ・失敗理由のPNGを保存しました。Debug／Releaseの保存画像で表示を確認しています。音声は実APIによる読み込み・再生要求を通していますが、聴感確認ではありません。

## 保存先と再実行

再実行入口は`Tools/ValidateStarterCopy.py`、コピーにだけ組み込む固定入力の検証器は`Tools/StarterCopyProbe.h/.cpp`です。恒久的なサンプルや通常ソリューションのプロジェクトは追加していません。

```bat
python Tools/ValidateStarterCopy.py --work-parent C:/dev --sdk-root C:/Users/g0190/OneDrive/Desktop/frame/dxlib_framework/ThirdParty/DxLib-3.25a-source
```

親ディレクトリは既存かつリポジトリ外でなければ拒否します。毎回新しいコピーを作り、各コマンドは失敗時に停止します。最終実行の13工程はすべて終了コード0です。検証後に残る通常Starter.exeは普通に起動できます。固定入力版を直接再実行するには画像欠落などの準備が必要なため、反復時はスクリプトから行ってください。

最終コピーの`validation/Summary.json`にソースの保存元・SDK・実行ファイルSHA-256、`source-manifest.json`にコピー元ファイル別ハッシュ、工程別logにコマンド・終了コードを保存しました。`Starter-documented.vcxproj`と入口の通常／計測版も保存しています。画像は`validation/images-{Debug,Release}-{0,1}`です。検証終了時に元ソースのハッシュ不変を照合しました。本記録だけは検証後に追加した結果の追記です。

元リポジトリの結果は`Build/FbxContinuation/starter-original-*-{build,tests,registration}.log`、`starter-original-configure.log`、`starter-copy-final.log`へ保存しています。

追加の確認：No-STLは290ファイル・違反0、Pythonは19件成功、配布検証7段階はすべて0。配布検証はNative無効・Debugの再配置／外部利用であり、コピー先StarterのNative有効・Debug／Release試験とは別です。既存clang-format、UTF-8／CRLF、git diff --checkも確認しました。

## 未実施と完了範囲

物理キー操作、音声の聴感、制作時間の利用者評価、SDK未導入PC、開発ツール未導入PC、実Direct3D9環境、D3D全資源のリーク列挙は未実施です。通常起動時のウィンドウ終了はOSの終了要求を自動送信したもので、物理キー操作の試験ではありません。

今回の限定作業は完了です。公開API、管理クラス、モデル・描画機能への変更はありません。過去のSmallGame-2026-09-23.mdの未実施記録は当時の結果として保持しました。次の描画機能の実装には着手していません。
