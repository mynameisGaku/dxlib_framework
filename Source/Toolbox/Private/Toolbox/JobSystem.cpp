// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/JobSystem.h"
namespace Toolbox
{
void FJobFence::Add_Internal(uint64 Count) noexcept
{
	if (Count == 0)
	{
		return;
	}
	FScopedLock Lock(m_Mutex);
	m_Pending.FetchAdd(Count);
}
void FJobFence::Complete_Internal() noexcept
{
	FScopedLock Lock(m_Mutex);
	const uint64 Previous = m_Pending.FetchSub(1);
	if (Previous == 1)
	{
		m_Condition.NotifyAll();
	}
}
void FJobFence::Fail_Internal() noexcept
{
	m_Failures.FetchAdd(1);
}
void FJobFence::Wait_Internal() noexcept
{
	m_Mutex.Lock();
	while (m_Pending.Load() != 0)
	{
		m_Condition.Wait(m_Mutex);
	}
	m_Mutex.Unlock();
}
struct FJobSystem::FImpl
{
	struct FJobNode
	{
		void* Context = nullptr;
		FInvoke Invoke = nullptr;
		FDestroy Destroy = nullptr;
		FJobFence* Fence = nullptr;
		FJobNode* pNext = nullptr;
	};
	struct FWorkerContext
	{
		FImpl* pSystem = nullptr;
		uint32 Index = 0;
	};
	/**
	 * 同一スレッド上で実行中のJobと所属Systemの退避先。
	 */
	struct FExecutionFrame
	{
		/**
		 * 実行中Jobが属するJob System。
		 */
		FImpl* pSystem = nullptr;
		/**
		 * 実行中Jobへ渡された完了Fence。指定なしはnullptr。
		 */
		FJobFence* pFence = nullptr;
		/**
		 * 一つ外側で実行中のJob枠。最も外側はnullptr。
		 */
		FExecutionFrame* pPrevious = nullptr;
	};
	FMutex m_QueueMutex;
	FConditionVariable m_QueueCondition;
	FJobNode* m_pHead = nullptr;
	FJobNode* m_pTail = nullptr;
	FThread* m_pWorkers = nullptr;
	FWorkerContext* m_pWorkerContexts = nullptr;
	uint32 m_WorkerThreadCount = 0;
	uint32 m_ExecutionThreadCount = 1;
	bool m_bAccepting = true;
	bool m_bStopping = false;
	bool m_bShutdownComplete = false;
	uint32 m_ActiveInlineExecutors = 0;
	FAtomicCounter m_UnhandledExceptions;
	FAtomicCounter m_SubmittedJobs;
	FAtomicCounter m_CompletedJobs;
	static thread_local FImpl* s_pCurrentSystem;
	static thread_local uint32 s_CurrentWorkerIndex;
	static thread_local FJobFence* s_pCurrentFence;
	static thread_local FExecutionFrame* s_pExecutionStack;

