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

現在実行中のJob自身または祖先Jobを含むFenceを、そのJobから待つことは循環依存なので`Wait()`が`false`を返します。別Fenceの子JobはWorkerがQueueを手伝いながら待つため、Worker数が少ない場合でも単純な親子待ちで停止しません。別Job SystemのFence待ちはQueue処理を伴わないblockとなり、相手側の完了で戻ります。

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
- `FJobSystem::TrySubmit()`: 複数Producerから同時利用可能。捕捉の複写に失敗した場合は例外が呼び出し側へ伝播し、そのJobは受理されない。受理済みJobの完了には影響しない。
- `FJobSystem::Wait()`: 外部スレッドおよび同じJob Systemの実行中Jobから利用可能。ただし自己・祖先Fence待ちは拒否し、循環するFence依存は利用側で作らない。別Job SystemのFence待ちはblockする。`Wait()`帰還後はそのFenceのJobが完了件数に含まれる。`IsComplete()`の観測だけでは件数・破棄の同期は保証しない。
- `FJobSystem::Shutdown()`: 複数の外部所有スレッドから同時呼び出し可能で、全呼び出しがJoin完了を待つ。Workerからの停止・破棄は行わない。
- `FJobSystem`の破棄: 所有側で排他的に行う。破棄と`TrySubmit/Wait`を競合させない。
- Scene、GameObject、GameObjectComponentの通常ライフサイクル: 引き続きGame Threadで変更する。
- DxLib Native描画・Handle生成破棄: Main/Native担当スレッドへ残す。
- Physics Worldの所有構造: 現時点では外部から同時変更しない。積分・BroadPhase・NarrowPhaseは借用Job Systemで並列化済み（安定順マージ、1/N一致を回帰）。Island Solverの並列化は別段階。Workerは構造を変更せず、NarrowPhaseは専用領域だけを書く。

## 共有Task Dispatcher

`Dxf::FTaskDispatcher`はApplicationが所有する共有実行窓口です。借用Job Systemへ
準備（`Prepare`）を分散し、所有スレッドの`PumpCommits`で投入順に反映（`Commit`）します。

- 準備はSceneやDxLibへ直接書かず、Taskが所有するCPU結果だけを書きます。
- 反映は準備済みの先頭から順に行い、先頭が未完了なら後続を待ちます。
- `FTaskScope`はDispatcher識別子・位置・世代を持ち、別Dispatcher・削除後・
  再利用後の利用を拒否します。親の取り消しは子孫へ伝播します。
- 取り消しは強制スレッド停止ではなく協調方式です。未反映の要求は上限件数で拒否します。
- `Shutdown`は受付を止め、実行中の準備を待って未反映を所有スレッドで破棄します。
- Scene切り替えでは旧Scopeを失効させて新Scopeを作り、切替失敗時は旧Scopeを維持します。

## 次の並列化単位

Job基盤とPhysicsの積分・BroadPhase・NarrowPhase・Island診断は接続済みです。残りは次です。

1. 互いに独立したIslandをWorkerへ割り当ててConstraint Solverを実行。
2. Barrier後に安定したBody/Collider/Pairキー順で結果とEventを統合。

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
- 自己・祖先Fence待ちの拒否。
- 別Job SystemのFence待ちと全Workerの子待ち。
- 捕捉の複写失敗と破棄時再入の隔離。
- 繰り返しShutdownの冪等性。
- Job例外の隔離と失敗記録。
- `ParallelFor`の1レーン／複数レーン結果一致と例外時のfalse返却。
- Job投入とShutdownの競合。
- 複数Shutdown呼び出しの完了同期。

Linux/GCCではThreadSanitizer用Presetも用意し、通常のASan/UBSanとは別構成で実行します。利用環境のTSan runtimeが正常にリンクできることを前提とします。
