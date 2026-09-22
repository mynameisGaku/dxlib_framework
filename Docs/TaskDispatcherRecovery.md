# TaskDispatcherの取り消し・退役契約

`4868dc9`の`Dxf::FTaskDispatcher`に対する回復更新です。以前のTaskDispatcher更新ZIPは不要です。
Application/Rendererの統合変更も、この更新の適用条件にはしていません。

## 所有と実行スレッド

Dispatcherは構築スレッドが所有します。JobSystemはDispatcherより長く生存させます。
Dispatcherの破棄を始める前に、外部から呼び出すProducerを停止・Joinしてください。
`Submit`、`CreateScope`、`Cancel`、`DestroyScope`、生存/取消照会はMutexで同期します。
`PumpCommits`は所有スレッド限定です。Job、Commit、捕捉データ破棄からの再入は例外で拒否します。

PrepareはCPU準備だけを行い、SceneやDxLibの状態へ直接書き込まないでください。
Commitは所有スレッドで投入順に実行されます。途中のPrepare失敗・協調取消は後続を止めません。
未完了の先頭は引き続き後続Commitを待たせます。実行中Taskを取り消しても、準備終了前には
保留件数の枠や捕捉を解放しません。関数の捕捉が外部にも共有されていれば、その外部所有分は残ります。

## Cancel / DestroyScope / RetireScope

`Cancel(Scope)`はScopeと子孫への新しい投入・未開始Commitを取り消します。
`DestroyScope(Scope)`はさらにScopeと子孫を失効させます。
この二つは実行中のPrepareを待ちません。**これだけでSceneを破棄してよいという意味ではありません。**

`RetireScope(Scope)`は、Scopeと子孫を失効させてから投入途中の処理と準備終了を待ち、
Dispatcherが保持する取り消し済みTaskと捕捉を回収します。Commitは実行しません。
所有スレッドのJob/Commit/捕捉破棄外で呼び、成功したことを確認してください。
Root、別Dispatcher、不正なScope、禁止された実行区間ではfalseです。falseなら対象を破棄しないでください。
同じDispatcherの旧世代のScopeは、新世代を取り消さずに同期できます。

**現実装の待機単位はScope専用ではなく、Dispatcher全体のPrepareです。**
無関係な長時間Prepareも待ちます。終わらないPrepareや、待機中の所有スレッドの処理を必要とする
Prepareがあると戻りません。強制終了やタイムアウト退役は実装していません。
全体待機中でも無関係なScopeへの投入を禁止するAPIではないため、継続的なProducerは待機を長引かせます。
回収時には他Scopeの取り消し済み・準備終了済みTaskも解放します。

`WaitForPrepares()`は既存のvoid/noexcept APIを維持しています。誤った実行区間では待機せず戻ります。
これを退役完了の成功判定に使わず、対象の破棄には`RetireScope()`の戻り値を使ってください。

## Shutdown

所有スレッドの安全な境界では、受付停止、投入完了待ち、Prepare完了待ち、捕捉回収の順に終了します。
Job/Commit/捕捉破棄や別スレッドから呼んだ場合は停止要求だけを行います。
その場合は所有スレッドの安全な境界で再度呼び出してください。
Commit中の停止要求は、外側の`PumpCommits()`から戻る前に処理を進めます。
実行中のCommit自体を途中で中断する機能ではありません。
Dispatcher自身をそのコールバックや捕捉のデストラクタからdeleteしてはいけません。

## 確保失敗と再入

投入に伴う確保失敗は登録を撤回し、例外を呼び出し元へ伝播します。
準備や反映の捕捉をMutex保持中に破棄しないよう、まず空のTask記録を確保してから所有権を移します。
Prepareの捕捉破棄が終わる前にはReadyを公開しません。
捕捉の破棄から照会・取消・新しい投入は可能ですが、循環する待機は拒否します。

## この更新に含まない接続

**SceneNavigator/ApplicationがScene破棄前にRetireScopeを呼ぶ接続は未実装です。**
ApplicationのScene切替が安全になったという更新ではありません。
Scene準備失敗時の旧Scene維持、OnExit/OnDeinitializeより前の退役順序、
終了要求・直接Navigator操作も含む実Application経由の回帰は別途必要です。

## 専用回帰テスト

`Tools/TaskValidation`は実TaskDispatcher、実JobSystem、実Threadingを直接ビルドする専用CMakeプロジェクトです。
確保失敗だけをテスト専用の既存AllocationFaultで注入します。本体の挙動を置き換えるダブルではありません。
ただしApplication、Renderer、Physics、Nativeや`dxf::framework`全体のリンク・起動は検証しません。
通常のroot CMakeには新しいテストを自動登録していません。
