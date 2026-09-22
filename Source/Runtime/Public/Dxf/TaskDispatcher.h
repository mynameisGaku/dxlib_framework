// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TASK_DISPATCHER_H
#define DXF_TASK_DISPATCHER_H
#include "Toolbox/Function.h"
#include "Toolbox/JobSystem.h"
#include "Toolbox/Mutex.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * Workerでの準備処理の結果。失敗と協調取り消しはCommitを実行しない。
 */
enum class ETaskPrepare : Toolbox::uint8
{
	/**
	 * 準備ができCommitへ進める。
	 */
	Success,
	/**
	 * 準備に失敗しCommitを実行しない。
	 */
	Failed,
	/**
	 * 取り消し要求を受けCommitを実行しない。
	 */
	Canceled
};
/**
 * Taskの所属を示す世代付きの非所有ハンドル。
 */
struct FTaskScope
{
	/**
	 * 発行したDispatcherの識別子。0は無効。
	 */
	Toolbox::uint32 Dispatcher = 0;
	/**
	 * Dispatcher内の位置。
	 */
	Toolbox::uint32 Index = 0;
	/**
	 * 同じ位置の再使用を見分ける世代。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 同じ所属を指すか調べる。
	 */
	bool operator==(const FTaskScope&) const = default;
	/**
	 * 有効な所属か調べる。
	 */
	FORCEINLINE bool IsValid() const noexcept
	{
		return Dispatcher != 0;
	}
};
/**
 * 投入する一つのTask。捕捉はDispatcherがCommitまたは破棄まで保持する。
 */
struct FTaskRequest
{
	/**
	 * Workerで実行するCPU準備。SceneやDxLibへ直接書かない。
	 */
	Toolbox::TFunction<ETaskPrepare()> Prepare;
	/**
	 * 所有スレッドで投入順に実行する反映。falseはCommit失敗。
	 */
	Toolbox::TFunction<bool()> Commit;
	/**
	 * 所属するScope。無効ならDispatcherのRoot Scopeへ付ける。
	 */
	FTaskScope Scope;
};
/**
 * Commit反映の集計。
 */
struct FCommitSummary
{
	/**
	 * 反映した件数。
	 */
	Toolbox::uint64 Committed = 0;
	/**
	 * 準備または反映に失敗した件数。
	 */
	Toolbox::uint64 Failed = 0;
	/**
	 * 取り消しや失効で反映しなかった件数。
	 */
	Toolbox::uint64 Canceled = 0;
	/**
	 * 先頭待ちで残った件数。
	 */
	Toolbox::uint64 Stalled = 0;
};
/**
 * Dispatcherの動作設定。
 */
struct FTaskSettings
{
	/**
	 * 保持する要求の上限件数。取消後にJobキューへ残る空通知も数え、超過投入を拒否する。
	 */
	Toolbox::uint32 MaxPending = 256;
};
/**
 * 共有Job Systemへ準備を分散し、所有スレッドで順序どおり反映する。
 * 所有スレッドとWorkerの両方からSubmitとCancelを使える。
 * PumpCommits・待機・破棄は構築スレッドのJob/Commit/捕捉破棄の外で行う。
 * 破棄開始前に外部Producerの呼び出し元を停止し、Dispatcherを借用先より先に破棄する。
 */
