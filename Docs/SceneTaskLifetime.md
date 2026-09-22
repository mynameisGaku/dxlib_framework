# SceneとTaskの寿命

## 今回の境界

Applicationが所有するTaskDispatcherをSceneNavigatorへ接続し、SceneのScopeはNavigatorが管理します。
`OnEnter`を呼ぶ前にScopeが確定し、`FSceneActivationContext`と`FTickContext`の
`Tasks` / `TaskScope`から利用できます。GameInstanceのTickは同じ窓口とRoot Scopeを受け取ります。

Sceneを切り替える順序は次のとおりです。

```text
遷移先の同期初期化
  → 新Scopeの確保（まだ利用者には公開しない）
  → 旧Scopeと子Scopeを退役（準備完了・取消済み捕捉の解放）
  → 旧SceneのOnExit
  → 子オブジェクトと旧SceneのOnDeinitialize
  → 旧Sceneの破棄
  → 新Scopeを現在Sceneの所属にする
  → 新SceneのOnEnter
```

遷移先の初期化・新Scopeの確保が失敗した場合、旧SceneのScopeは取り消しません。
旧Sceneの終了が始まった後に終了要求が出た場合は切替を中断し、初期化だけ済んだ候補も後始末します。
終了フックと旧Sceneのデストラクタの間は、`Application.GetSceneScope()`は空ではなく失効済み旧ハンドルを返します。
空ハンドルをRootへ解釈するTaskDispatcherの仕様により、終了中の再投入が全体Taskへ化けることを防ぎます。

## 利用例

以下はSceneの`OnTick(const FTickContext& Context)`など、結果を扱える区間の例です。
CPU準備ではSceneやDxLibへ書き込まず、Taskの捕捉内の結果だけを作ります。

```cpp
if (Context.Tasks == nullptr || !Context.TaskScope.IsValid())
{
    return;
}

Dxf::FTaskRequest Request;
Request.Scope = Context.TaskScope;
Request.Prepare = []()
{
    // CPUデータの準備を行う。
    return Dxf::ETaskPrepare::Success;
};
Request.Commit = []()
{
    // 所有スレッドで結果を反映する。
    return true;
};

const bool bAccepted = Context.Tasks->Submit(Toolbox::Move(Request));
if (!bAccepted)
{
    // 受付停止・取り消し・保留上限に応じて処理する。
    return;
}
```

`Submit`は確保失敗を例外で通知することがあります。`OnEnter`は既存のnoexcept契約のままなので、
その中で投入する場合は例外を捕捉して失敗を処理してください。設定したレーン数が1の場合、Prepareは投入中に同期実行されます。

## 再入と終了

TaskのCommitや捕捉解放の中で`Application.Shutdown()` / `SceneNavigator.Shutdown()`が呼ばれた場合、
その場ではSceneを破棄しません。要求を保存して安全な所有スレッドの境界へ遅延します。
ApplicationのStepはPump後にも終了を調べ、終了要求後に新しい描画フレームを開始しません。
Task内からの直接のScene切替反映や再帰Stepは失敗を返します。次の切替要求をキューへ積むことはできます。

Application外からNavigatorを直接呼んだ場合のScene通知・旧Scene破棄も同様に保護します。
手動Pumpで発生した終了要求は、次の`Step`または安全な位置での`Shutdown`で完了させます。
ApplicationとNavigatorは所有スレッドで操作・破棄する契約です。WorkerはTaskのCommit経由で要求を渡してください。
Dispatcherの`IsOwnerThread()`と`CanSynchronize()`はこの接続用の観測APIであり、WorkerからScene操作を許可するものではありません。

## 意図的に扱わないこと

- `OnInitialize`には新SceneのTask Scopeを提供しません。Scene用Taskは`OnEnter`以降に投入します。
  初期化中の`Application.GetSceneScope()`は遷移元のScopeです。初回起動時は空です。
- Root Scopeや無所属で投入したTaskはScene切替では退役しません。Sceneへ生ポインタで依存するTaskをRootへ置かないでください。
- Sceneより先に単体破棄されるGameObject/Componentの寿命までは延長しません。Commitでは世代付きハンドルの生存確認などが別途必要です。
- Scope待機分離（[TaskScopeIsolation.md](TaskScopeIsolation.md)）の適用後、`RetireScope`は対象Scopeとその子孫だけを待ちます。
  Root直下や兄弟Scopeの実行中Prepareは、Scene切替の完了を妨げません（`scene_task_switch_completes_while_root_and_sibling_prepares_are_blocked`）。
  `WaitForPrepares()`と`Shutdown()`は従来どおり全体を同期します。
  対象Scope内のPrepareには強制終了・タイムアウトがなく、それが終了しなければ退役も完了しません。
- `CanSynchronize`の観測後に行う外部の破棄を一般に保証するAPIではありません。対象退役には`RetireScope`の成功が必要です。
- Sceneのコンストラクタや同期初期化で、外部に保存したポインタから勝手に開始した非同期処理は管理できません。

## 検証入口

専用構成の既定値は、本体の`dxf::framework`へリンクします。root CMakeを上書きせずに利用できます。

```powershell
cmake -S .\Tools\SceneTaskValidation -B .\Build\scene-task-integration -A x64
if ($LASTEXITCODE -ne 0) { throw "Scene Taskの生成に失敗しました。" }
cmake --build .\Build\scene-task-integration --config Debug --target dxf_scene_task_integration_tests
if ($LASTEXITCODE -ne 0) { throw "Scene Taskのビルドに失敗しました。" }
ctest --test-dir .\Build\scene-task-integration -C Debug -L scene-task --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Scene Taskのテストに失敗しました。" }
```

Visual Studio用の新規ビルドディレクトリを想定しています。Ninja等を選んでいる環境では`-A x64`を付けず、そのGeneratorを明示してください。
Releaseも別途ビルド・CTestしてください。実DxLib SDKの動作と既存テスト一式は、この専用テストとは別に確認します。

`-DDXF_SCENE_FOCUSED=ON`はApplicationが実際に使う依存部分だけをビルドする検証用の明示オプションです。
Application/Scene/Dispatcher/Job/Asset/Audio/Rendererの本体を使い、外部Backendのみをテスト実装へ置き換えます。
配布時のLinux環境で実行したのはこちらです。Physics・Native・既存全テストを含む全体ビルド成功という意味ではありません。
2026-09-23にはWindows / MSVCで既定構成（実`dxf::framework`、Renderer統合・Scope待機分離済み）をDebug / Releaseで実行しました。
結果は`Validation/RendererSceneIntegration-2026-09-23.md`を参照してください。
新規変更したRuntime実装と新規テストには警告エラー扱いを適用し、変更対象外の依存の既存警告は表示したまま保持します。