	explicit FImpl(uint32 RequestedCount)
	{
		uint32 Resolved = RequestedCount == 0 ? FThread::HardwareThreadCount() : RequestedCount;
		if (Resolved == 0)
		{
			Resolved = 1;
		}
		if (Resolved > 64)
		{
			Resolved = 64;
		}
		m_ExecutionThreadCount = Resolved;
		if (Resolved <= 1)
		{
			return;
		}
		m_WorkerThreadCount = Resolved - 1;
		m_pWorkers = new FThread[m_WorkerThreadCount];
		try
		{
			m_pWorkerContexts = new FWorkerContext[m_WorkerThreadCount];
		}
		catch (...)
		{
			delete[] m_pWorkers;
			m_pWorkers = nullptr;
			m_WorkerThreadCount = 0;
			throw;
		}
		uint32 Started = 0;
		for (; Started < m_WorkerThreadCount; ++Started)
		{
			m_pWorkerContexts[Started].pSystem = this;
			m_pWorkerContexts[Started].Index = Started;
			if (!m_pWorkers[Started].Start(&WorkerEntry_Internal, m_pWorkerContexts + Started))
			{
				break;
			}
		}
		if (Started != m_WorkerThreadCount)
		{
			m_QueueMutex.Lock();
			m_bAccepting = false;
			m_bStopping = true;
			m_QueueCondition.NotifyAll();
			m_QueueMutex.Unlock();
			for (uint32 Index = 0; Index < Started; ++Index)
			{
				m_pWorkers[Index].Join();
			}
			delete[] m_pWorkerContexts;
			delete[] m_pWorkers;
			m_pWorkerContexts = nullptr;
			m_pWorkers = nullptr;
			m_WorkerThreadCount = 0;
			throw FException("Failed to create JobSystem worker thread");
		}
	}
	~FImpl()
	{
		Shutdown_Internal();
		while (m_pHead != nullptr)
		{
			FJobNode* Node = PopUnlocked_Internal();
			Node->Destroy(Node->Context);
			if (Node->Fence != nullptr)
			{
				Node->Fence->Complete_Internal();
			}
			delete Node;
		}
		delete[] m_pWorkerContexts;
		delete[] m_pWorkers;
		m_pWorkerContexts = nullptr;
		m_pWorkers = nullptr;
	}
	static void WorkerEntry_Internal(void* Context)
	{
		FWorkerContext* Worker = static_cast<FWorkerContext*>(Context);
		FImpl* System = Worker->pSystem;
		s_pCurrentSystem = System;
		s_CurrentWorkerIndex = Worker->Index;
		for (;;)
		{
			FJobNode* Node = System->WaitPop_Internal();
			if (Node == nullptr)
			{
				break;
			}
			System->ExecuteNode_Internal(Node);
		}
		s_CurrentWorkerIndex = FJobSystem::InvalidWorkerIndex;
		s_pCurrentSystem = nullptr;
	}
	FJobNode* PopUnlocked_Internal() noexcept
	{
		FJobNode* Node = m_pHead;
		if (Node == nullptr)
		{
			return nullptr;
		}
		m_pHead = Node->pNext;
		if (m_pHead == nullptr)
		{
			m_pTail = nullptr;
		}
		Node->pNext = nullptr;
		return Node;
	}
	FJobNode* WaitPop_Internal() noexcept
	{
		m_QueueMutex.Lock();
		while (m_pHead == nullptr && !m_bStopping)
		{
			m_QueueCondition.Wait(m_QueueMutex);
		}
		if (m_pHead == nullptr)
		{
			m_QueueMutex.Unlock();
			return nullptr;
		}
		FJobNode* Node = PopUnlocked_Internal();
		m_QueueMutex.Unlock();
		return Node;
	}
	FJobNode* TryPop_Internal() noexcept
	{
		FScopedLock Lock(m_QueueMutex);
		return PopUnlocked_Internal();
	}
	void ExecuteNode_Internal(FJobNode* Node) noexcept
	{
		FImpl* PreviousSystem = s_pCurrentSystem;
		const uint32 PreviousWorkerIndex = s_CurrentWorkerIndex;
		FJobFence* PreviousFence = s_pCurrentFence;
		// 退避する一つ外側の実行枠。
		FExecutionFrame* PreviousFrame = s_pExecutionStack;
		// 今回実行するJobの所属とFenceを示す実行枠。
		FExecutionFrame Frame;
		Frame.pSystem = this;
		Frame.pFence = Node->Fence;
		Frame.pPrevious = PreviousFrame;
		s_pExecutionStack = &Frame;
		s_pCurrentSystem = this;
		s_pCurrentFence = Node->Fence;
		try
		{
			Node->Invoke(Node->Context);
		}
		catch (...)
		{
			m_UnhandledExceptions.FetchAdd(1);
			if (Node->Fence != nullptr)
			{
				Node->Fence->Fail_Internal();
			}
		}
		s_pExecutionStack = PreviousFrame;
		s_pCurrentFence = PreviousFence;
		s_pCurrentSystem = PreviousSystem;
		s_CurrentWorkerIndex = PreviousWorkerIndex;
		// Fence完了の通知より先に実行件数を確定し、Wait帰還後の件数観測を安定させる。
		m_CompletedJobs.FetchAdd(1);
		Node->Destroy(Node->Context);
		if (Node->Fence != nullptr)
		{
			Node->Fence->Complete_Internal();
		}
		delete Node;
	}
	bool SubmitRaw_Internal(void* Context, FInvoke Invoke, FDestroy Destroy, FJobFence* Fence)
	{
		FJobNode* Node = new FJobNode();
		Node->Context = Context;
		Node->Invoke = Invoke;
		Node->Destroy = Destroy;
		Node->Fence = Fence;
		if (m_ExecutionThreadCount == 1)
		{
			m_QueueMutex.Lock();
			if (!m_bAccepting)
			{
				m_QueueMutex.Unlock();
				delete Node;
				return false;
			}
			if (Fence != nullptr)
			{
				Fence->Add_Internal(1);
			}
			++m_ActiveInlineExecutors;
			m_SubmittedJobs.FetchAdd(1);
			m_QueueMutex.Unlock();
			ExecuteNode_Internal(Node);
			m_QueueMutex.Lock();
			--m_ActiveInlineExecutors;
			if (m_bStopping && m_ActiveInlineExecutors == 0)
			{
				m_QueueCondition.NotifyAll();
			}
			m_QueueMutex.Unlock();
			return true;
		}
		m_QueueMutex.Lock();
		if (!m_bAccepting)
		{
			m_QueueMutex.Unlock();
			delete Node;
			return false;
		}
		if (Fence != nullptr)
		{
			Fence->Add_Internal(1);
		}
		if (m_pTail != nullptr)
		{
			m_pTail->pNext = Node;
		}
		else
		{
			m_pHead = Node;
		}
		m_pTail = Node;
		m_SubmittedJobs.FetchAdd(1);
		m_QueueMutex.Unlock();
		m_QueueCondition.NotifyOne();
		return true;
	}
	void Shutdown_Internal() noexcept
	{
		m_QueueMutex.Lock();
		if (m_bShutdownComplete)
		{
			m_QueueMutex.Unlock();
			return;
		}
		if (m_bStopping)
		{
			while (!m_bShutdownComplete)
			{
				m_QueueCondition.Wait(m_QueueMutex);
			}
			m_QueueMutex.Unlock();
			return;
		}
		m_bAccepting = false;
		m_bStopping = true;
		m_QueueCondition.NotifyAll();
		while (m_ActiveInlineExecutors != 0)
		{
			m_QueueCondition.Wait(m_QueueMutex);
		}
		m_QueueMutex.Unlock();
		for (uint32 Index = 0; Index < m_WorkerThreadCount; ++Index)
		{
			m_pWorkers[Index].Join();
		}
		m_QueueMutex.Lock();
		m_bShutdownComplete = true;
		m_QueueCondition.NotifyAll();
		m_QueueMutex.Unlock();
	}
	bool Wait_Internal(FJobFence& Fence) noexcept
	{
		if (s_pCurrentSystem == this)
		{
			// 実行中Job自身または祖先Jobを含むFenceへの待機は循環するため拒否する。
			// 外側へ遡る走査対象の実行枠。
			FExecutionFrame* Frame = s_pExecutionStack;
			while (Frame != nullptr)
			{
				if (Frame->pSystem == this && Frame->pFence == &Fence)
				{
					return false;
				}
				Frame = Frame->pPrevious;
			}
		}
		if (s_pCurrentSystem != this)
		{
			// Pending=0を観測するだけでなく、最後のComplete処理がMutexを
			// 解放するまで同期してから返す。Fence破棄との競合を防ぐ。
			Fence.Wait_Internal();
			return true;
		}
		while (!Fence.IsComplete())
		{
			FJobNode* Node = TryPop_Internal();
			if (Node != nullptr)
			{
				ExecuteNode_Internal(Node);
				continue;
			}
			Fence.Wait_Internal();
		}
		// Workerが別Workerの最後のCompleteと同時に0を観測した場合も、
		// Fence内部の同期処理が終わるまで待って寿命境界を確定する。
		Fence.Wait_Internal();
		return true;
	}
};
thread_local FJobSystem::FImpl* FJobSystem::FImpl::s_pCurrentSystem = nullptr;
thread_local uint32 FJobSystem::FImpl::s_CurrentWorkerIndex = FJobSystem::InvalidWorkerIndex;
thread_local FJobFence* FJobSystem::FImpl::s_pCurrentFence = nullptr;
thread_local FJobSystem::FImpl::FExecutionFrame* FJobSystem::FImpl::s_pExecutionStack = nullptr;
FJobSystem::FJobSystem(uint32 ExecutionThreadCount) : m_pImpl(new FImpl(ExecutionThreadCount))
{
}
FJobSystem::~FJobSystem()
{
	delete m_pImpl;
	m_pImpl = nullptr;
}
void FJobSystem::Shutdown() noexcept
{
	if (m_pImpl != nullptr)
	{
		m_pImpl->Shutdown_Internal();
	}
}
bool FJobSystem::SubmitRaw_Internal(void* Context, FInvoke Invoke, FDestroy Destroy, FJobFence* Fence)
{
	return m_pImpl->SubmitRaw_Internal(Context, Invoke, Destroy, Fence);
}
bool FJobSystem::Wait(FJobFence& Fence) noexcept
{
	return m_pImpl->Wait_Internal(Fence);
}
uint32 FJobSystem::GetExecutionThreadCount() const noexcept
{
	return m_pImpl->m_ExecutionThreadCount;
}
bool FJobSystem::IsInWorkerThread() const noexcept
{
	return FImpl::s_pCurrentSystem == m_pImpl && FImpl::s_CurrentWorkerIndex != InvalidWorkerIndex;
}
uint32 FJobSystem::GetCurrentWorkerIndex() const noexcept
{
	return IsInWorkerThread() ? FImpl::s_CurrentWorkerIndex : InvalidWorkerIndex;
}
uint64 FJobSystem::GetUnhandledExceptionCount() const noexcept
{
	return m_pImpl->m_UnhandledExceptions.Load();
}
uint64 FJobSystem::GetSubmittedJobCount() const noexcept
{
	return m_pImpl->m_SubmittedJobs.Load();
}
uint64 FJobSystem::GetCompletedJobCount() const noexcept
{
	return m_pImpl->m_CompletedJobs.Load();
}
} // namespace Toolbox
