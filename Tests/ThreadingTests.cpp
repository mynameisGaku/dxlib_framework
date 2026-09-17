// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/JobSystem.h"
#include "Toolbox/Mutex.h"
#include "Toolbox/Thread.h"
namespace
{
using namespace Toolbox;
struct FThreadCounterContext
{
	FAtomicCounter* Counter = nullptr;
	uint64* ThreadId = nullptr;
};
static void IncrementOnThread_Internal(void* Raw)
{
	FThreadCounterContext* Context = static_cast<FThreadCounterContext*>(Raw);
	*Context->ThreadId = FThread::CurrentThreadId();
	Context->Counter->FetchAdd(1);
}

static void ThrowAcrossThreadBoundary_Internal(void*)
{
	throw FException("thread boundary exception");
}
struct FMutexCounterContext
{
	FMutex* Mutex = nullptr;
	uint64* Counter = nullptr;
	uint32 Iterations = 0;
};
static void IncrementWithMutex_Internal(void* Raw)
{
	FMutexCounterContext* Context = static_cast<FMutexCounterContext*>(Raw);
	for (uint32 Index = 0; Index < Context->Iterations; ++Index)
	{
		FScopedLock Lock(*Context->Mutex);
		++*Context->Counter;
	}
}
struct FBatchProducerContext
{
	FJobSystem* Jobs = nullptr;
	FJobFence* Fence = nullptr;
	TAtomic<uint32>* Flags = nullptr;
	uint32 Begin = 0;
	uint32 End = 0;
	FAtomicCounter* Rejected = nullptr;
};
static void ProduceBatch_Internal(void* Raw)
{
	FBatchProducerContext* Context = static_cast<FBatchProducerContext*>(Raw);
	for (uint32 Index = Context->Begin; Index < Context->End; ++Index)
	{
		if (!Context->Jobs->TrySubmit([Context, Index]()
		{
			Context->Flags[Index].FetchAdd(1);
		}, Context->Fence))
		{
			Context->Rejected->FetchAdd(1);
			return;
		}
	}
}
struct FShutdownProducerContext
{
	FJobSystem* Jobs = nullptr;
	FAtomicCounter Accepted{0};
	FAtomicCounter Rejected{0};
};
static void ProduceUntilShutdown_Internal(void* Raw)
{
	FShutdownProducerContext* Context = static_cast<FShutdownProducerContext*>(Raw);
	for (uint32 Index = 0; Index < 20000; ++Index)
	{
		if (Context->Jobs->TrySubmit([]() {}))
		{
			Context->Accepted.FetchAdd(1);
		}
		else
		{
			Context->Rejected.FetchAdd(1);
			return;
		}
	}
}
struct FShutdownWaitContext
{
	FJobSystem* Jobs = nullptr;
	FAtomicCounter* Entered = nullptr;
	FAtomicCounter* Returned = nullptr;
};
static void ShutdownFromThread_Internal(void* Raw)
{
	FShutdownWaitContext* Context = static_cast<FShutdownWaitContext*>(Raw);
	Context->Entered->FetchAdd(1);
	Context->Jobs->Shutdown();
	Context->Returned->FetchAdd(1);
}
TEST("atomic compare exchange and arithmetic are consistent")
{
	TAtomic<uint64> Value(10);
	REQUIRE(Value.FetchAdd(5) == 10);
	REQUIRE(Value.Load() == 15);
	REQUIRE(Value.FetchSub(3) == 15);
	REQUIRE(Value.Load() == 12);
	uint64 Expected = 12;
	REQUIRE(Value.CompareExchange(Expected, 44));
	REQUIRE(Value.Load() == 44);
	Expected = 12;
	REQUIRE(!Value.CompareExchange(Expected, 99));
	REQUIRE(Expected == 44);
	REQUIRE(Value.Exchange(7) == 44);
	REQUIRE(Value.Load() == 7);
}
TEST("thread starts joins and exposes a different id")
{
	FAtomicCounter Counter;
	uint64 WorkerId = 0;
	FThreadCounterContext Context{&Counter, &WorkerId};
	const uint64 MainId = FThread::CurrentThreadId();
	FThread Thread;
	REQUIRE(Thread.Start(&IncrementOnThread_Internal, &Context));
	Thread.Join();
	REQUIRE(!Thread.IsJoinable());
	REQUIRE(Counter.Load() == 1);
	REQUIRE(WorkerId != 0);
	REQUIRE(WorkerId != MainId);
	REQUIRE(FThread::HardwareThreadCount() >= 1);
}
TEST("thread boundary contains an entry exception and still joins")
{
	FThread Thread;
	REQUIRE(Thread.Start(&ThrowAcrossThreadBoundary_Internal, nullptr));
	Thread.Join();
	REQUIRE(!Thread.IsJoinable());
}
TEST("worker identity distinguishes real workers from synchronous jobs")
{
	FJobSystem Inline(1);
	FJobFence InlineFence;
	FAtomicCounter InlineReportedWorker;
	REQUIRE(Inline.TrySubmit([&Inline, &InlineReportedWorker]()
	{
		if (Inline.IsInWorkerThread() || Inline.GetCurrentWorkerIndex() != FJobSystem::InvalidWorkerIndex)
		{
			InlineReportedWorker.FetchAdd(1);
		}
	}, &InlineFence));
	REQUIRE(Inline.Wait(InlineFence));
	REQUIRE(InlineReportedWorker.Load() == 0);

	FJobSystem Parallel(2);
	FJobFence WorkerFence;
	FAtomicCounter WorkerReported;
	REQUIRE(Parallel.TrySubmit([&Parallel, &WorkerReported]()
	{
		if (Parallel.IsInWorkerThread() && Parallel.GetCurrentWorkerIndex() != FJobSystem::InvalidWorkerIndex)
		{
			WorkerReported.FetchAdd(1);
		}
	}, &WorkerFence));
	REQUIRE(Parallel.Wait(WorkerFence));
	REQUIRE(WorkerReported.Load() == 1);
}
TEST("mutex protects shared writes from several threads")
{
	constexpr uint32 ThreadCount = 4;
	constexpr uint32 Iterations = 10000;
	FMutex Mutex;
	uint64 Counter = 0;
	FMutexCounterContext Context{&Mutex, &Counter, Iterations};
	FThread Threads[ThreadCount];
	for (uint32 Index = 0; Index < ThreadCount; ++Index)
	{
		REQUIRE(Threads[Index].Start(&IncrementWithMutex_Internal, &Context));
	}
	for (uint32 Index = 0; Index < ThreadCount; ++Index)
	{
		Threads[Index].Join();
	}
	REQUIRE(Counter == static_cast<uint64>(ThreadCount) * Iterations);
}
TEST("job system executes ten thousand jobs exactly once")
{
	constexpr uint32 Count = 10000;
	TAtomic<uint32>* Flags = new TAtomic<uint32>[Count];
	FJobSystem Jobs(8);
	FJobFence Fence;
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		REQUIRE(Jobs.TrySubmit([Flags, Index]()
		{
			Flags[Index].FetchAdd(1);
		}, &Fence));
	}
	REQUIRE(Jobs.Wait(Fence));
	REQUIRE(Fence.IsComplete());
	REQUIRE(Fence.FailureCount() == 0);
	REQUIRE(Jobs.GetSubmittedJobCount() == Count);
	REQUIRE(Jobs.GetCompletedJobCount() == Count);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		REQUIRE(Flags[Index].Load() == 1);
	}
	delete[] Flags;
}
TEST("multiple producers can submit concurrently without losing jobs")
{
	constexpr uint32 ProducerCount = 4;
	constexpr uint32 JobsPerProducer = 5000;
	constexpr uint32 Total = ProducerCount * JobsPerProducer;
	TAtomic<uint32>* Flags = new TAtomic<uint32>[Total];
	FJobSystem Jobs(8);
	FJobFence Fence;
	FAtomicCounter Rejected;
	FBatchProducerContext Producers[ProducerCount];
	FThread Threads[ProducerCount];
	for (uint32 Producer = 0; Producer < ProducerCount; ++Producer)
	{
		Producers[Producer].Jobs = &Jobs;
		Producers[Producer].Fence = &Fence;
		Producers[Producer].Flags = Flags;
		Producers[Producer].Begin = Producer * JobsPerProducer;
		Producers[Producer].End = Producers[Producer].Begin + JobsPerProducer;
		Producers[Producer].Rejected = &Rejected;
		REQUIRE(Threads[Producer].Start(&ProduceBatch_Internal, Producers + Producer));
	}
	for (uint32 Producer = 0; Producer < ProducerCount; ++Producer)
	{
		Threads[Producer].Join();
	}
	REQUIRE(Jobs.Wait(Fence));
	REQUIRE(Rejected.Load() == 0);
	for (uint32 Index = 0; Index < Total; ++Index)
	{
		REQUIRE(Flags[Index].Load() == 1);
	}
	delete[] Flags;
}
TEST("worker wait helps child jobs instead of deadlocking")
{
	FJobSystem Jobs(2);
	FJobFence Outer;
	FAtomicCounter ChildRan;
	REQUIRE(Jobs.TrySubmit([&Jobs, &ChildRan]()
	{
		FJobFence Child;
		if (!Jobs.TrySubmit([&ChildRan]()
		{
			ChildRan.FetchAdd(1);
		}, &Child))
		{
			throw FException("child submit rejected");
		}
		if (!Jobs.Wait(Child))
		{
			throw FException("child wait rejected");
		}
	}, &Outer));
	REQUIRE(Jobs.Wait(Outer));
	REQUIRE(Outer.FailureCount() == 0);
	REQUIRE(ChildRan.Load() == 1);
}
TEST("waiting on the fence that contains the current job is rejected")
{
	FJobSystem Jobs(2);
	FJobFence Fence;
	FAtomicCounter Rejected;
	REQUIRE(Jobs.TrySubmit([&Jobs, &Fence, &Rejected]()
	{
		if (!Jobs.Wait(Fence))
		{
			Rejected.FetchAdd(1);
		}
	}, &Fence));
	REQUIRE(Jobs.Wait(Fence));
	REQUIRE(Rejected.Load() == 1);
}
TEST("job exceptions are contained and recorded without stopping workers")
{
	FJobSystem Jobs(4);
	FJobFence Fence;
	REQUIRE(Jobs.TrySubmit([]()
	{
		throw FException("expected worker exception");
	}, &Fence));
	REQUIRE(Jobs.TrySubmit([]() {}, &Fence));
	REQUIRE(Jobs.Wait(Fence));
	REQUIRE(Fence.FailureCount() == 1);
	REQUIRE(Jobs.GetUnhandledExceptionCount() == 1);
	REQUIRE(Jobs.GetCompletedJobCount() == 2);
}
TEST("parallel for produces the same indexed result with one and many lanes")
{
	constexpr size_t Count = 4096;
	uint64* Single = new uint64[Count]{};
	uint64* Parallel = new uint64[Count]{};
	FJobSystem SingleJobs(1);
	FJobSystem ParallelJobs(8);
	REQUIRE(ParallelFor(SingleJobs, Count, [Single](size_t Index)
	{
		Single[Index] = static_cast<uint64>(Index) * static_cast<uint64>(Index + 17);
	}, 16));
	REQUIRE(ParallelFor(ParallelJobs, Count, [Parallel](size_t Index)
	{
		Parallel[Index] = static_cast<uint64>(Index) * static_cast<uint64>(Index + 17);
	}, 16));
	for (size_t Index = 0; Index < Count; ++Index)
	{
		REQUIRE(Single[Index] == Parallel[Index]);
	}
	delete[] Single;
	delete[] Parallel;
}
TEST("parallel for reports a contained job exception")
{
	FJobSystem Jobs(4);
	const bool Success = ParallelFor(Jobs, static_cast<size_t>(128), [](size_t Index)
	{
		if (Index == 57)
		{
			throw FException("parallel failure");
		}
	}, 4);
	REQUIRE(!Success);
	REQUIRE(Jobs.GetUnhandledExceptionCount() == 1);
}
TEST("parallel for contains exceptions consistently in single lane mode")
{
	FJobSystem Jobs(1);
	const bool Success = ParallelFor(Jobs, static_cast<size_t>(16), [](size_t Index)
	{
		if (Index == 3)
		{
			throw FException("single lane parallel failure");
		}
	}, 64);
	REQUIRE(!Success);
	REQUIRE(Jobs.GetUnhandledExceptionCount() == 1);
}
TEST("single lane mode runs jobs synchronously on the caller")
{
	FJobSystem Jobs(1);
	FJobFence Fence;
	const uint64 Caller = FThread::CurrentThreadId();
	uint64 Executed = 0;
	REQUIRE(Jobs.TrySubmit([&Executed]()
	{
		Executed = FThread::CurrentThreadId();
	}, &Fence));
	REQUIRE(Fence.IsComplete());
	REQUIRE(Jobs.Wait(Fence));
	REQUIRE(Executed == Caller);
	REQUIRE(Jobs.GetExecutionThreadCount() == 1);
}
TEST("shutdown races with producers without dropping accepted jobs")
{
	FJobSystem Jobs(4);
	FShutdownProducerContext Producer;
	Producer.Jobs = &Jobs;
	FThread ProducerThread;
	REQUIRE(ProducerThread.Start(&ProduceUntilShutdown_Internal, &Producer));
	Jobs.Shutdown();
	ProducerThread.Join();
	REQUIRE(!Jobs.TrySubmit([]() {}));
	REQUIRE(Jobs.GetCompletedJobCount() == Producer.Accepted.Load());
}
TEST("wait synchronizes the final fence completion before fence destruction")
{
	FJobSystem Jobs(4);
	for (uint32 Iteration = 0; Iteration < 2000; ++Iteration)
	{
		FJobFence Fence;
		REQUIRE(Jobs.TrySubmit([]() {}, &Fence));
		REQUIRE(Jobs.Wait(Fence));
		REQUIRE(Fence.IsComplete());
	}
}
TEST("concurrent shutdown callers return only after accepted work finishes")
{
	FJobSystem Jobs(4);
	FAtomicCounter Started;
	FAtomicCounter Release;
	FJobFence Fence;
	REQUIRE(Jobs.TrySubmit([&Started, &Release]()
	{
		Started.Store(1);
		while (Release.Load() == 0)
		{
			FThread::Yield();
		}
	}, &Fence));
	while (Started.Load() == 0)
	{
		FThread::Yield();
	}
	FAtomicCounter Entered;
	FAtomicCounter Returned;
	FShutdownWaitContext ContextA{&Jobs, &Entered, &Returned};
	FShutdownWaitContext ContextB{&Jobs, &Entered, &Returned};
	FThread First;
	FThread Second;
	REQUIRE(First.Start(&ShutdownFromThread_Internal, &ContextA));
	REQUIRE(Second.Start(&ShutdownFromThread_Internal, &ContextB));
	while (Entered.Load() != 2)
	{
		FThread::Yield();
	}
	for (uint32 Index = 0; Index < 1000; ++Index)
	{
		FThread::Yield();
	}
	REQUIRE(Returned.Load() == 0);
	Release.Store(1);
	First.Join();
	Second.Join();
	REQUIRE(Returned.Load() == 2);
	REQUIRE(Fence.IsComplete());
}
} // namespace
