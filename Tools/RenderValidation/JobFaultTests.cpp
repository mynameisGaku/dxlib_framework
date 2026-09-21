// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/JobSystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
// 検証実行ファイルだけの確保故障注入。本体ライブラリのABIやallocatorは変更しない。
thread_local Toolbox::int64 FailureCountdown = -1;
Toolbox::FAtomicCounter Outstanding;
Toolbox::FAtomicCounter FailureInjected;

void* AllocateTestMemory_Internal(Toolbox::size_t Size)
{
	if (FailureCountdown == 0)
	{
		FailureCountdown = -1;
		FailureInjected.Store(1);
		throw Toolbox::FException("injected allocation failure");
	}
	if (FailureCountdown > 0)
	{
		--FailureCountdown;
	}
	void* Memory = malloc(Size == 0 ? 1 : Size);
	if (Memory == nullptr)
	{
		throw Toolbox::FException("test allocator exhausted");
	}
	Outstanding.FetchAdd(1);
	return Memory;
}
void FreeTestMemory_Internal(void* Memory) noexcept
{
	if (Memory != nullptr)
	{
		Outstanding.FetchSub(1);
		free(Memory);
	}
}
}

void* operator new(Toolbox::size_t Size)
{
	return AllocateTestMemory_Internal(Size);
}
void* operator new[](Toolbox::size_t Size)
{
	return AllocateTestMemory_Internal(Size);
}
void operator delete(void* Memory) noexcept
{
	FreeTestMemory_Internal(Memory);
}
void operator delete[](void* Memory) noexcept
{
	FreeTestMemory_Internal(Memory);
}
void operator delete(void* Memory, Toolbox::size_t) noexcept
{
	FreeTestMemory_Internal(Memory);
}
void operator delete[](void* Memory, Toolbox::size_t) noexcept
{
	FreeTestMemory_Internal(Memory);
}

