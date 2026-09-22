# Scene / Task寿命統合 — 15f5df4向け

前回のTaskDispatcher復旧版の上に、Scene破棄前の退役をApplication / SceneNavigatorへ接続する更新です。
基準コミットは`15f5df42571dddee2a0dcaede16e0fa826a9061b`です。

## この作業ツリーでの統合状況（2026-09-23）

上記以降は配布時点の説明です。この作業ツリーでは、Physics Snapshot・Debug表示移行・Application／Renderer統合・Scope待機分離を維持したScene寿命統合・DXF_LOGを
同一ツリーへ統合し、Windows / MSVCのDebug / Releaseで累積回帰を実行しました（root CTest 20/20、Scene 20/20、Snapshot 36/36ほか）。
結果と未完了事項（Release実SDKリンクがSDK同梱のMT版DxLibのFBX依存で失敗）は`Validation/RendererSceneIntegration-2026-09-23.md`を参照してください。

## 変更

- SceneのOnEnter前にScopeを確定し、Activation / Tick ContextへTask窓口とScopeを渡します。
- 切替時は遷移先の同期初期化・新Scope確保の成功後に旧Scopeを退役します。
- 旧ScopeのPrepare完了と取消済み捕捉の解放が終わってからOnExit / OnDeinitialize / 破棄を行います。
- TaskのCommitや捕捉破棄、Navigatorの直接操作に再入する終了は、安全な所有スレッドの境界へ遅延します。
- 初期化に失敗した候補は後始末し、旧Sceneと旧Scopeは維持します。
- Root Scopeの仕事は通常のScene切替では取り消しません。

仕様と使用例は`Payload/Docs/SceneTaskLifetime.md`を参照してください。適用後はリポジトリの`Docs/SceneTaskLifetime.md`になります。

## 適用

ZIPの`dxf_scene_task_lifetime`フォルダーを**リポジトリの外**、例えば`Downloads`へ展開してください。
`Payload`や`Testing`の中身を手動で丸ごとコピーしないでください。

リポジトリのルートに移動したPowerShellで実行します。

```powershell
$Apply = "$HOME\Downloads\dxf_scene_task_lifetime\apply_scene_tasks.py"

python "$Apply" --root .
if ($LASTEXITCODE -ne 0) {
    throw "検査が停止しました。強制適用せず、表示された差分・変更対象を確認してください。"
}

python "$Apply" --root . --apply
if ($LASTEXITCODE -ne 0) {
    throw "適用が停止しました。表示された理由と退避先を確認してください。"
}

git diff --check
if ($LASTEXITCODE -ne 0) {
    throw "差分の形式検査に失敗しました。"
}
```

既存6ファイルを更新し、新規6ファイルを追加します。通常は12ファイルです。すでに同じ更新があるファイルには触れません。
変更対象と重なる未コミット・stage済み変更、別内容の新規パス、必要な非同期基盤の差異を検出すると停止します。
変更前ファイルはリポジトリの隣の`<repo名>.scene-task-backup-*`へ退避します。reset / stash / commit / pushを行いません。
UTF-8のBOMとCRLFの有無は比較時に正規化し、更新時には既存の形式を保ちます。Shift-JISの入力は受け付けません。
実行中に対象を別のエディタやプロセスで変更しないでください。電源断を含む一括原子性の保証はありません。

前回配布したApplication / Renderer統合器で生成した版も、対応するファイルハッシュが一致すれば認識します。
その場合は`FRenderSystem`と`SetExecutionJobs`の接続を保った更新を選びます。
任意の手編集へ自動マージするものではなく、ヘッダー・cppが混在した状態も拒否します。
**root CMake、Renderer実装、FVector2、アセットRoot、.dxfpaths、Assets原本、Physicsの数値処理は更新対象外です。**

## 適用後の検証

Visual Studio用の新規ビルドディレクトリから、専用テストを生成します。
既定値は本体の`dxf::framework`へリンクする構成です。元のslnを変更・再利用する必要はありません。

```powershell
cmake -S .\Tools\SceneTaskValidation -B .\Build\scene-task-integration -A x64
if ($LASTEXITCODE -ne 0) { throw "Scene Taskの生成に失敗しました。" }

cmake --build .\Build\scene-task-integration --config Debug --target dxf_scene_task_integration_tests
if ($LASTEXITCODE -ne 0) { throw "Debugビルドに失敗しました。" }
ctest --test-dir .\Build\scene-task-integration -C Debug -L scene-task --output-on-failure --no-tests=error
if ($LASTEXITCODE -ne 0) { throw "Debugテストに失敗しました。" }

cmake --build .\Build\scene-task-integration --config Release --target dxf_scene_task_integration_tests
if ($LASTEXITCODE -ne 0) { throw "Releaseビルドに失敗しました。" }
ctest --test-dir .\Build\scene-task-integration -C Release -L scene-task --output-on-failure --no-tests=error
if ($LASTEXITCODE -ne 0) { throw "Releaseテストに失敗しました。" }
```

Ninjaなど別Generatorを使用している場合は`-A x64`を付けず、その環境に合わせたGeneratorを指定してください。
この専用構成とは別に、通常の全体ビルド・既存CTest・実DxLib起動も確認してください。公開Contextとコンストラクタの変更があるため、古いオブジェクトファイルの使い回しは避けてください。

Linuxの部分検証を再現する場合は、明示的に`-DDXF_SCENE_FOCUSED=ON`を指定します。
対象Rootに実装依存がそろっていることが必要です。この配布物はフレームワーク全ソースのZIPではありません。

## 今回確認した範囲

実Application / SceneNavigator / Dispatcher / Job / Asset / Audio / Rendererの依存部分をビルドしました。
外部Backendだけをテスト実装へ置き換えた回帰は18件です。
GCC Debug・Release、Clang ASan/UBSan、GCC ThreadSanitizerで通過し、Releaseは各50回・計900回も通過しました。
前回のRenderer統合版を再現した構成でも18件が通過しています。これは3D機能そのものの新規検証ではありません。
適用器の18件のテストも通過しています。

**Windows / MSVC、実DxLib SDK、完全なリポジトリ全体・既存テスト一式の成功報告ではありません。**
詳細な終了コード・原ログは`STATUS.json`と`Validation`、検証の境界は`AUDIT.md`にあります。
既存依存には警告が残っています。新規変更したRuntime実装とテストは警告エラー扱いで確認していますが、依存全体の警告ゼロとはしていません。

## 制限

`RetireScope`は従来どおりDispatcher全体のPrepareを待ちます。無関係なTaskが終わらない場合も待機が続きます。
今回新たに非同期初期化を導入していません。Scene用TaskはOnEnter以降、終了フックでは投入しない契約です。
Sceneより先に消える個々のGameObject / Componentの寿命は、この変更だけでは保護しません。

`Testing/installer_fixture.zip`と`Testing/renderer_before.zip`は適用器の使い捨て試験用です。ユーザーのリポジトリへコピーされません。
`scene_task_lifetime.patch`は基準のRenderer未統合版に対するレビュー用差分です。通常の適用には必ず適用器を使用してください。
