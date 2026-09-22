# TaskDispatcher回復更新 — 4868dc9

前回のTaskDispatcher更新を**未適用**として作り直したパッケージです。
古いTaskDispatcher ZIPや期限切れリンクは不要です。以前のZIPと同一バイトの再配布ではありません。

対象は `mynameisGaku/dxlib_framework` の次のソースです。

```text
4868dc9811d60e227aa0d7c4d2c18410388d3d63
```

適用器はHEADの文字列だけで判断せず、変更対象とテスト依存ファイルの内容を確認します。
Application/Renderer統合を適用済みでも、それらの変更には触れません。
対象TaskDispatcher自体や依存JobSystem等に別の修正がある場合は停止します。強制適用はありません。

## 今回入るもの

TaskDispatcher.h / TaskDispatcher.cppの2ファイルを置き換えます。
専用回帰テスト、専用CMakeプロジェクトと実行入口、API契約の文書を追加します。合計6ファイルです。
ルートCMake、Application、SceneNavigator、Renderer、FVector2、Physics、Assets、アセットRoot、
`.dxfpaths`、既存の修復用スクリプトは変更しません。

主な修正は、協調取消で止まる先頭Task、停止後のScope生成と投入、確保失敗時の登録残り、
捕捉破棄中のMutex再入、投入と停止の間のFence登録前の隙間、準備の捕捉破棄前にCommitへ進む問題です。
`RetireScope()`を追加し、親子Scopeの世代も検証します。

**Sceneを破棄する前にRetireScopeを呼ぶApplication/SceneNavigator側の接続は、この更新には含みません。**
したがって、ApplicationのScene切替全体を修復・検証した更新ではありません。

## 適用

Python 3.10以降、Gitが必要です。ビルドにはCMake 3.24以降とC++20対応のC++環境が必要です。
ZIP内の`dxf_task_recovery`フォルダーを、リポジトリの外、例えばDownloadsへ展開してください。
**PayloadやTestingを手動でリポジトリへ上書きコピーしないでください。**
Testingの内部ZIPは、適用器テスト専用の旧版ファイルです。

Windows PowerShellでリポジトリのルートへ移動し、次を実行します。

```powershell
$Apply = "$HOME\Downloads\dxf_task_recovery\apply_task_dispatcher.py"

# 検査のみ。成功しても、この段階では変更しない。
python "$Apply" --root .
if ($LASTEXITCODE -ne 0) { throw "検査が停止しました。強制適用しないでください。" }

# 検査した更新を適用する。
python "$Apply" --root . --apply
if ($LASTEXITCODE -ne 0) { throw "適用が停止しました。表示された理由と退避先を確認してください。" }

git diff --check
if ($LASTEXITCODE -ne 0) { throw "差分の検査に失敗しました。" }
```

初回はREPLACE 2件、ADD 4件が表示されます。同じ結果が既に入っているファイルは変更しません。
変更するファイルと重なるstage済み・未コミット変更は拒否し、無関係なApplication等の変更は保持します。
CRLFは比較時に正規化し、置き換えるファイルの改行とBOMは保持します。
作業中は対象ファイルの編集や自動整形を止めてください。別プロセスとの排他的ファイルロックは取りません。

変更前の内容はリポジトリ隣の`<repo名>.task-backup-*`へ退避します。
書き込み失敗時は完了済みの書き込みを戻します。別プロセスの変更で差し戻せない場合は、そのファイルを
上書きせず退避先を表示します。電源断・強制終了までを一括トランザクションとして保証するものではありません。
Gitのreset・stash・commit・pushやstage操作は行いません。

## 専用テスト

実TaskDispatcher / JobSystem / Threadingを使う専用プロジェクトです。
Application/Rendererの接続状態に依存せず実行できます。既存ビルドディレクトリは使いません。

```powershell
foreach ($Config in @("Debug", "Release")) {
    $Build = ".\Build\TaskDispatcherRecovery-$Config"

    cmake -S .\Tools\TaskValidation -B "$Build" "-DCMAKE_BUILD_TYPE=$Config"
    if ($LASTEXITCODE -ne 0) { throw "$Config の生成に失敗しました。" }

    cmake --build "$Build" --config $Config
    if ($LASTEXITCODE -ne 0) { throw "$Config のビルドに失敗しました。" }

    ctest --test-dir "$Build" -C $Config --output-on-failure --no-tests=error
    if ($LASTEXITCODE -ne 0) { throw "$Config のテストに失敗しました。" }
}
```

正常時は各構成で26件です。この専用ターゲットはroot CMakeの既存CTestには自動登録していません。
フレームワーク全体の通常ビルドと既存テストも別途必要です。

適用器だけを検証するには、展開フォルダーで`python test_applier.py`を実行します。
テストは一時ディレクトリ内の使い捨てGitリポジトリで動作し、実プロジェクトは操作しません。

## 検証結果

| 検証 | この環境で確認した結果 |
|---|---|
| GCC Debug | 26 / 26通過 |
| GCC Release | 26 / 26通過 |
| Clang ASan + UBSan | 26 / 26通過 |
| GCC ThreadSanitizer | 26 / 26通過 |
| Release反復 | 各26件を50回、計1,300回通過 |
| Python適用器 | 15 / 15通過 |
| ZIP展開・適用後のRelease | 26 / 26通過。再適用も変更なし |
| No-STL | 部分ソース20ファイル、違反0 |

これらはLinuxでの専用テスト結果です。ログは`Validation/`、各実行の終了コードは`STATUS.json`にあります。
**Windows/MSVC、実DxLib SDK、全体ビルド、既存Framework/Native/Physicsテスト、実Application起動は未検証です。**
Sanitizerの成功は、このテスト範囲で報告がなかったという意味であり、すべての利用方法の無競合を証明しません。

## RetireScopeの注意

`Cancel()` / `DestroyScope()`は実行中Prepareを待ちません。
`RetireScope()`は所有スレッドの安全な境界から呼び、trueを確認してから対象の破棄へ進みます。

**この実装はDispatcher全体のPrepareを待ちます。対象外Scopeの長時間処理も待機対象です。**
終わらない処理を強制終了する機能、Scope専用Fence、待機のタイムアウトはありません。
Job・Commit・捕捉のデストラクタから呼ぶことはできません。
詳細は適用後の`Docs/TaskDispatcherRecovery.md`を参照してください。

## 内容物

`Payload/`は適用する6ファイルだけです。`MANIFEST.json`は対象・依存ファイルの照合情報、
`task_dispatcher_recovery.patch`はレビュー用差分です。適用は上の検査付きスクリプトを使ってください。
`Testing/installer_fixture.zip`は適用器テスト用で、実プロジェクトへ展開するものではありません。
`SHA256.json`はパッケージ内ファイルの破損確認用です。電子署名ではありません。
