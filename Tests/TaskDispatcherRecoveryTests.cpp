// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/TaskDispatcher.h"
#include "Toolbox/SharedPtr.h"
#include "Toolbox/Testing/AllocationFault.h"
using namespace Dxf;
namespace
{
// 待機と通知を条件変数で同期するテスト用ゲート。
class FGate
{
public:
	void Open() noexcept
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		m_bOpen = true;
		m_Changed.NotifyAll();
	}
	void Wait() noexcept
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		while (!m_bOpen)
		{
			m_Changed.Wait(m_Mutex);
		}
	}
private:
	Toolbox::FMutex m_Mutex;
	Toolbox::FConditionVariable m_Changed;
	bool m_bOpen = false;
};
// 失敗したテストでも待機中のJobを解放して終了可能にする。
class FOpenOnExit
{
public:
	explicit FOpenOnExit(FGate& Gate) : m_pGate(&Gate)
	{
	}
	~FOpenOnExit()
	{
		m_pGate->Open();
	}
private:
	FGate* m_pGate;
};
// 捕捉の最終解放を観測する。照会先は観測対象より長く生存する。
class FOnRelease
{
public:
	explicit FOnRelease(Toolbox::TFunction<void()> Function) : m_Function(Toolbox::Move(Function))
	{
	}
	~FOnRelease()
	{
		m_Function();
	}
private:
	Toolbox::TFunction<void()> m_Function;
};
// 旧版も同じテストをビルドし、APIの未実装を実行時のRedとして残す。
template <typename T> bool Retire_Internal(T& Tasks, const FTaskScope& Scope)
{
	if constexpr (requires { Tasks.RetireScope(Scope); })
	{
		return Tasks.RetireScope(Scope);
	}
	else
	{
		// MSVCの/WXで到達不能コード警告にならないよう、未実装側を分岐内へ置く。
		return false;
	}
}
// 準備済みの正常要求を作る。
FTaskRequest Healthy_Internal(FTaskScope Scope = {})
{
	return {[]()
	{
		return ETaskPrepare::Success;
	}, []()
	{
		return true;
	}, Scope};
}
}
TEST("td_canceled_prepare_head")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	REQUIRE(Tasks.Submit({[]()
	{
		return ETaskPrepare::Canceled;
	}, []()
	{
		return false;
	}, {}}));
	REQUIRE(Tasks.Submit(Healthy_Internal()));
	const FCommitSummary Summary = Tasks.PumpCommits();
	REQUIRE(Summary.Canceled == 1);
	REQUIRE(Summary.Committed == 1);
	REQUIRE(Summary.Stalled == 0);
}
TEST("td_stopped_scope_creation")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	Tasks.Shutdown();
	REQUIRE(!Tasks.CreateScope().IsValid());
	REQUIRE(!Tasks.CreateScope(Tasks.GetRootScope()).IsValid());
	REQUIRE(!Tasks.Submit(Healthy_Internal()));
	Tasks.Shutdown();
}
TEST("td_canceled_scope_admission")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Parent = Tasks.CreateScope();
	const FTaskScope Child = Tasks.CreateScope(Parent);
	Tasks.Cancel(Parent);
	REQUIRE(!Tasks.Submit(Healthy_Internal(Parent)));
	REQUIRE(!Tasks.Submit(Healthy_Internal(Child)));
	REQUIRE(!Tasks.CreateScope(Parent).IsValid());
	REQUIRE(!Tasks.CreateScope(Child).IsValid());
	REQUIRE(Tasks.Submit(Healthy_Internal()));
}
TEST("td_order_and_bound_1_4")
{
	for (Toolbox::uint32 Lanes : {1u, 4u})
	{
		Toolbox::FJobSystem Jobs(Lanes);
		FTaskDispatcher Tasks(Jobs, {8});
		Toolbox::int32 Next = 0;
		for (Toolbox::int32 Index = 0; Index < 8; ++Index)
		{
			REQUIRE(Tasks.Submit({[]()
			{
				return ETaskPrepare::Success;
			}, [&Next, Index]()
			{
				const bool bOrdered = Next == Index;
				++Next;
				return bOrdered;
			}, {}}));
		}
		REQUIRE(!Tasks.Submit(Healthy_Internal()));
		Tasks.WaitForPrepares();
		const FCommitSummary Summary = Tasks.PumpCommits();
		REQUIRE(Summary.Committed == 8);
		REQUIRE(Summary.Failed == 0);
		REQUIRE(Next == 8);
		REQUIRE(Tasks.Submit(Healthy_Internal()));
	}
}
TEST("td_unready_head_nonblocking")
{
	FGate Started;
	FGate Release;
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	FOpenOnExit Rescue(Release);
	REQUIRE(Tasks.Submit({[&]()
	{
		Started.Open();
		Release.Wait();
		return ETaskPrepare::Success;
	}, []()
	{
		return true;
	}, {}}));
	Started.Wait();
	REQUIRE(Tasks.Submit(Healthy_Internal()));
	REQUIRE(Tasks.PumpCommits().Committed == 0);
	Release.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Tasks.PumpCommits().Committed == 2);
}
TEST("td_failure_order")
{
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	REQUIRE(Tasks.Submit({[]() -> ETaskPrepare
	{
		throw Toolbox::FException("prepare");
	}, []()
	{
		return true;
	}, {}}));
	REQUIRE(Tasks.Submit({[]()
	{
		return ETaskPrepare::Failed;
	}, []()
	{
		return true;
	}, {}}));
	REQUIRE(Tasks.Submit({[]()
	{
		return ETaskPrepare::Success;
	}, []() -> bool
	{
		throw Toolbox::FException("commit");
	}, {}}));
	REQUIRE(Tasks.Submit(Healthy_Internal()));
	Tasks.WaitForPrepares();
	const FCommitSummary Summary = Tasks.PumpCommits();
	REQUIRE(Summary.Failed == 3);
	REQUIRE(Summary.Committed == 1);
}
TEST("td_capture_pump_query_reentry")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	Toolbox::int32 Released = 0;
	const FTaskScope Scope = Tasks.CreateScope();
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Tasks.IsCanceled(Scope);
			++Released;
		});
		REQUIRE(Tasks.Submit({{}, [Capture]()
		{
			return true;
		}, Scope}));
	}
	Tasks.Cancel(Scope);
	REQUIRE(Tasks.PumpCommits().Canceled == 1);
	REQUIRE(Released == 1);
}
TEST("td_capture_shutdown_reentry")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	Toolbox::int32 Released = 0;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Tasks.IsCanceled(Tasks.GetRootScope());
			Tasks.Shutdown();
			++Released;
		});
		REQUIRE(Tasks.Submit({{}, [Capture]()
		{
			return true;
		}, {}}));
	}
	Tasks.Shutdown();
	REQUIRE(Released == 1);
	REQUIRE(Tasks.PumpCommits().Stalled == 0);
}
TEST("td_job_rejection_capture_reentry")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	Jobs.Shutdown();
	Toolbox::int32 Released = 0;
	FTaskRequest Request;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Tasks.IsCanceled(Tasks.GetRootScope());
			++Released;
		});
		Request.Commit = [Capture]()
		{
			return true;
		};
	}
	REQUIRE(!Tasks.Submit(Toolbox::Move(Request)));
	REQUIRE(Released == 1);
	REQUIRE(Tasks.PumpCommits().Stalled == 0);
}
TEST("td_allocation_rollback")
{
	Toolbox::int32 Injected = 0;
	for (Toolbox::int64 Failure = 0; Failure < 8; ++Failure)
	{
		Toolbox::FJobSystem Jobs(1);
		FTaskDispatcher Tasks(Jobs, {1});
		FTaskRequest Request = Healthy_Internal();
		Toolbox::Testing::SetAllocationFailureCountdown(Failure);
		bool bThrew = false;
		try
		{
			Tasks.Submit(Toolbox::Move(Request));
		}
		catch (...)
		{
			bThrew = true;
		}
		const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		if (bInjected)
		{
			++Injected;
			REQUIRE(bThrew);
			REQUIRE(Tasks.Submit(Healthy_Internal()));
		}
		Tasks.WaitForPrepares();
		REQUIRE(Tasks.PumpCommits().Committed == 1);
	}
	REQUIRE(Injected >= 3);
}
TEST("td_allocation_capture_reentry")
{
	for (Toolbox::int64 Failure = 0; Failure < 4; ++Failure)
	{
		Toolbox::FJobSystem Jobs(1);
		FTaskDispatcher Tasks(Jobs, {1});
		Toolbox::int32 Released = 0;
		FTaskRequest Request;
		{
			auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
			{
				Tasks.IsCanceled(Tasks.GetRootScope());
				++Released;
			});
			Request.Commit = [Capture]()
			{
				return true;
			};
		}
		Toolbox::Testing::SetAllocationFailureCountdown(Failure);
		try
		{
			Tasks.Submit(Toolbox::Move(Request));
		}
		catch (...)
		{
		}
		Toolbox::Testing::SetAllocationFailureCountdown(-1);
		Tasks.Shutdown();
		REQUIRE(Released == 1);
	}
}
TEST("td_prepare_capture_before_commit")
{
	FGate Begin;
	FGate Destroying;
	FGate Release;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FOpenOnExit Rescue(Release);
	Toolbox::int32 Commits = 0;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Destroying.Open();
			Release.Wait();
		});
		REQUIRE(Tasks.Submit({[Capture, &Begin]()
		{
			Begin.Wait();
			return ETaskPrepare::Success;
		}, [&]()
		{
			++Commits;
			return true;
		}, {}}));
	}
	Begin.Open();
	Destroying.Wait();
	const FCommitSummary Before = Tasks.PumpCommits();
	Release.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Before.Committed == 0);
	REQUIRE(Commits == 0);
	REQUIRE(Tasks.PumpCommits().Committed == 1);
}
TEST("td_retire_subtree_wait_and_capture")
{
	FGate Started;
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Parent = Tasks.CreateScope();
	const FTaskScope Child = Tasks.CreateScope(Parent);
	Toolbox::FAtomicCounter Finished;
	Toolbox::FAtomicCounter Released;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Tasks.IsCanceled(Child);
			Released.FetchAdd(1);
		});
		REQUIRE(Tasks.Submit({[&, Capture]()
		{
			Started.Open();
			while (!Tasks.IsCanceled(Child))
			{
				Toolbox::FThread::Yield();
			}
			Finished.Store(1);
			return ETaskPrepare::Canceled;
		}, [Capture]()
		{
			return false;
		}, Child}));
	}
	Started.Wait();
	// 旧版にAPIがない場合もJobが止まらないよう、先に協調取消を出す。
	Tasks.Cancel(Parent);
	const bool bRetired = Retire_Internal(Tasks, Parent);
	if (!bRetired)
	{
		Tasks.WaitForPrepares();
	}
	REQUIRE(bRetired);
	REQUIRE(Finished.Load() == 1);
	REQUIRE(Released.Load() == 1);
	REQUIRE(!Tasks.IsScopeAlive(Parent));
	REQUIRE(!Tasks.IsScopeAlive(Child));
	REQUIRE(Tasks.PumpCommits().Stalled == 0);
}
TEST("td_retire_preserves_live_root")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	REQUIRE(Tasks.Submit(Healthy_Internal(Scope)));
	REQUIRE(Tasks.Submit(Healthy_Internal()));
	REQUIRE(Retire_Internal(Tasks, Scope));
	const FCommitSummary Summary = Tasks.PumpCommits();
	REQUIRE(Summary.Committed == 1);
	REQUIRE(Summary.Canceled == 0);
}
TEST("td_retire_rejects_job_and_commit")
{
	for (Toolbox::uint32 Lanes : {1u, 4u})
	{
		Toolbox::FJobSystem Jobs(Lanes);
		FTaskDispatcher Tasks(Jobs);
		const FTaskScope Scope = Tasks.CreateScope();
		Toolbox::FAtomicCounter PrepareRejected;
		bool bCommitRejected = false;
		REQUIRE(Tasks.Submit({[&]()
		{
			PrepareRejected.Store(Retire_Internal(Tasks, Scope) ? 0 : 1);
			return ETaskPrepare::Success;
		}, [&]()
		{
			bCommitRejected = !Retire_Internal(Tasks, Scope);
			return true;
		}, Scope}));
		Tasks.WaitForPrepares();
		REQUIRE(Tasks.PumpCommits().Committed == 1);
		REQUIRE(PrepareRejected.Load() == 1);
		REQUIRE(bCommitRejected);
		REQUIRE(Retire_Internal(Tasks, Scope));
	}
}
TEST("td_shutdown_from_commit")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	REQUIRE(Tasks.Submit({{}, [&]()
	{
		Tasks.Shutdown();
		return true;
	}, {}}));
	REQUIRE(Tasks.Submit(Healthy_Internal()));
	const FCommitSummary Summary = Tasks.PumpCommits();
	REQUIRE(Summary.Committed == 1);
	REQUIRE(!Tasks.CreateScope().IsValid());
	REQUIRE(Tasks.PumpCommits().Stalled == 0);
}
TEST("td_reentrant_pump_rejected")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	bool bRejected = false;
	REQUIRE(Tasks.Submit({{}, [&]()
	{
		try
		{
			Tasks.PumpCommits();
		}
		catch (const Toolbox::FException&)
		{
			bRejected = true;
		}
		return true;
	}, {}}));
	REQUIRE(Tasks.Submit(Healthy_Internal()));
	REQUIRE(Tasks.PumpCommits().Committed == 2);
	REQUIRE(bRejected);
}
TEST("td_scope_generations_2000")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	FTaskScope Previous;
	for (Toolbox::int32 Iteration = 0; Iteration < 2000; ++Iteration)
	{
		const FTaskScope Parent = Tasks.CreateScope();
		const FTaskScope Child = Tasks.CreateScope(Parent);
		Tasks.Cancel(Previous);
		Tasks.DestroyScope(Previous);
		REQUIRE(!Tasks.IsCanceled(Parent));
		REQUIRE(!Tasks.IsCanceled(Child));
		Tasks.DestroyScope(Parent);
		REQUIRE(!Tasks.IsScopeAlive(Child));
		const FTaskScope Reused = Tasks.CreateScope();
		Tasks.Cancel(Child);
		REQUIRE(!Tasks.IsCanceled(Reused));
		REQUIRE(!Tasks.CreateScope(Child).IsValid());
		Tasks.DestroyScope(Reused);
		Previous = Parent;
	}
}
TEST("td_retire_stale_does_not_cancel_new")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Old = Tasks.CreateScope();
	Tasks.DestroyScope(Old);
	const FTaskScope New = Tasks.CreateScope();
	REQUIRE(Tasks.Submit(Healthy_Internal(New)));
	REQUIRE(Retire_Internal(Tasks, Old));
	REQUIRE(!Tasks.IsCanceled(New));
	REQUIRE(Tasks.PumpCommits().Committed == 1);
}
TEST("td_owner_thread_commit")
{
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	const Toolbox::uint64 Owner = Toolbox::FThread::CurrentThreadId();
	Toolbox::uint64 Committer = 0;
	REQUIRE(Tasks.Submit({{}, [&]()
	{
		Committer = Toolbox::FThread::CurrentThreadId();
		return true;
	}, {}}));
	Tasks.WaitForPrepares();
	REQUIRE(Tasks.PumpCommits().Committed == 1);
	REQUIRE(Committer == Owner);
}
TEST("td_retire_waits_prepare_destructor")
{
	FGate Begin;
	FGate Destroying;
	FGate Release;
	Toolbox::FAtomicCounter Finished;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	FOpenOnExit Rescue(Release);
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Destroying.Open();
			Release.Wait();
			Finished.Store(1);
		});
		REQUIRE(Tasks.Submit({[Capture, &Begin]()
		{
			Begin.Wait();
			return ETaskPrepare::Success;
		}, {}, Scope}));
	}
	Begin.Open();
	Destroying.Wait();
	// Retireが失効を公開してから、別スレッドが捕捉の破棄完了を許す。
	struct FReleaseContext
	{
		FTaskDispatcher* Tasks;
		FTaskScope Scope;
		FGate* Release;
		Toolbox::FAtomicCounter Abort;
	};
	FReleaseContext Context{&Tasks, Scope, &Release, Toolbox::FAtomicCounter{0}};
	Toolbox::FThread Releaser;
	REQUIRE(Releaser.Start([](void* Raw)
	{
		auto& Value = *static_cast<FReleaseContext*>(Raw);
		while (Value.Tasks->IsScopeAlive(Value.Scope) && Value.Abort.Load() == 0)
		{
			Toolbox::FThread::Yield();
		}
		Value.Release->Open();
	}, &Context));
	const bool bRetired = Retire_Internal(Tasks, Scope);
	const bool bFinishedOnReturn = Finished.Load() == 1;
	Context.Abort.Store(1);
	Releaser.Join();
	Tasks.WaitForPrepares();
	REQUIRE(bRetired);
	REQUIRE(bFinishedOnReturn);
	REQUIRE(Tasks.PumpCommits().Stalled == 0);
}
TEST("td_retire_rejects_wrong_thread_and_capture")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	struct FProbe
	{
		FTaskDispatcher* Tasks;
		FTaskScope Scope;
		bool RetireRejected = false;
		bool PumpRejected = false;
	};
	FProbe Probe{&Tasks, Scope};
	Toolbox::FThread Thread;
	REQUIRE(Thread.Start([](void* Raw)
	{
		auto& Value = *static_cast<FProbe*>(Raw);
		Value.RetireRejected = !Retire_Internal(*Value.Tasks, Value.Scope);
		try
		{
			Value.Tasks->PumpCommits();
		}
		catch (const Toolbox::FException&)
		{
			Value.PumpRejected = true;
		}
	}, &Probe));
	Thread.Join();
	REQUIRE(Probe.RetireRejected);
	REQUIRE(Probe.PumpRejected);
	bool bCaptureRejected = false;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			bCaptureRejected = !Retire_Internal(Tasks, Scope);
		});
		REQUIRE(Tasks.Submit({{}, [Capture]()
		{
			return true;
		}, Scope}));
	}
	REQUIRE(Tasks.PumpCommits().Committed == 1);
	REQUIRE(bCaptureRejected);
	REQUIRE(Retire_Internal(Tasks, Scope));
}
TEST("td_canceled_running_task_keeps_pending_bound")
{
	FGate Started;
	FGate Release;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs, {1});
	const FTaskScope Scope = Tasks.CreateScope();
	FOpenOnExit Rescue(Release);
	REQUIRE(Tasks.Submit({[&]()
	{
		Started.Open();
		Release.Wait();
		return ETaskPrepare::Success;
	}, {}, Scope}));
	Started.Wait();
	Tasks.Cancel(Scope);
	const FCommitSummary Waiting = Tasks.PumpCommits();
	const bool bRejected = !Tasks.Submit(Healthy_Internal());
	Release.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Waiting.Stalled == 1);
	REQUIRE(bRejected);
	REQUIRE(Tasks.PumpCommits().Canceled == 1);
	REQUIRE(Tasks.Submit(Healthy_Internal()));
}
TEST("td_capture_can_submit_after_pump")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	bool bAccepted = false;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			bAccepted = Tasks.Submit(Healthy_Internal());
		});
		REQUIRE(Tasks.Submit({{}, [Capture]()
		{
			return true;
		}, {}}));
	}
	REQUIRE(Tasks.PumpCommits().Committed == 2);
	REQUIRE(bAccepted);
}
TEST("td_retire_invalid_and_repeated")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	FTaskDispatcher Other(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	REQUIRE(!Retire_Internal(Tasks, {}));
	REQUIRE(!Retire_Internal(Tasks, Tasks.GetRootScope()));
	REQUIRE(!Retire_Internal(Tasks, Other.CreateScope()));
	REQUIRE(Tasks.IsScopeAlive(Scope));
	REQUIRE(Retire_Internal(Tasks, Scope));
	REQUIRE(Retire_Internal(Tasks, Scope));
	FTaskDispatcher NoCapacity(Jobs, {0});
	REQUIRE(!NoCapacity.Submit(Healthy_Internal()));
}
TEST("td_multiple_producers_race_shutdown")
{
	for (Toolbox::uint32 Lanes : {1u, 4u})
	{
		FGate Start;
		Toolbox::FAtomicCounter Attempted;
		Toolbox::FAtomicCounter Released;
		Toolbox::FAtomicCounter Errors;
		Toolbox::FJobSystem Jobs(Lanes);
		FTaskDispatcher Tasks(Jobs);
		struct FProducerContext
		{
			FTaskDispatcher* Tasks;
			FGate* Start;
			Toolbox::FAtomicCounter* Attempted;
			Toolbox::FAtomicCounter* Released;
			Toolbox::FAtomicCounter* Errors;
		};
		FProducerContext Context{&Tasks, &Start, &Attempted, &Released, &Errors};
		Toolbox::FThread Producers[4];
		// 途中のThread生成失敗でも、開始済みThreadを先に解放する。
		FOpenOnExit Rescue(Start);
		for (Toolbox::FThread& Producer : Producers)
		{
			REQUIRE(Producer.Start([](void* Raw)
			{
				auto& Value = *static_cast<FProducerContext*>(Raw);
				Value.Start->Wait();
				for (Toolbox::int32 Index = 0; Index < 200; ++Index)
				{
					try
					{
						auto Capture = Toolbox::MakeShared<FOnRelease>([&Value]()
						{
							Value.Tasks->IsCanceled(Value.Tasks->GetRootScope());
							Value.Released->FetchAdd(1);
						});
						Value.Tasks->Submit({[Capture]()
						{
							return ETaskPrepare::Success;
						},
							[Capture]()
							{
								return true;
							}, {}});
					}
					catch (...)
					{
						Value.Errors->FetchAdd(1);
					}
					Value.Attempted->FetchAdd(1);
				}
			}, &Context));
		}
		Start.Open();
		while (Attempted.Load() < 32)
		{
			Toolbox::FThread::Yield();
		}
		Tasks.Shutdown();
		for (Toolbox::FThread& Producer : Producers)
		{
			Producer.Join();
		}
		Tasks.Shutdown();
		REQUIRE(Attempted.Load() == 800);
		REQUIRE(Released.Load() == 800);
		REQUIRE(Errors.Load() == 0);
		REQUIRE(!Tasks.CreateScope().IsValid());
		REQUIRE(Tasks.PumpCommits().Stalled == 0);
	}
}