class FTaskDispatcher
{
public:
	/**
	 * 借用するJob Systemと設定を受け取り、初期状態を構築する。
	 * Job SystemはDispatcherより長く生存させる。
	 * @param Jobs 準備の実行に借用するJob System。
	 * @param Settings 動作設定。
	 */
	explicit FTaskDispatcher(Toolbox::FJobSystem& Jobs, FTaskSettings Settings = {});
	/**
	 * 受付を止め、受理済みを破棄してWorkerと内部資源を解放する。
	 */
	~FTaskDispatcher();
	/**
	 * WorkerとQueueの所有権を複製できないためコピーを禁止する。
	 */
	FTaskDispatcher(const FTaskDispatcher&) = delete;
	/**
	 * WorkerとQueueの所有権を複製できないためコピー代入を禁止する。
	 */
	FTaskDispatcher& operator=(const FTaskDispatcher&) = delete;
	/**
	 * 子Scopeを作る。空の親はRoot。停止中・失効または取消済みの親では無効値を返す。
	 * @param Parent 親にするScope。
	 */
	FTaskScope CreateScope(const FTaskScope& Parent = {});
	/**
	 * Scopeと子孫を失効・取り消しする。準備や捕捉の解放は待たない。
	 * 利用対象を破棄する前にはRetireScopeの成功が必要。存在しない指定は無視する。
	 * @param Scope 失効させるScope。
	 */
	void DestroyScope(const FTaskScope& Scope) noexcept;
	/**
	 * 子孫を失効させ、投入処理・準備の完了と取消済み捕捉の解放を待つ。
	 * 対象Scopeと子孫だけを待ち、Rootや兄弟の準備・投入・捕捉には待機しない。
	 * Commitは実行しない。他Scopeの取消済み要求もこの操作では回収しない。
	 * 未開始のPrepareは実行せず捕捉を回収する。実行中のPrepareは終了まで待つ。
	 * Jobキューに残る空の通知はDispatcher全体のFenceで追跡し、受付上限にも含める。
	 * 所有スレッドの非再入区間のみ。Job/Commit/捕捉破棄中、Root、別Dispatcherはfalse。
	 * 同じDispatcherが発行した旧世代は、新世代を取り消さず完了待ちできる。
	 * 世代の再利用はTaskと捕捉の解放後だけ。別世代へ再利用済みなら退役済みとする。
	 * falseの場合は退役完了を保証しないため、対象を破棄してはいけない。
	 * @param Scope 破棄前に同期するScene等のScope。
	 */
	bool RetireScope(const FTaskScope& Scope) noexcept;
	/**
	 * Taskを受け付ける。停止中・上限超過・失効または取消済みScopeではfalseを返す。
	 * 確保失敗は登録を撤回して例外を伝播する。受理前拒否ではRequestを移動しない。
	 * @param Request 実行する準備と反映。
	 */
	bool Submit(FTaskRequest&& Request);
	/**
	 * 準備済みの先頭から投入順に反映する。先頭が未完了なら止まる。
	 * 反映中のSubmitやCancelは次の反復で考慮する。
	 * 構築スレッド以外、Job・反映・捕捉破棄からの再入は例外で拒否する。
	 */
	FCommitSummary PumpCommits();
	/**
	 * 受理済みの準備がすべて終わるまで待つ。Commitは実行しない。
	 * 終わらない準備と組み合わせると戻らないため、所有スレッドから使う。
	 * Job・Commit・捕捉破棄中や別スレッドからの誤呼び出しでは待機せず戻る。
	 * 戻り値のない観測用APIなので、破棄可否の判定にはRetireScopeを使う。
	 */
	void WaitForPrepares() noexcept;
	/**
	 * Scopeと子孫を取り消す。実行中の準備は協調点で止まる。
	 * 存在しない指定は無視する。
	 * @param Scope 取り消すScope。
	 */
	void Cancel(const FTaskScope& Scope) noexcept;
	/**
	 * Scopeが有効か調べる。
	 * @param Scope 調べるScope。
	 */
	bool IsScopeAlive(const FTaskScope& Scope) noexcept;
	/**
	 * Scopeが取り消し済みか失効しているか調べる。準備側の協調点で使う。
	 * @param Scope 調べるScope。
	 */
	bool IsCanceled(const FTaskScope& Scope) noexcept;
	/**
	 * 受付とScope生成を止め、投入処理・準備を待って未反映を破棄する。
	 * Job/Commit/捕捉破棄中や別スレッドでは停止要求だけを行う。
	 * その場合、所有スレッドの安全な境界で再度呼び出して終了を完了させる。
	 */
	void Shutdown() noexcept;
	/**
	 * 所属しない要求に使うRoot Scopeを返す。
	 */
	FTaskScope GetRootScope() const noexcept;
	/**
	 * このDispatcherの識別子を返す。
	 */
	FORCEINLINE Toolbox::uint32 GetDispatcherId() const noexcept
	{
		return m_DispatcherId;
	}

