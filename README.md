# e753045 継続修正パッケージ

対象リポジトリ: mynameisGaku/dxlib_framework

対象commit: `e753045c76affa2cc69e890cb70f13a9ef430172`（main）

**本体6ファイルの修正、描画の公開入口、追加テスト、CMake登録を含む13ファイル分のパッチです。指令書や失敗状態だけの保存ではありません。GitHubへは反映していません。**

## 今回の範囲

- RenderContextから並列生成を呼ぶ入口を追加。
- 描画生成中の再入、不正Thread・同期JobからのNative呼出し、失敗時の部分追加を防止。
- Job投入途中の例外、Worker部分構築の例外、捕捉破棄中のFence待機を修正。
- 実OS Workerの所属と入れ子Jobの所属を分離。
- 現在のPrivate/Dxf・Private/Toolbox配置と、sln基準アセットRootを維持。

## 適用

ZIPはリポジトリの外へ展開してください。`ChangedFiles`を本体へ部分コピーしないでください。

未コミット作業を保存したうえで、リポジトリのルートで実行します。以下のPatch値は実際の展開先へ変更します。

```powershell
$Patch = "$HOME\Downloads\dxf_e753045_render_jobs_fix\e753045_render_jobs.patch"

git status --short
git branch --show-current
git rev-parse HEAD

git apply --check "$Patch"
if ($LASTEXITCODE -ne 0) { throw 'Patch check failed. Do not partially copy or force apply.' }

git apply "$Patch"
if ($LASTEXITCODE -ne 0) { throw 'Patch application failed.' }

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'Diff check failed.' }
git diff --stat
```

`git apply`は作業ツリーへの適用だけであり、commit/pushしません。別CLで変更されていたら適用を強制せず、その差分を確認してください。root CMakeは2行の登録追加のみで、丸ごとの置換ファイルは同梱していません。Patchには新規ファイルも含まれ、CMakeのテスト登録はDXF_BUILD_TESTS=ONの場合にだけ有効になります。

## Windowsの本体検証

既存のVisual Studio環境初期化とSDK設定を使い、通常の全体検証を実行します。

```powershell
.\Build.cmd -Clean
if ($LASTEXITCODE -ne 0) { throw 'Full Windows build/test failed.' }
python Tools/CheckNoStl.py
if ($LASTEXITCODE -ne 0) { throw 'Source policy check failed.' }
```

Releaseも既存の検証スクリプトで実行してください。NinjaではMSVC環境を取り込んだ同じプロセスで実行し、古いヘッダー依存情報が残っている場合は最初に既存のClean経路を使用してください。生成したVisual Studioプロジェクトを使う場合はGenerateProjectFiles.bat -Developmentでテストターゲットも再生成します。

## 追加分を独立検証する場合

```powershell
cmake -S Tools/RenderValidation -B Build/RenderContinuation -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'Configure failed.' }
cmake --build Build/RenderContinuation
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
ctest --test-dir Build/RenderContinuation --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
```

独立検証は本体のThreading / JobSystem / RenderQueue / RenderSystemのcppを直接ビルドします。別実装で代用はしていません。ただしApplication / Asset / Physicsの全体テストではありません。

## 実行済み

Linux上の正確な対象ソース・依存ヘッダーの部分チェックアウトで次を実行しました。

- 新規描画・追加回帰20/20。
- 無変更の現行Threadingテスト23/23。
- 故障注入5/5系統（うち構築系統は確保位置0〜13）。
- GCC Debug / Release、GCC ASan+UBSan、GCC TSan、Clang Releaseで7/7 CTest。
- 描画・ThreadingのRelease実行ファイルをそれぞれ100回反復し通過。
- 直接影響する4公開ヘッダーをGCC/Clangで単独コンパイル、8/8。
- Source/Testsの取得済み40ファイル＋検証用cpp2ファイルのSTL監査で違反0。
- パッチを元の正確な依存ソース部分へ再適用し、新規Releaseビルド・7/7 CTest。

パッチ再適用試験のroot CMakeは、GitHubで読んだ実際の挿入箇所を使った文脈fixtureです。フレームワークroot全体の再ビルドではありません。

## 未確認

- 実リポジトリ全体のビルドと既存全テスト。
- Windows/MSVC・実DxLib SDK・実機描画/音声。
- Physicsの長時間安定性と全Scope/Assetの寿命。

上記の未確認項目を今回の7/7へ含めてはいません。実SDK用の成功ログやGitHubのcommit/push結果はありません。

## 同梱物

- `e753045_render_jobs.patch`: 適用する一式。
- `ChangedFiles/`: レビュー用の修正後ファイル。部分コピーしない。
- `MANIFEST.json`: 基準blob SHA、修正後ハッシュ、対象パス。
- `Validation/`: この作業での実行ログ、Red、比較監査、Green。
- `ChangedFiles/Docs/Development/E753045RenderJobAudit.md`: API契約、監査と未確認範囲。

既存の古いStage ZIPやスクリプトを追加適用する必要はありません。
