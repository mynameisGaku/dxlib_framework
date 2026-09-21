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
	 * 未完了のまま保持する上限件数。これを超える投入は拒否する。
	 */
	Toolbox::uint32 MaxPending = 256;
};
/**
 * 共有Job Systemへ準備を分散し、所有スレッドで順序どおり反映する。
 * 所有スレッドとWorkerの両方からSubmitとCancelを使える。
 * PumpCommitsとShutdownは所有スレッドから呼ぶ。
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
	 * 子Scopeを作る。無効な親はRoot Scopeとして扱う。
	 * @param Parent 親にするScope。
	 */
	FTaskScope CreateScope(const FTaskScope& Parent = {});
	/**
	 * Scopeを失効させ子孫を取り消す。存在しない指定は無視する。
	 * @param Scope 失効させるScope。
	 */
	void DestroyScope(const FTaskScope& Scope) noexcept;
	/**
	 * Taskを受け付ける。停止中・上限超過・失効Scopeではfalseを返す。
	 * 捕捉の確保に失敗した場合は例外が呼び出し側へ伝播する。
	 * @param Request 実行する準備と反映。
	 */
	bool Submit(FTaskRequest&& Request);
	/**
	 * 準備済みの先頭から投入順に反映する。先頭が未完了なら止まる。
	 * 反映中のSubmitやCancelは次の反復で考慮する。
	 */
	FCommitSummary PumpCommits();
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
	 * 受付を止め、実行中の準備を待って未反映を破棄する。複数回呼べる。
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
	};
	/**
	 * 指定Scopeと子孫を取り消す。呼び出し側で同期していること。
	 * @param ScopeIndex 取り消すScopeの位置。
	 */
	void CancelAt_Internal(Toolbox::uint32 ScopeIndex) noexcept;
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