	/**
	 * 構築したスレッドから呼ばれているかを調べる。
	 */
	FORCEINLINE bool IsOwnerThread() const noexcept
	{
		return Toolbox::FThread::CurrentThreadId() == m_OwnerThread;
	}
	/**
	 * 所有スレッドの、Scene終了などの同期を開始できる実行区間かを調べる。
	 * Job・Commit・捕捉破棄・進行中の待機からはfalse。停止完了の判定ではない。
	 */
	FORCEINLINE bool CanSynchronize() const noexcept
	{
		return CanDrain_Internal();
	}

private:
	/**
	 * 要求の進行状態。
	 */
	enum class ETaskState : Toolbox::uint8
	{
		/**
		 * 受理済みで準備が終わっていない。
		 */
		Submitted,
		/**
		 * 準備ができ反映待ち。
		 */
		Ready,
		/**
		 * 準備に失敗した。
		 */
		Failed,
		/**
		 * 取り消された。
		 */
		Canceled
	};
	/**
	 * 受理した一つの要求と進行状態。
	 */
	struct FTaskRecord
	{
		/**
		 * Workerで実行するCPU準備。
		 */
		Toolbox::TFunction<ETaskPrepare()> Prepare;
		/**
		 * 所有スレッドで実行する反映。
		 */
		Toolbox::TFunction<bool()> Commit;
		/**
		 * 所属Scopeの位置。
		 */
		Toolbox::uint32 ScopeIndex = 0;
		/**
		 * 所属Scopeの世代。
		 */
		Toolbox::uint64 ScopeGeneration = 0;
		/**
		 * 投入順の通し番号。
		 */
		Toolbox::uint64 Sequence = 0;
		/**
		 * 進行状態。
		 */
		ETaskState State = ETaskState::Submitted;
		/**
		 * Job Systemへの投入呼び出しが確定していない間は、反映・回収しない。
		 */
		bool bSubmitting = true;
		/**
		 * WorkerがPrepareの実行権を取得したか。未開始の要求だけを先行回収できる。
		 */
		bool bPreparing = false;
	};
	/**
	 * Scopeの有効性と取り消し状態。
	 */
	struct FScopeRecord
	{
		/**
		 * 親Scopeの位置。Rootは自身を指す。
		 */
		Toolbox::uint32 Parent = 0;
		/**
		 * 親位置が再利用されたとき、別世代の親子関係を辿らないための世代。
		 */
		Toolbox::uint64 ParentGeneration = 0;
		/**
		 * 同じ位置の再使用を見分ける世代。
		 */
		Toolbox::uint64 Generation = 0;
		/**
		 * 有効なScopeか。
		 */
		bool bAlive = false;
		/**
		 * 取り消し済みか。
		 */
		bool bCanceled = false;
		/**
		 * 自身と子孫のTaskが保持する参照数。捕捉の解放まで祖先の再利用を防ぐ。
		 */
		Toolbox::uint64 PendingTasks = 0;
	};
	/**
	 * 指定Scopeと子孫を取り消す。呼び出し側で同期していること。
	 * @param ScopeIndex 取り消すScopeの位置。
	 * @param bDestroy 子孫の生存フラグも失効させるか。
	 */
	void CancelAt_Internal(Toolbox::uint32 ScopeIndex, bool bDestroy = false) noexcept;
	/**
	 * 親の世代も照合して子孫かを判定する。Mutex保持中に使う。
	 * @param Child 調べる子孫候補。
	 * @param Parent 起点の親。
	 */
	bool IsDescendant_Internal(FTaskScope Child, const FTaskScope& Parent) const noexcept;
	/**
	 * 所有スレッドの、待機と捕捉解放を実行できる区間か調べる。
	 */
	bool CanDrain_Internal() const noexcept;
	/**
	 * 進行中の投入がFenceへ登録または撤回を終えるまで待つ。
	 */
	void WaitForSubmissions_Internal() noexcept;
	/**
	 * Job投入の成否を確定し、拒否・例外時は登録と捕捉を撤回する。
	 * @param Sequence 投入中の通し番号。
	 * @param bAccepted Job Systemが受理したか。
	 */
	void FinishSubmission_Internal(Toolbox::uint64 Sequence, bool bAccepted) noexcept;
	/**
	 * 記録を移動してから削除する。移動先は空、Mutex保持中に使う。
	 * @param Index 取り出す位置。
	 * @param Record ロック外で破棄する移動先。
	 */
	void TakeTask_Internal(Toolbox::size_t Index, FTaskRecord& Record) noexcept;
	/**
	 * Task所属からRootまでの保持数を更新する。生存中または保持中の系譜とMutexが必要。
	 * @param ScopeIndex 対象Taskの所属位置。
	 * @param bAdd 受理時は加算、最終捕捉解放後は減算する。
	 */
	void AdjustPending_Internal(Toolbox::uint32 ScopeIndex, bool bAdd) noexcept;
	/**
	 * 捕捉をロック外で解放し、所属と祖先の保持数を減らす。Mutex外から呼ぶ。
	 * @param Record 登録領域から取り出したTask。空記録には何もしない。
	 */
	void ReleaseTask_Internal(FTaskRecord& Record) noexcept;
	/**
	 * Scopeが有効か調べる。呼び出し側で同期していること。
	 * @param Scope 調べるScope。
	 */
	bool IsScopeAlive_Internal(const FTaskScope& Scope) const noexcept;
	/**
	 * Scopeか祖先が取り消し済みか失効しているか調べる。呼び出し側で同期していること。
	 * @param Scope 調べるScope。
	 */
	bool IsCanceled_Internal(const FTaskScope& Scope) const noexcept;
	/**
	 * 通し番号の要求を準備する。破棄済みなら何もしない。
	 * @param Sequence 実行する記録の通し番号。
	 */
	void RunPrepare_Internal(Toolbox::uint64 Sequence) noexcept;
	/**
	 * 準備の実行に借用するJob System。
	 */
	Toolbox::FJobSystem* m_pJobs;
	/**
	 * 動作設定。
	 */
	FTaskSettings m_Settings;
	/**
	 * このDispatcherの識別子。
	 */
	Toolbox::uint32 m_DispatcherId;
	/**
	 * 構築スレッド。Commitと同期APIの呼び出しを制限する。
	 */
	Toolbox::uint64 m_OwnerThread;
	/**
	 * Root Scopeの世代。
	 */
	Toolbox::uint64 m_RootGeneration = 1;
	/**
	 * 次に発行するScope世代。
	 */
	Toolbox::uint64 m_NextGeneration = 2;
	/**
	 * 次に発行するTask通し番号。
	 */
	Toolbox::uint64 m_NextSequence = 1;
	/**
	 * Scopeの登録領域。位置0はRoot Scope。
	 */
	Toolbox::TVector<FScopeRecord> m_Scopes;
	/**
	 * 投入順に並ぶ要求の登録領域。
	 */
	Toolbox::TVector<FTaskRecord> m_Tasks;
	/**
	 * 実行中の準備を数えるFence。
	 */
	Toolbox::FJobFence m_Fence;
	/**
	 * 登録領域を守るMutex。
	 */
	Toolbox::FMutex m_Mutex;
	/**
	 * 投入確定・準備完了・捕捉解放を通知し、全体待機とScope単位待機を起こす。
	 */
	Toolbox::FConditionVariable m_ProgressChanged;
	/**
	 * 登録後、Job投入または撤回と捕捉の解放を完了していない呼び出し数。
	 */
	Toolbox::uint64 m_Submitting = 0;
	/**
	 * 捕捉回収後もJobキューに残る空通知の数。受付上限は通知消費まで回復させない。
	 */
	Toolbox::uint64 m_CanceledQueuedJobs = 0;
	/**
	 * 所有スレッドで反映・完了待ち・捕捉解放を行っているか。
	 */
	bool m_bDraining = false;
	/**
	 * 受付を止めたか。
	 */
	bool m_bStopping = false;
	/**
	 * 終了処理が完了しているか。
	 */
	bool m_bShutdownComplete = false;
};
} // namespace Dxf
#endif
