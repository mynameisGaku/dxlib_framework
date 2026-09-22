// SPDX-License-Identifier: NOASSERTION
#include "Dxf/TaskDispatcher.h"
namespace Dxf
{
namespace
{
// 捕捉破棄から別Dispatcherを経由して戻る場合も、待機の循環を拒否する。
thread_local Toolbox::uint32 CaptureReleaseDepth = 0;
// 捕捉の解放中だけ再入する待機を禁止する。
class FCaptureReleaseGuard
{
public:
	FCaptureReleaseGuard() noexcept
	{
		++CaptureReleaseDepth;
	}
	~FCaptureReleaseGuard()
	{
		--CaptureReleaseDepth;
	}
};
// 所有スレッドの非再入区間を、例外経路でも復元する。
class FDrainGuard
{
public:
	explicit FDrainGuard(bool& Flag) noexcept : m_pFlag(&Flag)
	{
		*m_pFlag = true;
	}
	~FDrainGuard()
	{
		*m_pFlag = false;
	}
private:
	bool* m_pFlag;
};
}
// 借用先を記録し、Rootと再利用しないDispatcher識別子を発行する。
FTaskDispatcher::FTaskDispatcher(Toolbox::FJobSystem& Jobs, FTaskSettings Settings)
    : m_pJobs(&Jobs), m_Settings(Settings), m_DispatcherId(0),
      m_OwnerThread(Toolbox::FThread::CurrentThreadId())
{
	// 識別子が尽きた場合は0を保ち、周回して既存IDと衝突させない。
	static Toolbox::TAtomic<Toolbox::uint32> NextId{1};
	Toolbox::uint32 Candidate = NextId.Load();
	while (Candidate != 0)
	{
		if (NextId.CompareExchange(Candidate, Candidate + 1))
		{
			m_DispatcherId = Candidate;
			break;
		}
	}
	if (m_DispatcherId == 0)
	{
		throw Toolbox::FException("Task dispatcher identifier overflow");
	}
	// 位置0のRoot Scope。
	FScopeRecord Root;
	Root.ParentGeneration = m_RootGeneration;
	Root.Generation = m_RootGeneration;
	Root.bAlive = true;
	m_Scopes.PushBack(Root);
}
// 外部Producer停止後、所有スレッドの非再入区間で破棄する契約。
FTaskDispatcher::~FTaskDispatcher()
{
	Shutdown();
}
// 停止・取消と同じMutexの下で生成を検査する。
FTaskScope FTaskDispatcher::CreateScope(const FTaskScope& Parent)
{
	Toolbox::FScopedLock Lock(m_Mutex);
	// 空の親だけがRootを指定する。
	const FTaskScope EffectiveParent = Parent.IsValid() ? Parent : GetRootScope();
	if (m_bStopping || m_NextGeneration == 0 || IsCanceled_Internal(EffectiveParent))
	{
		return {};
	}
	// 空き位置の世代は再使用せず、新しい通し番号を割り当てる。
	Toolbox::size_t Index = 1;
	while (Index < m_Scopes.Size() && m_Scopes[Index].bAlive)
	{
		++Index;
	}
	if (Index > static_cast<Toolbox::size_t>(static_cast<Toolbox::uint32>(-1)))
	{
		return {};
	}
	FScopeRecord Record;
	Record.Parent = EffectiveParent.Index;
	Record.ParentGeneration = EffectiveParent.Generation;
	Record.Generation = m_NextGeneration;
	Record.bAlive = true;
	if (Index == m_Scopes.Size())
	{
		m_Scopes.PushBack(Record);
	}
	else
	{
		m_Scopes[Index] = Record;
	}
	++m_NextGeneration;
	return {m_DispatcherId, static_cast<Toolbox::uint32>(Index), Record.Generation};
}
// 非同期取消だけを行い、捕捉や準備の終了は待たない。
void FTaskDispatcher::DestroyScope(const FTaskScope& Scope) noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	if (Scope.Index == 0 || !IsScopeAlive_Internal(Scope))
	{
		return;
	}
	CancelAt_Internal(Scope.Index, true);
}
// Mutexを持った状態では利用者の捕捉を破棄しない。
bool FTaskDispatcher::Submit(FTaskRequest&& Request)
{
	// Recordの解放より後にガードが戻るよう宣言順を固定する。
	FCaptureReleaseGuard ReleaseGuard;
	FTaskRecord Record;
	Toolbox::uint64 Sequence = 0;
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		const FTaskScope Scope = Request.Scope.IsValid() ? Request.Scope : GetRootScope();
		if (m_bStopping || m_NextSequence == 0 || IsCanceled_Internal(Scope) ||
		    m_Tasks.Size() >= m_Settings.MaxPending)
		{
			return false;
		}
		Sequence = m_NextSequence;
		Record.ScopeIndex = Scope.Index;
		Record.ScopeGeneration = Scope.Generation;
		Record.Sequence = Sequence;
		Record.Prepare = Toolbox::Move(Request.Prepare);
		Record.Commit = Toolbox::Move(Request.Commit);
		// 空記録を先に確保する。EmplaceBackの再確保用一時変数へ捕捉を移さない。
		// 既存記録の再配置は捕捉を複製しないnoexcept移動に限定する。
		static_assert(__is_nothrow_constructible(FTaskRecord, FTaskRecord&&));
		static_assert(__is_nothrow_assignable(FTaskRecord&, FTaskRecord&&));
		m_Tasks.EmplaceBack() = Toolbox::Move(Record);
		++m_NextSequence;
		++m_Submitting;
	}
	bool bAccepted = false;
	try
	{
		bAccepted = m_pJobs->TrySubmit([this, Sequence]()
		{
			RunPrepare_Internal(Sequence);
		}, &m_Fence);
	}
	catch (...)
	{
		FinishSubmission_Internal(Sequence, false);
		throw;
	}
	FinishSubmission_Internal(Sequence, bAccepted);
	return bAccepted;
}
// 移動元を空にしてからシフトし、Erase内のユーザーデストラクタを避ける。
void FTaskDispatcher::TakeTask_Internal(Toolbox::size_t Index, FTaskRecord& Record) noexcept
{
	Record = Toolbox::Move(m_Tasks[Index]);
	m_Tasks.Erase(m_Tasks.Begin() + Index);
}
// Fence登録前の隙間と、拒否時の捕捉解放までを投入中として扱う。
void FTaskDispatcher::FinishSubmission_Internal(Toolbox::uint64 Sequence, bool bAccepted) noexcept
{
	FCaptureReleaseGuard ReleaseGuard;
	FTaskRecord Retired;
	if (!bAccepted)
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		for (Toolbox::size_t Index = 0; Index < m_Tasks.Size(); ++Index)
		{
			if (m_Tasks[Index].Sequence == Sequence)
			{
				TakeTask_Internal(Index, Retired);
				break;
			}
		}
	}
	Retired.Prepare = {};
	Retired.Commit = {};
	Toolbox::FScopedLock Lock(m_Mutex);
	--m_Submitting;
	m_SubmissionsChanged.NotifyAll();
}
// 準備とその捕捉の解放が終わった先頭だけを反映する。
FCommitSummary FTaskDispatcher::PumpCommits()
{
	if (!CanDrain_Internal())
	{
		throw Toolbox::FException("Task commits require the owner thread outside callbacks and drains");
	}
	FCommitSummary Summary;
	bool bStopping = false;
	{
		FDrainGuard Drain(m_bDraining);
		for (;;)
		{
			FTaskRecord Retired;
			{
				Toolbox::FScopedLock Lock(m_Mutex);
				if (m_Tasks.IsEmpty() || m_Tasks[0].State == ETaskState::Submitted)
				{
					break;
				}
				TakeTask_Internal(0, Retired);
			}
			// この検査がCommit開始の境界。以後の取消は開始済みCommitを中断しない。
			bool bCanceled = false;
			{
				Toolbox::FScopedLock Lock(m_Mutex);
				const FTaskScope Scope{m_DispatcherId, Retired.ScopeIndex, Retired.ScopeGeneration};
				bCanceled = m_bStopping || IsCanceled_Internal(Scope) || Retired.State == ETaskState::Canceled;
			}
			if (bCanceled)
			{
				++Summary.Canceled;
			}
			else if (Retired.State == ETaskState::Failed)
			{
				++Summary.Failed;
			}
			else
			{
				bool bCommitted = false;
				try
				{
					bCommitted = Retired.Commit ? Retired.Commit() : true;
				}
				catch (...)
				{
					bCommitted = false;
				}
				if (bCommitted)
				{
					++Summary.Committed;
				}
				else
				{
					++Summary.Failed;
				}
			}
			// 反映・取消・失敗のどの経路でも、捕捉破棄はMutex外。
			FCaptureReleaseGuard ReleaseGuard;
			Retired.Prepare = {};
			Retired.Commit = {};
		}
		Toolbox::FScopedLock Lock(m_Mutex);
		Summary.Stalled = static_cast<Toolbox::uint64>(m_Tasks.Size());
		bStopping = m_bStopping;
	}
	if (bStopping)
	{
		Shutdown();
	}
	return Summary;
}
// 所有スレッド以外では所有スレッド専用フラグにも触れない。
bool FTaskDispatcher::CanDrain_Internal() const noexcept
{
	return Toolbox::FThread::CurrentThreadId() == m_OwnerThread && CaptureReleaseDepth == 0 &&
	       !Toolbox::FJobSystem::IsExecutingJob() && !m_bDraining;
}
// 投入中の要求が後からFenceへ現れることを防ぐ。
void FTaskDispatcher::WaitForSubmissions_Internal() noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	while (m_Submitting != 0)
	{
		m_SubmissionsChanged.Wait(m_Mutex);
	}
}
// 不正な実行区間では待機しない。破棄可否の判定にはRetireScopeを使う。
void FTaskDispatcher::WaitForPrepares() noexcept
{
	if (!CanDrain_Internal())
	{
		return;
	}
	FDrainGuard Drain(m_bDraining);
	WaitForSubmissions_Internal();
	m_pJobs->Wait(m_Fence);
}
// 保守的に全Prepareを待ち、取消済み要求と捕捉を回収する。
bool FTaskDispatcher::RetireScope(const FTaskScope& Scope) noexcept
{
	if (!CanDrain_Internal())
	{
		return false;
	}
	FDrainGuard Drain(m_bDraining);
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		if (Scope.Dispatcher != m_DispatcherId || Scope.Index == 0 || Scope.Index >= m_Scopes.Size() ||
		    Scope.Generation == 0 || Scope.Generation > m_Scopes[Scope.Index].Generation)
		{
			return false;
		}
		// 旧世代から新世代を取り消さない。旧世代の準備は下で同期する。
		if (m_Scopes[Scope.Index].Generation == Scope.Generation)
		{
			CancelAt_Internal(Scope.Index, true);
		}
	}
	WaitForSubmissions_Internal();
	if (!m_pJobs->Wait(m_Fence))
	{
		return false;
	}
	for (;;)
	{
		FCaptureReleaseGuard ReleaseGuard;
		FTaskRecord Retired;
		bool bFound = false;
		{
			Toolbox::FScopedLock Lock(m_Mutex);
			for (Toolbox::size_t Index = 0; Index < m_Tasks.Size(); ++Index)
			{
				const FTaskRecord& Record = m_Tasks[Index];
				const FTaskScope Current{m_DispatcherId, Record.ScopeIndex, Record.ScopeGeneration};
				if (Record.State != ETaskState::Submitted &&
				    (IsCanceled_Internal(Current) || Record.State == ETaskState::Canceled))
				{
					TakeTask_Internal(Index, Retired);
					bFound = true;
					break;
				}
			}
		}
		if (!bFound)
		{
			break;
		}
	}
	return true;
}
// 子孫への取消は親子の両方の世代を照合する。
void FTaskDispatcher::Cancel(const FTaskScope& Scope) noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	if (Scope.Dispatcher == m_DispatcherId && Scope.Index < m_Scopes.Size() &&
	    m_Scopes[Scope.Index].Generation == Scope.Generation)
	{
		CancelAt_Internal(Scope.Index);
	}
}
// 照会は捕捉破棄からも呼び出せる。
bool FTaskDispatcher::IsScopeAlive(const FTaskScope& Scope) noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	return IsScopeAlive_Internal(Scope);
}
// 準備中の協調取消点から呼び出せる。
bool FTaskDispatcher::IsCanceled(const FTaskScope& Scope) noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	return IsCanceled_Internal(Scope);
}
// 停止を先に公開し、投入中の処理・全Prepare・捕捉の順で同期する。
void FTaskDispatcher::Shutdown() noexcept
{
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		if (m_bShutdownComplete)
		{
			return;
		}
		m_bStopping = true;
		CancelAt_Internal(0, true);
	}
	if (!CanDrain_Internal())
	{
		return;
	}
	FDrainGuard Drain(m_bDraining);
	WaitForSubmissions_Internal();
	m_pJobs->Wait(m_Fence);
	// まとめて所有権だけを外し、利用者のデストラクタはロック外で実行する。
	Toolbox::TVector<FTaskRecord> Retired;
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		Retired.Swap(m_Tasks);
	}
	{
		FCaptureReleaseGuard ReleaseGuard;
		Retired.Clear();
	}
	Toolbox::FScopedLock Lock(m_Mutex);
	m_bShutdownComplete = true;
}
// Rootのハンドルは不変。停止後の有効性はIsScopeAlive/IsCanceledで調べる。
FTaskScope FTaskDispatcher::GetRootScope() const noexcept
{
	return {m_DispatcherId, 0, m_RootGeneration};
}
// 失効済みノードでも世代を使って古い親子関係を安全に辿る。
bool FTaskDispatcher::IsDescendant_Internal(FTaskScope Child, const FTaskScope& Parent) const noexcept
{
	for (Toolbox::size_t Depth = 0; Depth <= m_Scopes.Size(); ++Depth)
	{
		if (Child.Dispatcher != m_DispatcherId || Child.Index >= m_Scopes.Size() ||
		    m_Scopes[Child.Index].Generation != Child.Generation)
		{
			return false;
		}
		if (Child == Parent)
		{
			return true;
		}
		if (Child.Index == 0)
		{
			return false;
		}
		const FScopeRecord& Record = m_Scopes[Child.Index];
		Child.Index = Record.Parent;
		Child.Generation = Record.ParentGeneration;
	}
	return false;
}
// 子孫全体を一度に取消・失効させ、旧子Scopeからの再投入を拒否する。
void FTaskDispatcher::CancelAt_Internal(Toolbox::uint32 ScopeIndex, bool bDestroy) noexcept
{
	const FTaskScope Parent{m_DispatcherId, ScopeIndex, m_Scopes[ScopeIndex].Generation};
	for (Toolbox::size_t Index = 0; Index < m_Scopes.Size(); ++Index)
	{
		const FTaskScope Child{m_DispatcherId, static_cast<Toolbox::uint32>(Index), m_Scopes[Index].Generation};
		if (IsDescendant_Internal(Child, Parent))
		{
			m_Scopes[Index].bCanceled = true;
			if (bDestroy)
			{
				m_Scopes[Index].bAlive = false;
			}
		}
	}
}
// 自身の世代と生存フラグを照合する。
bool FTaskDispatcher::IsScopeAlive_Internal(const FTaskScope& Scope) const noexcept
{
	return Scope.Dispatcher == m_DispatcherId && Scope.Index < m_Scopes.Size() &&
	       m_Scopes[Scope.Index].Generation == Scope.Generation && m_Scopes[Scope.Index].bAlive;
}
// 各世代を照合してRootへ辿る。親が再利用されていた場合も失効とする。
bool FTaskDispatcher::IsCanceled_Internal(const FTaskScope& Scope) const noexcept
{
	FTaskScope Current = Scope;
	for (Toolbox::size_t Depth = 0; Depth <= m_Scopes.Size(); ++Depth)
	{
		if (!IsScopeAlive_Internal(Current) || m_Scopes[Current.Index].bCanceled)
		{
			return true;
		}
		if (Current.Index == 0)
		{
			return false;
		}
		const FScopeRecord& Record = m_Scopes[Current.Index];
		Current.Index = Record.Parent;
		Current.Generation = Record.ParentGeneration;
	}
	return true;
}
// Prepareの捕捉が解放されるより前にはReadyを公開しない。
void FTaskDispatcher::RunPrepare_Internal(Toolbox::uint64 Sequence) noexcept
{
	Toolbox::TFunction<ETaskPrepare()> Prepare;
	FTaskScope Scope;
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		for (FTaskRecord& Record : m_Tasks)
		{
			if (Record.Sequence == Sequence)
			{
				Scope = {m_DispatcherId, Record.ScopeIndex, Record.ScopeGeneration};
				if (IsCanceled_Internal(Scope))
				{
					Record.State = ETaskState::Canceled;
					return;
				}
				Prepare = Toolbox::Move(Record.Prepare);
				break;
			}
		}
	}
	if (!Scope.IsValid())
	{
		return;
	}
	ETaskPrepare Outcome = ETaskPrepare::Failed;
	try
	{
		Outcome = Prepare ? Prepare() : ETaskPrepare::Success;
	}
	catch (...)
	{
		Outcome = ETaskPrepare::Failed;
	}
	{
		FCaptureReleaseGuard ReleaseGuard;
		Prepare = {};
	}
	Toolbox::FScopedLock Lock(m_Mutex);
	for (FTaskRecord& Record : m_Tasks)
	{
		if (Record.Sequence == Sequence)
		{
			if (IsCanceled_Internal(Scope) || Outcome == ETaskPrepare::Canceled)
			{
				Record.State = ETaskState::Canceled;
			}
			else if (Outcome == ETaskPrepare::Success)
			{
				Record.State = ETaskState::Ready;
			}
			else
			{
				Record.State = ETaskState::Failed;
			}
			break;
		}
	}
}
} // namespace Dxf