namespace
{
// 構築の各確保箇所で失敗させ、開始済みThreadを含む所有資源の回収を調べる。
bool ConstructionFault_Internal(Toolbox::int64 Position)
{
	const Toolbox::uint64 Before = Outstanding.Load();
	FailureCountdown = Position;
	try
	{
		Toolbox::FJobSystem Jobs(4);
	}
	catch (const Toolbox::FException&)
	{
	}
	FailureCountdown = -1;
	const Toolbox::uint64 After = Outstanding.Load();
	printf("construction fault=%lld before=%llu after=%llu\n", static_cast<long long>(Position),
	       static_cast<unsigned long long>(Before), static_cast<unsigned long long>(After));
	return Before == After;
}
struct FReleaseState
{
	Toolbox::FAtomicCounter Release;
};
void ReleaseAfterFault_Internal(void* Context)
{
	auto* State = static_cast<FReleaseState*>(Context);
	while (FailureInjected.Load() == 0)
	{
		Toolbox::FThread::Yield();
	}
	// 待機者が早期復帰する不具合の検出を助ける。正しさの判定は完了数で行う。
	for (Toolbox::size_t Iteration = 0; Iteration < 20000; ++Iteration)
	{
		Toolbox::FThread::Yield();
	}
	State->Release.Store(1);
}
bool SubmissionFault_Internal()
{
	Toolbox::FJobSystem Jobs(2);
	FReleaseState State;
	Toolbox::FThread Releaser;
	if (!Releaser.Start(&ReleaseAfterFault_Internal, &State))
	{
		return false;
	}
	bool bCaught = false;
	FailureInjected.Store(0);
	// Fenceの同期資源2回と最初のCallable/Nodeの後、次のCallableで失敗する。
	FailureCountdown = 4;
	try
	{
		Toolbox::ParallelFor(Jobs, 256, [&](Toolbox::size_t)
		{
			while (State.Release.Load() == 0)
			{
				Toolbox::FThread::Yield();
			}
		}, 1);
	}
	catch (const Toolbox::FException&)
	{
		bCaught = true;
	}
	FailureCountdown = -1;
	const Toolbox::uint64 Submitted = Jobs.GetSubmittedJobCount();
	const Toolbox::uint64 Completed = Jobs.GetCompletedJobCount();
	printf("submission caught=%d submitted=%llu completed_at_return=%llu\n", bCaught ? 1 : 0,
	       static_cast<unsigned long long>(Submitted), static_cast<unsigned long long>(Completed));
	fflush(stdout);
	Releaser.Join();
	Jobs.Shutdown();
	return bCaught && Submitted > 0 && Completed == Submitted;
}
class FWaitDuringDestruction
{
public:
	FWaitDuringDestruction(Toolbox::FJobSystem& Jobs, Toolbox::FJobFence& Fence, bool& Rejected)
	    : m_pJobs(&Jobs), m_pFence(&Fence), m_pRejected(&Rejected)
	{
	}
	FWaitDuringDestruction(const FWaitDuringDestruction&) = delete;
	FWaitDuringDestruction(FWaitDuringDestruction&& Other) noexcept
	    : m_pJobs(Toolbox::Exchange(Other.m_pJobs, nullptr)), m_pFence(Other.m_pFence),
	      m_pRejected(Other.m_pRejected)
	{
	}
	~FWaitDuringDestruction()
	{
		if (m_pJobs != nullptr)
		{
			*m_pRejected = !m_pJobs->Wait(*m_pFence);
		}
	}
	void operator()() const noexcept
	{
	}
private:
	Toolbox::FJobSystem* m_pJobs;
	Toolbox::FJobFence* m_pFence;
	bool* m_pRejected;
};
bool CaptureWait_Internal()
{
	Toolbox::FJobSystem Jobs(1);
	Toolbox::FJobFence Fence;
	bool bRejected = false;
	if (!Jobs.TrySubmit(FWaitDuringDestruction(Jobs, Fence, bRejected), &Fence))
	{
		return false;
	}
	return Jobs.Wait(Fence) && bRejected;
}
bool CrossSystemAncestor_Internal()
{
	Toolbox::FJobSystem A(1);
	Toolbox::FJobSystem B(1);
	Toolbox::FJobFence Ancestor;
	bool bRejected = false;
	const bool bAccepted = A.TrySubmit([&]()
	{
		B.TrySubmit([&]()
		{
			bRejected = !A.Wait(Ancestor);
		});
	}, &Ancestor);
	return bAccepted && A.Wait(Ancestor) && bRejected;
}
bool FenceAllocation_Internal()
{
	const Toolbox::uint64 Before = Outstanding.Load();
	FailureCountdown = 0;
	bool bCaught = false;
	try
	{
		Toolbox::FJobFence Fence;
	}
	catch (const Toolbox::FException&)
	{
		bCaught = true;
	}
	FailureCountdown = -1;
	return bCaught && Outstanding.Load() == Before;
}
}

int main(int Count, char** Arguments)
{
	if (Count < 2)
	{
		return 2;
	}
	bool bPassed = false;
	if (strcmp(Arguments[1], "construction") == 0)
	{
		bPassed = true;
		if (Count == 3)
		{
			bPassed = ConstructionFault_Internal(atoll(Arguments[2]));
		}
		else
		{
			for (Toolbox::int64 Position = 0; Position < 14; ++Position)
			{
				bPassed = ConstructionFault_Internal(Position) && bPassed;
			}
		}
	}
	else if (strcmp(Arguments[1], "submission") == 0)
	{
		bPassed = SubmissionFault_Internal();
	}
	else if (strcmp(Arguments[1], "capture-wait") == 0)
	{
		bPassed = CaptureWait_Internal();
	}
	else if (strcmp(Arguments[1], "cross-system") == 0)
	{
		bPassed = CrossSystemAncestor_Internal();
	}
	else if (strcmp(Arguments[1], "fence-allocation") == 0)
	{
		bPassed = FenceAllocation_Internal();
	}
	printf("%s %s\n", bPassed ? "PASS" : "FAIL", Arguments[1]);
	return bPassed ? 0 : 1;
}
