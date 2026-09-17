# スレッドとJob System

## 基本方針

フレームワーク全体を「どのスレッドからでも自由に変更できる」構造にはしません。所有構造の変更とDxLib Native呼び出しは、今後も担当スレッド／担当フェーズを明確にします。重いCPU計算だけをJobとして分離し、読み取り入力とWorkerごとの結果をBarrier後に確定する構成を基本にします。

`Toolbox::FJobSystem`はそのための低レベル基盤です。STLの`std::thread`、`std::mutex`、`std::future`等は使用せず、WindowsではWin32同期API、POSIX環境ではpthreadへToolbox Private層から接続します。

## ExecutionThreadCount

`FJobSystem(0)`はOSが報告する論理スレッド数を使用します。上限は64です。

`FJobSystem(1)`は完全な同期実行です。Worker Threadを生成せず、`TrySubmit()`したJobを呼び出しスレッドで完了させます。並列版との比較、デバッグ、決定性確認に使用します。

2以上では`ExecutionThreadCount - 1`本のWorker Threadを所有します。呼び出し側はFence待機中のWorkerから子Jobを処理できるため、親Jobから別Fenceの子Jobを投入して待つ構成をサポートします。

## Fence

`FJobFence`は、そのFenceを参照するJobと`Wait()`がすべて終わるまで生存させます。Job System本体より長く生存させる必要はありません。同じFenceへ投入したJobがすべて完了するまで`Wait()`できます。`IsComplete()`は非同期の状態観測用で、Fenceを破棄する直前の寿命同期には必ず`Wait()`を使用します。`FailureCount()`はFenceの寿命中で累積するため、独立した処理単位には別Fenceを使います。

現在実行中のJob自身を含むFenceを、そのJobから待つことは循環依存なので`Wait()`が`false`を返します。別Fenceの子JobはWorkerがQueueを手伝いながら待つため、Worker数が少ない場合でも単純な親子待ちで停止しません。

Job本体から外へ出た例外はWorker Threadを終了させず、FenceとJob Systemの失敗数へ記録します。`ParallelFor()`は担当Jobに失敗があった場合`false`を返します。

## Shutdown

`Shutdown()`は新規Jobの受付を停止し、それ以前に受理したJobを最後まで処理してWorkerをJoinします。Job投入との競合は、Jobが「受理されて必ず一度完了する」か「拒否されて実行されない」のどちらかになります。

複数の外部スレッドから同時に`Shutdown()`された場合も、全呼び出しがWorkerのJoin完了を待ってから戻ります。

Job System自身をWorker Jobの中から破棄したり、Workerから`Shutdown()`して自分自身をJoinする使用方法は対象外です。所有者はJob Systemの寿命をWorker Jobより長く保ち、Game/Main側の終了フェーズから停止してください。

## Thread契約

現段階の契約は次の通りです。

- `TAtomic<T>`の操作と`FMutex::Lock/TryLock/Unlock`: ThreadSafe。
- `FConditionVariable::NotifyOne/NotifyAll`: 複数スレッドから利用可能。`Wait()`は対象`FMutex`を保持して呼び、必ず条件をwhileで再確認する。破棄時には待機者を残さない。
- `FThread`: 単独所有。別スレッドから同じ`FThread`へ`Start/Join/Move/Destroy`を同時実行しない。静的なThread ID・CPU数・Yield取得は複数スレッドから利用可能。所有しているThread自身からその`FThread`を破棄・Joinしない。
- `FJobFence`: `IsComplete/PendingCount/FailureCount`は並行参照可能。Fenceを参照するJobまたはWaitが残る間は破棄しない。
- `FJobSystem::TrySubmit()`: 複数Producerから同時利用可能。
- `FJobSystem::Wait()`: 外部スレッドおよび同じJob Systemの実行中Jobから利用可能。ただし自己Fence待ちは拒否し、循環するFence依存は利用側で作らない。
- `FJobSystem::Shutdown()`: 複数の外部所有スレッドから同時呼び出し可能で、全呼び出しがJoin完了を待つ。Workerからの停止・破棄は行わない。
- `FJobSystem`の破棄: 所有側で排他的に行う。破棄と`TrySubmit/Wait`を競合させない。
- Scene、GameObject、GameObjectComponentの通常ライフサイクル: 引き続きGame Threadで変更する。
- DxLib Native描画・Handle生成破棄: Main/Native担当スレッドへ残す。
- Physics Worldの所有構造: 現時点では外部から同時変更しない。今後BroadPhase、NarrowPhase、Island SolverをJob化する際に読み取りSnapshotと確定フェーズを分離する。

## 次の並列化単位

Job基盤の次は、PhysicsのBroadPhaseとIslandを責務として分離してから並列化します。

1. Bodyの独立した速度／位置積分。
2. BroadPhase候補生成。
3. 候補PairごとのNarrowPhaseをWorker-local Contact Bufferへ出力。
4. Contact GraphからIslandを構築。
5. 互いに独立したIslandをWorkerへ割り当ててConstraint Solverを実行。
6. Barrier後に安定したBody/Collider/Pairキー順で結果とEventを統合。

同一Bodyへ複数Workerから同時書き込みするSolverにはしません。一つの巨大Island内部の並列Constraint Solverは、Island間並列を検証した後の別段階です。

## 検証

Threading専用テストでは次を確認します。

- 原子加算・減算・比較交換。
- OS Threadの開始／JoinとThread境界での例外隔離。
- 同期実行Jobと実WorkerのThread識別。
- Mutexによる共有書き込み保護。
- 10,000 Jobのexactly-once実行。
- 4 Producerからの同時投入。
- Worker内からの子Job投入とWait。
- 自己Fence待ちの拒否。
- Job例外の隔離と失敗記録。
- `ParallelFor`の1レーン／複数レーン結果一致と例外時のfalse返却。
- Job投入とShutdownの競合。
- 複数Shutdown呼び出しの完了同期。

Linux/GCCではThreadSanitizer用Presetも用意し、通常のASan/UBSanとは別構成で実行します。利用環境のTSan runtimeが正常にリンクできることを前提とします。
