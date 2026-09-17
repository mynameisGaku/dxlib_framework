// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_JOB_SYSTEM_H
#define TOOLBOX_JOB_SYSTEM_H
#include "Toolbox/Atomic.h"
#include "Toolbox/ConditionVariable.h"
#include "Toolbox/Thread.h"
namespace Toolbox
{
class FJobSystem;
/**
 * まとめて投入したJobの完了数を追跡する。追跡中のJobとWaitが終わるまで生存させる。
 */
class FJobFence
{
public:
	/**
	 * 未追跡の空Fenceを作る。
	 */
	FJobFence() noexcept = default;
	/**
	 * 同じ同期状態を複製できないためコピーを禁止する。
	 */
	FJobFence(const FJobFence&) = delete;
	/**
	 * 同じ同期状態を複製できないためコピー代入を禁止する。
	 */
	FJobFence& operator=(const FJobFence&) = delete;
	/**
	 * 追跡中のJob数が0かを観測する。Fenceを破棄する寿命同期にはJobSystem::Waitを使う。
	 */
	FORCEINLINE bool IsComplete() const noexcept
	{
		return m_Pending.Load() == 0;
	}
	/**
	 * 追跡中のJob数を返す。診断用途でありFence破棄の同期にはWaitを使う。
	 */
	FORCEINLINE uint64 PendingCount() const noexcept
	{
		return m_Pending.Load();
	}
	/**
	 * Job本体から伝播せず捕捉された例外数を返す。Fenceの寿命中で累積する。
	 */
	FORCEINLINE uint64 FailureCount() const noexcept
	{
		return m_Failures.Load();
	}

private:
	friend class FJobSystem;
	/**
	 * 新たに追跡するJob数をPendingへ加える。
	 * @param Count 追加するJob数。
	 */
	void Add_Internal(uint64 Count) noexcept;
	/**
	 * Job一件の完了を記録し、最後なら待機者を起こす。
	 */
	void Complete_Internal() noexcept;
	/**
	 * Job本体から外へ出た例外一件を記録する。
	 */
	void Fail_Internal() noexcept;
	/**
	 * Pendingが0になり最後のComplete処理が同期区間を抜けるまで待つ。
	 */
	void Wait_Internal() noexcept;
	/**
	 * 完了していない追跡Job数。
	 */
	FAtomicCounter m_Pending;
	/**
	 * 捕捉したJob例外の累積件数。
	 */
	FAtomicCounter m_Failures;
	/**
	 * Pending完了通知とFence寿命境界を同期するMutex。
	 */
	FMutex m_Mutex;
	/**
	 * Pendingが0になるまで待機する条件変数。
	 */
	FConditionVariable m_Condition;
};
/**
 * 固定数のWorkerへ短いCPU Jobを配送する。
 * ExecutionThreadCount=1では呼び出しスレッド上で同期実行し、比較・デバッグに利用できる。
 */
class FJobSystem
{
public:
	/**
	 * Worker外を表すGetCurrentWorkerIndexの戻り値。
	 */
	static constexpr uint32 InvalidWorkerIndex = static_cast<uint32>(-1);
	/**
	 * Job Systemを構築する。0はOS論理スレッド数、1は完全シングルスレッド実行。
	 * @param ExecutionThreadCount 使用する実行レーン数。2以上ではN-1本のWorkerを所有する。
	 */
	explicit FJobSystem(uint32 ExecutionThreadCount = 0);
	/**
	 * 受付を停止し受理済みJobを完了させてからWorkerと内部資源を破棄する。
	 */
	~FJobSystem();
	/**
	 * WorkerとQueueの所有権を複製できないためコピーを禁止する。
	 */
	FJobSystem(const FJobSystem&) = delete;
	/**
	 * WorkerとQueueの所有権を複製できないためコピー代入を禁止する。
	 */
	FJobSystem& operator=(const FJobSystem&) = delete;
	/**
	 * Job受付を止め、受理済みJobを完了させてWorkerをJoinする。複数回呼べる。
	 * Worker Jobの中から呼び出してはいけない。
	 */
	void Shutdown() noexcept;
	/**
	 * 受付中ならJobを一度だけ実行対象へ追加する。停止後はfalseを返す。
	 * Fenceを指定した場合は実行完了までFenceのPendingへ含める。
	 * @param Function 引数を取らないJob本体。
	 * @param Fence 任意の完了Fence。Job完了まで生存させる。
	 */
	template <typename F> bool TrySubmit(F&& Function, FJobFence* Fence = nullptr)
	{
		using TCallable = TDecay<F>;
		TCallable* Callable = new TCallable(Forward<F>(Function));
		bool Accepted = false;
		try
		{
			Accepted = SubmitRaw_Internal(Callable, &InvokeCallable_Internal<TCallable>,
										  &DestroyCallable_Internal<TCallable>, Fence);
		}
		catch (...)
		{
			delete Callable;
			throw;
		}
		if (!Accepted)
		{
			delete Callable;
		}
		return Accepted;
	}
	/**
	 * Fenceが完了するまで待つ。同じJobSystemのJob実行中なら待機中にQueueを処理する。
	 * 現在Job自身を含むFenceは循環待機になるためfalseを返す。
	 * @param Fence 完了を待つFence。
	 */
	bool Wait(FJobFence& Fence) noexcept;
	/**
	 * 設定から解決した実行レーン数を返す。1は同期実行。
	 */
	uint32 GetExecutionThreadCount() const noexcept;
	/**
	 * 呼び出しスレッドがこのJobSystemの実OS Worker Threadか返す。
	 * 同期実行レーン上のJobではfalseを返す。
	 */
	bool IsInWorkerThread() const noexcept;
	/**
	 * 呼び出しWorkerの0始まり番号を返す。Worker外ならInvalidWorkerIndex。
	 */
	uint32 GetCurrentWorkerIndex() const noexcept;
	/**
	 * Job本体から外へ伝播させず捕捉した例外数を返す。
	 */
	uint64 GetUnhandledExceptionCount() const noexcept;
	/**
	 * 受理したJob数を返す。
	 */
	uint64 GetSubmittedJobCount() const noexcept;
	/**
	 * 実行を終えたJob数を返す。例外終了も一回の完了として数える。
	 */
	uint64 GetCompletedJobCount() const noexcept;

private:
	/**
	 * 型消去したJob本体の実行入口。
	 */
	using FInvoke = void (*)(void* Context);
	/**
	 * 型消去したJob捕捉状態の破棄入口。
	 */
	using FDestroy = void (*)(void* Context);
	/**
	 * 型消去前のCallableを復元して実行する。
	 * @param Context TCallableとして確保した捕捉状態。
	 */
	template <typename F> static void InvokeCallable_Internal(void* Context)
	{
		(*static_cast<F*>(Context))();
	}
	/**
	 * 型消去前のCallableを復元して破棄する。
	 * @param Context TCallableとして確保した捕捉状態。
	 */
	template <typename F> static void DestroyCallable_Internal(void* Context) noexcept
	{
		delete static_cast<F*>(Context);
	}
	/**
	 * 型消去済みJobをQueueまたは同期実行経路へ受理する。
	 * @param Context Jobが所有する捕捉状態。
	 * @param Invoke Job本体の実行入口。
	 * @param Destroy 捕捉状態の破棄入口。
	 * @param Fence 任意の完了Fence。
	 */
	bool SubmitRaw_Internal(void* Context, FInvoke Invoke, FDestroy Destroy, FJobFence* Fence);
	/**
	 * Queue、Worker、同期状態を保持する実装型。
	 */
	struct FImpl;
	/**
	 * Job Systemの全実装状態を排他的に所有する領域。
	 */
	FImpl* m_pImpl = nullptr;
};
/**
 * 独立した添字範囲をJob Systemへ分割して処理する。
 * FunctionはParallelForの完了まで呼び出し元が保持し、各添字は一度だけ呼ばれる。
 * @param Jobs 使用するJob System。
 * @param Count 処理する要素数。
 * @param Function size_t添字を受け取る処理。
 * @param MinimumBatch 一つのJobへまとめる最小要素数。
 * @return 全範囲を受理・実行でき、Job例外もなかった場合true。
 */
template <typename F>
bool ParallelFor(FJobSystem& Jobs, size_t Count, F&& Function, size_t MinimumBatch = 1)
{
	if (Count == 0)
	{
		return true;
	}
	if (MinimumBatch == 0)
	{
		MinimumBatch = 1;
	}
	const uint32 Lanes = Jobs.GetExecutionThreadCount();
	if (Lanes <= 1 || Count <= MinimumBatch)
	{
		FJobFence Fence;
		const bool Accepted = Jobs.TrySubmit([Count, &Function]()
		{
			for (size_t Index = 0; Index < Count; ++Index)
			{
				Function(Index);
			}
		}, &Fence);
		if (!Accepted)
		{
			return false;
		}
		return Jobs.Wait(Fence) && Fence.FailureCount() == 0;
	}
	const size_t DesiredJobs = static_cast<size_t>(Lanes) * 4;
	size_t Batch = Count / DesiredJobs;
	if (Count % DesiredJobs != 0)
	{
		++Batch;
	}
	if (Batch < MinimumBatch)
	{
		Batch = MinimumBatch;
	}
	FJobFence Fence;
	size_t Begin = 0;
	while (Begin < Count)
	{
		const size_t Remaining = Count - Begin;
		const size_t End = Batch < Remaining ? Begin + Batch : Count;
		const bool Accepted = Jobs.TrySubmit([Begin, End, &Function]()
		{
			for (size_t Index = Begin; Index < End; ++Index)
			{
				Function(Index);
			}
		}, &Fence);
		if (!Accepted)
		{
			Jobs.Wait(Fence);
			return false;
		}
		Begin = End;
	}
	return Jobs.Wait(Fence) && Fence.FailureCount() == 0;
}
} // namespace Toolbox
#endif
