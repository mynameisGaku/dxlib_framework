// SPDX-License-Identifier: NOASSERTION
#include "Dxf/TaskDispatcher.h"
namespace Dxf
{
// 借用するJob Systemと設定を受け取り、初期状態を構築する。
// @param Jobs 準備の実行に借用するJob System。
// @param Settings 動作設定。
FTaskDispatcher::FTaskDispatcher(Toolbox::FJobSystem& Jobs, FTaskSettings Settings)
    : m_pJobs(&Jobs), m_Settings(Settings)
{
	// 発行済みのDispatcherと重ならない識別子。
	static Toolbox::TAtomic<Toolbox::uint32> NextId{1};
	m_DispatcherId = NextId.FetchAdd(1);
	if (m_DispatcherId == 0)
	{
		throw Toolbox::FException("Task dispatcher identifier overflow");
	}
	// 位置0のRoot Scope。
	FScopeRecord Root;
	Root.Parent = 0;
	Root.Generation = m_RootGeneration;
	Root.bAlive = true;
	m_Scopes.PushBack(Root);
}
// 受付を止め、受理済みを破棄してWorkerと内部資源を解放する。
FTaskDispatcher::~FTaskDispatcher()
{
	Shutdown();
}
// 子Scopeを作る。無効な親はRoot Scopeとして扱う。
// @param Parent 親にするScope。
FTaskScope FTaskDispatcher::CreateScope(const FTaskScope& Parent)
{
	Toolbox::FScopedLock Lock(m_Mutex);
	// 親Scopeの位置。無効な指定はRoot Scope。
	Toolbox::uint32 ParentIndex = 0;
	if (Parent.IsValid())
	{
		if (!IsScopeAlive_Internal(Parent))
		{
			return {};
		}
		ParentIndex = Parent.Index;
	}
	// 再利用する空き位置。
	Toolbox::uint32 Index = m_Scopes.Size();
	for (Toolbox::uint32 Slot = 1; Slot < m_Scopes.Size(); ++Slot)
	{
		if (!m_Scopes[Slot].bAlive)
		{
			Index = Slot;
			break;
		}
	}
	// 新しい世代のScope記録。
	FScopeRecord Record;
	Record.Parent = ParentIndex;
	Record.Generation = m_NextGeneration;
	Record.bAlive = true;
	++m_NextGeneration;
	if (Index == m_Scopes.Size())
	{
		m_Scopes.PushBack(Record);
	}
	else
	{
		m_Scopes[Index] = Record;
	}
	FTaskScope Scope;
	Scope.Dispatcher = m_DispatcherId;
	Scope.Index = Index;
	Scope.Generation = Record.Generation;
	return Scope;
}
// Scopeを失効させ子孫を取り消す。存在しない指定は無視する。
// @param Scope 失効させるScope。
void FTaskDispatcher::DestroyScope(const FTaskScope& Scope) noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	if (Scope.Dispatcher != m_DispatcherId || Scope.Index == 0 || Scope.Index >= m_Scopes.Size())
	{
		return;
	}
	FScopeRecord& Record = m_Scopes[Scope.Index];
	if (!Record.bAlive || Record.Generation != Scope.Generation)
	{
		return;
	}
	Record.bAlive = false;
	CancelAt_Internal(Scope.Index);
}
// Taskを受け付ける。停止中・上限超過・失効Scopeではfalseを返す。
// @param Request 実行する準備と反映。
bool FTaskDispatcher::Submit(FTaskRequest&& Request)
{
	// 要求の所属位置と世代。
	Toolbox::uint32 ScopeIndex = 0;
	Toolbox::uint64 ScopeGeneration = m_RootGeneration;
	// 要求の通し番号。
	Toolbox::uint64 Sequence = 0;
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		if (m_bStopping)
		{
			return false;
		}
		if (Request.Scope.IsValid())
		{
			if (!IsScopeAlive_Internal(Request.Scope))
			{
				return false;
			}
			ScopeIndex = Request.Scope.Index;
			ScopeGeneration = Request.Scope.Generation;
		}
		if (m_Tasks.Size() >= m_Settings.MaxPending)
		{
			return false;
		}
		Sequence = m_NextSequence;
		++m_NextSequence;
		FTaskRecord Record;
		Record.Prepare = Toolbox::Move(Request.Prepare);
		Record.Commit = Toolbox::Move(Request.Commit);
		Record.ScopeIndex = ScopeIndex;
		Record.ScopeGeneration = ScopeGeneration;
		Record.Sequence = Sequence;
		m_Tasks.PushBack(Toolbox::Move(Record));
	}
	// 準備Jobの借用先。
	FTaskDispatcher* Self = this;
	if (!m_pJobs->TrySubmit([Self, Sequence]()
	    {
		    Self->RunPrepare_Internal(Sequence);
	    }, &m_Fence))
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		for (Toolbox::size_t Index = 0; Index < m_Tasks.Size(); ++Index)
		{
			if (m_Tasks[Index].Sequence == Sequence)
			{
				m_Tasks.Erase(m_Tasks.Begin() + Index);
				break;
			}
		}
		return false;
	}
	return true;
}
// 準備済みの先頭から投入順に反映する。先頭が未完了なら止まる。
FCommitSummary FTaskDispatcher::PumpCommits()
{
	// 反映の集計。
	FCommitSummary Summary;
	for (;;)
	{
		// 今回反映する処理。
		Toolbox::TFunction<bool()> Commit;
		{
			Toolbox::FScopedLock Lock(m_Mutex);
			if (m_Tasks.IsEmpty())
			{
				break;
			}
			FTaskRecord& Front = m_Tasks[0];
			FTaskScope Scope;
			Scope.Dispatcher = m_DispatcherId;
			Scope.Index = Front.ScopeIndex;
			Scope.Generation = Front.ScopeGeneration;
			if (!IsScopeAlive_Internal(Scope) || IsCanceled_Internal(Scope))
			{
				++Summary.Canceled;
				m_Tasks.Erase(m_Tasks.Begin());
				continue;
			}
			if (Front.State == ETaskState::Failed)
			{
				++Summary.Failed;
				m_Tasks.Erase(m_Tasks.Begin());
				continue;
			}
			if (Front.State != ETaskState::Ready)
			{
				break;
			}
			Commit = Toolbox::Move(Front.Commit);
			m_Tasks.Erase(m_Tasks.Begin());
		}
		// 反映に成功したか。
		bool bCommitted = false;
		try
		{
			bCommitted = Commit ? Commit() : true;
		}
		catch (const Toolbox::FException&)
		{
			bCommitted = false;
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
	Toolbox::FScopedLock Lock(m_Mutex);
	Summary.Stalled = static_cast<Toolbox::uint64>(m_Tasks.Size());
	return Summary;
}
// Scopeと子孫を取り消す。実行中の準備は協調点で止まる。
// @param Scope 取り消すScope。
void FTaskDispatcher::Cancel(const FTaskScope& Scope) noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	if (Scope.Dispatcher != m_DispatcherId || Scope.Index >= m_Scopes.Size())
	{
		return;
	}
	const FScopeRecord& Record = m_Scopes[Scope.Index];
	if (Record.Generation != Scope.Generation)
	{
		return;
	}
	CancelAt_Internal(Scope.Index);
}
// Scopeが有効か調べる。
// @param Scope 調べるScope。
bool FTaskDispatcher::IsScopeAlive(const FTaskScope& Scope) noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	return IsScopeAlive_Internal(Scope);
}
// Scopeが取り消し済みか失効しているか調べる。準備側の協調点で使う。
// @param Scope 調べるScope。
bool FTaskDispatcher::IsCanceled(const FTaskScope& Scope) noexcept
{
	Toolbox::FScopedLock Lock(m_Mutex);
	return IsCanceled_Internal(Scope);
}
// 受付を止め、実行中の準備を待って未反映を破棄する。複数回呼べる。
void FTaskDispatcher::Shutdown() noexcept
{
	m_Mutex.Lock();
	if (m_bShutdownComplete)
	{
		m_Mutex.Unlock();
		return;
	}
	m_bStopping = true;
	m_Mutex.Unlock();
	m_pJobs->Wait(m_Fence);
	m_Mutex.Lock();
	m_Tasks.Clear();
	m_bShutdownComplete = true;
	m_Mutex.Unlock();
}
// 所属しない要求に使うRoot Scopeを返す。
FTaskScope FTaskDispatcher::GetRootScope() const noexcept
{
	FTaskScope Scope;
	Scope.Dispatcher = m_DispatcherId;
	Scope.Index = 0;
	Scope.Generation = m_RootGeneration;
	return Scope;
}
// 指定Scopeと子孫を取り消す。呼び出し側で同期していること。
// @param ScopeIndex 取り消すScopeの位置。
void FTaskDispatcher::CancelAt_Internal(Toolbox::uint32 ScopeIndex) noexcept
{
	m_Scopes[ScopeIndex].bCanceled = true;
	for (Toolbox::uint32 Index = 0; Index < m_Scopes.Size(); ++Index)
	{
		// 祖先を辿って対象を含むか調べる。
		Toolbox::uint32 Current = Index;
		for (Toolbox::size_t Depth = 0; Depth <= m_Scopes.Size(); ++Depth)
		{
			if (Current == ScopeIndex)
			{
				m_Scopes[Index].bCanceled = true;
				break;
			}
			const Toolbox::uint32 Parent = m_Scopes[Current].Parent;
			if (Parent == Current)
			{
				break;
			}
			Current = Parent;
		}
	}
}
// Scopeが有効か調べる。呼び出し側で同期していること。
// @param Scope 調べるScope。
bool FTaskDispatcher::IsScopeAlive_Internal(const FTaskScope& Scope) const noexcept
{
	return Scope.Dispatcher == m_DispatcherId && Scope.Index < m_Scopes.Size() &&
	       m_Scopes[Scope.Index].Generation == Scope.Generation && m_Scopes[Scope.Index].bAlive;
}
// Scopeか祖先が取り消し済みか失効しているか調べる。呼び出し側で同期していること。
// @param Scope 調べるScope。
bool FTaskDispatcher::IsCanceled_Internal(const FTaskScope& Scope) const noexcept
{
	if (Scope.Dispatcher != m_DispatcherId || Scope.Index >= m_Scopes.Size())
	{
		return true;
	}
	if (m_Scopes[Scope.Index].Generation != Scope.Generation)
	{
		return true;
	}
	// 調べるScopeからRootへ辿る位置。
	Toolbox::uint32 Current = Scope.Index;
	for (Toolbox::size_t Depth = 0; Depth <= m_Scopes.Size(); ++Depth)
	{
		const FScopeRecord& Record = m_Scopes[Current];
		if (!Record.bAlive || Record.bCanceled)
		{
			return true;
		}
		if (Current == 0)
		{
			return Record.Generation != m_RootGeneration;
		}
		Current = Record.Parent;
	}
	return true;
}
// 通し番号の要求を準備する。破棄済みなら何もしない。
// @param Sequence 実行する記録の通し番号。
void FTaskDispatcher::RunPrepare_Internal(Toolbox::uint64 Sequence) noexcept
{
	// 実行する準備処理。
	Toolbox::TFunction<ETaskPrepare()> Prepare;
	// 要求の所属。
	FTaskScope Scope;
	// 記録を見つけたか。
	bool bFound = false;
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		for (Toolbox::size_t Index = 0; Index < m_Tasks.Size(); ++Index)
		{
			if (m_Tasks[Index].Sequence == Sequence)
			{
				bFound = true;
				Scope.Dispatcher = m_DispatcherId;
				Scope.Index = m_Tasks[Index].ScopeIndex;
				Scope.Generation = m_Tasks[Index].ScopeGeneration;
				if (IsCanceled_Internal(Scope))
				{
					m_Tasks[Index].State = ETaskState::Canceled;
					return;
				}
				Prepare = Toolbox::Move(m_Tasks[Index].Prepare);
				break;
			}
		}
		if (!bFound)
		{
			return;
		}
		if (!Prepare)
		{
			for (Toolbox::size_t Index = 0; Index < m_Tasks.Size(); ++Index)
			{
				if (m_Tasks[Index].Sequence == Sequence && m_Tasks[Index].State == ETaskState::Submitted)
				{
					m_Tasks[Index].State = ETaskState::Ready;
					break;
				}
			}
			return;
		}
	}
	// 準備の結果。
	ETaskPrepare Outcome = ETaskPrepare::Failed;
	try
	{
		Outcome = Prepare();
	}
	catch (const Toolbox::FException&)
	{
		Outcome = ETaskPrepare::Failed;
	}
	catch (...)
	{
		Outcome = ETaskPrepare::Failed;
	}
	Toolbox::FScopedLock Lock(m_Mutex);
	for (Toolbox::size_t Index = 0; Index < m_Tasks.Size(); ++Index)
	{
		if (m_Tasks[Index].Sequence == Sequence && m_Tasks[Index].State == ETaskState::Submitted)
		{
			if (Outcome == ETaskPrepare::Success)
			{
				m_Tasks[Index].State = ETaskState::Ready;
			}
			else if (Outcome == ETaskPrepare::Canceled)
			{
				m_Tasks[Index].State = ETaskState::Canceled;
			}
			else
			{
				m_Tasks[Index].State = ETaskState::Failed;
			}
			break;
		}
	}
}
} // namespace Dxf
