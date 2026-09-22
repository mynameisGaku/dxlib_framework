// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/TaskDispatcher.h"
#include "Toolbox/SharedPtr.h"
using namespace Dxf;
namespace
{
// 時間の長短ではなく、許可の有無でPrepareを止める観測用ゲート。
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
// REQUIRE失敗時にも先にゲートを開いてJobの終了を許す。
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
// 最終捕捉の解放時に、Scopeとメモリの寿命境界を観測する。
class FOnRelease
{
public:
	explicit FOnRelease(Toolbox::TFunction<void()> Action) : m_Action(Toolbox::Move(Action))
	{
	}
	~FOnRelease()
	{
		m_Action();
	}
private:
	Toolbox::TFunction<void()> m_Action;
};
// 準備済みの空要求。指定しなければRootに属する。
FTaskRequest Healthy_Internal(FTaskScope Scope = {})
{
	return {{}, {}, Scope};
}
// 明示解放まで止まるRoot要求を作る。
FTaskRequest BlockedRoot_Internal(FGate& Started, FGate& Release)
{
	return {[&Started, &Release]()
	{
		Started.Open();
		Release.Wait();
		return ETaskPrepare::Success;
	}, {}, {}};
}
}
TEST("scope_empty_does_not_wait_root")
{
	FGate Started;
	FGate Release;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FOpenOnExit Rescue(Release);
	const FTaskScope Scope = Tasks.CreateScope();
	REQUIRE(Tasks.Submit(BlockedRoot_Internal(Started, Release)));
	Started.Wait();
	// Rootを解放するのは退役が戻った後だけ。全体待機ならCTestがタイムアウトする。
	REQUIRE(Tasks.RetireScope(Scope));
	REQUIRE(!Tasks.IsScopeAlive(Scope));
	REQUIRE(Tasks.IsScopeAlive(Tasks.GetRootScope()));
	REQUIRE(Tasks.PumpCommits().Stalled == 1);
	Release.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Tasks.PumpCommits().Committed == 1);
}
TEST("scope_descendants_finish_without_waiting_root")
{
	FGate RootStarted;
	FGate ReleaseRoot;
	FGate ChildStarted;
	Toolbox::FAtomicCounter Finished;
	Toolbox::FAtomicCounter Released;
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	FOpenOnExit Rescue(ReleaseRoot);
	const FTaskScope Parent = Tasks.CreateScope();
	const FTaskScope Child = Tasks.CreateScope(Parent);
	const FTaskScope Grandchild = Tasks.CreateScope(Child);
	REQUIRE(Tasks.Submit(BlockedRoot_Internal(RootStarted, ReleaseRoot)));
	RootStarted.Wait();
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Tasks.IsCanceled(Grandchild);
			Released.FetchAdd(1);
		});
		REQUIRE(Tasks.Submit({[&, Capture]()
		{
			ChildStarted.Open();
			while (!Tasks.IsCanceled(Grandchild))
			{
				Toolbox::FThread::Yield();
			}
			Finished.Store(1);
			return ETaskPrepare::Canceled;
		}, [Capture]()
		{
			return false;
		}, Grandchild}));
	}
	ChildStarted.Wait();
	REQUIRE(Tasks.RetireScope(Parent));
	REQUIRE(Finished.Load() == 1);
	REQUIRE(Released.Load() == 1);
	REQUIRE(!Tasks.IsScopeAlive(Child));
	REQUIRE(!Tasks.IsScopeAlive(Grandchild));
	REQUIRE(Tasks.PumpCommits().Stalled == 1);
	ReleaseRoot.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Tasks.PumpCommits().Committed == 1);
}
TEST("scope_retire_does_not_collect_canceled_sibling")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Selected = Tasks.CreateScope();
	const FTaskScope Sibling = Tasks.CreateScope();
	Toolbox::int32 Released = 0;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			++Released;
		});
		REQUIRE(Tasks.Submit({{}, [Capture]()
		{
			return false;
		}, Sibling}));
	}
	REQUIRE(Tasks.Submit(Healthy_Internal(Selected)));
	Tasks.Cancel(Sibling);
	REQUIRE(Tasks.RetireScope(Selected));
	REQUIRE(Released == 0);
	const FCommitSummary Summary = Tasks.PumpCommits();
	REQUIRE(Summary.Canceled == 1);
	REQUIRE(Released == 1);
}
TEST("scope_ignores_unrelated_inline_submission")
{
	FGate Started;
	FGate Release;
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Selected = Tasks.CreateScope();
	struct FSubmission
	{
		FTaskDispatcher* Tasks;
		FGate* Started;
		FGate* Release;
		bool bAccepted = false;
	};
	FSubmission Data{&Tasks, &Started, &Release};
	Toolbox::FThread Producer;
	FOpenOnExit Rescue(Release);
	REQUIRE(Producer.Start([](void* Raw)
	{
		auto& Value = *static_cast<FSubmission*>(Raw);
		Value.bAccepted = Value.Tasks->Submit(BlockedRoot_Internal(*Value.Started, *Value.Release));
	}, &Data));
	Started.Wait();
	REQUIRE(Tasks.Submit(Healthy_Internal(Selected)));
	// 別ProducerのSubmitが戻っていなくても、選択Scopeは完了できる。
	REQUIRE(Tasks.RetireScope(Selected));
	Release.Open();
	Producer.Join();
	REQUIRE(Data.bAccepted);
	Tasks.WaitForPrepares();
	REQUIRE(Tasks.PumpCommits().Committed == 1);
}
TEST("scope_destroy_pins_running_ancestry")
{
	FGate Started;
	FGate Release;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FOpenOnExit Rescue(Release);
	const FTaskScope Parent = Tasks.CreateScope();
	const FTaskScope Child = Tasks.CreateScope(Parent);
	FTaskRequest Request = BlockedRoot_Internal(Started, Release);
	Request.Scope = Child;
	REQUIRE(Tasks.Submit(Toolbox::Move(Request)));
	Started.Wait();
	Tasks.DestroyScope(Parent);
	const FTaskScope New = Tasks.CreateScope();
	REQUIRE(New.Index != Parent.Index);
	REQUIRE(New.Index != Child.Index);
	Release.Open();
	REQUIRE(Tasks.RetireScope(Parent));
	const FTaskScope Reused = Tasks.CreateScope();
	REQUIRE(Reused.Index == Parent.Index);
	REQUIRE(Reused.Generation != Parent.Generation);
	Tasks.Cancel(Child);
	REQUIRE(!Tasks.IsCanceled(New));
	REQUIRE(!Tasks.IsCanceled(Reused));
}
TEST("scope_old_generation_does_not_wait_new")
{
	FGate Started;
	FGate Release;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FOpenOnExit Rescue(Release);
	const FTaskScope Old = Tasks.CreateScope();
	REQUIRE(Tasks.RetireScope(Old));
	const FTaskScope New = Tasks.CreateScope();
	REQUIRE(New.Index == Old.Index);
	FTaskRequest Request = BlockedRoot_Internal(Started, Release);
	Request.Scope = New;
	REQUIRE(Tasks.Submit(Toolbox::Move(Request)));
	Started.Wait();
	REQUIRE(Tasks.RetireScope(Old));
	REQUIRE(!Tasks.IsCanceled(New));
	Release.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Tasks.PumpCommits().Committed == 1);
}
TEST("scope_pin_survives_capture_destructor")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	FTaskScope DuringRelease;
	bool bRetireRejected = false;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			DuringRelease = Tasks.CreateScope();
			bRetireRejected = !Tasks.RetireScope(Scope);
		});
		REQUIRE(Tasks.Submit({{}, [Capture]()
		{
			return false;
		}, Scope}));
	}
	REQUIRE(Tasks.RetireScope(Scope));
	REQUIRE(DuringRelease.IsValid());
	REQUIRE(DuringRelease.Index != Scope.Index);
	REQUIRE(bRetireRejected);
	const FTaskScope AfterRelease = Tasks.CreateScope();
	REQUIRE(AfterRelease.Index == Scope.Index);
}
TEST("scope_retirement_preserves_commit_order")
{
	for (Toolbox::uint32 Lanes : {1u, 4u})
	{
		Toolbox::FJobSystem Jobs(Lanes);
		FTaskDispatcher Tasks(Jobs);
		const FTaskScope Scope = Tasks.CreateScope();
		Toolbox::TVector<Toolbox::int32> Order;
		for (Toolbox::int32 Index = 0; Index < 6; ++Index)
		{
			const FTaskScope Owner = Index == 1 || Index == 4 ? Scope : FTaskScope{};
			REQUIRE(Tasks.Submit({{}, [&, Index]()
			{
				Order.PushBack(Index);
				return true;
			}, Owner}));
		}
		REQUIRE(Tasks.RetireScope(Scope));
		REQUIRE(Order.IsEmpty());
		Tasks.WaitForPrepares();
		REQUIRE(Tasks.PumpCommits().Committed == 4);
		REQUIRE(Order.Size() == 4);
		REQUIRE(Order[0] == 0 && Order[1] == 2 && Order[2] == 3 && Order[3] == 5);
	}
}
TEST("scope_shutdown_still_drains_root")
{
	FGate Started;
	Toolbox::FAtomicCounter Finished;
	Toolbox::FAtomicCounter Released;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Released.FetchAdd(1);
		});
		REQUIRE(Tasks.Submit({[&, Capture]()
		{
			Started.Open();
			while (!Tasks.IsCanceled(Tasks.GetRootScope()))
			{
				Toolbox::FThread::Yield();
			}
			Finished.Store(1);
			return ETaskPrepare::Canceled;
		}, [Capture]()
		{
			return false;
		}, {}}));
	}
	Started.Wait();
	Tasks.Shutdown();
	REQUIRE(Finished.Load() == 1);
	REQUIRE(Released.Load() == 1);
	REQUIRE(Tasks.PumpCommits().Stalled == 0);
	REQUIRE(!Tasks.CreateScope().IsValid());
}
TEST("scope_generation_churn_with_tasks")
{
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	for (Toolbox::int32 Index = 0; Index < 500; ++Index)
	{
		const FTaskScope Parent = Tasks.CreateScope();
		const FTaskScope Child = Tasks.CreateScope(Parent);
		REQUIRE(Tasks.Submit(Healthy_Internal(Child)));
		Tasks.DestroyScope(Parent);
		const FTaskScope Other = Tasks.CreateScope();
		REQUIRE(Tasks.Submit(Healthy_Internal(Other)));
		REQUIRE(Tasks.RetireScope(Parent));
		REQUIRE(!Tasks.IsCanceled(Other));
		Tasks.WaitForPrepares();
		REQUIRE(Tasks.PumpCommits().Committed == 1);
		REQUIRE(Tasks.RetireScope(Other));
	}
}
TEST("scope_invalid_handles_do_not_change_live_scope")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	FTaskDispatcher Other(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	FTaskScope Future = Scope;
	++Future.Generation;
	REQUIRE(!Tasks.RetireScope({}));
	REQUIRE(!Tasks.RetireScope(Tasks.GetRootScope()));
	REQUIRE(!Tasks.RetireScope(Other.CreateScope()));
	REQUIRE(!Tasks.RetireScope(Future));
	REQUIRE(Tasks.IsScopeAlive(Scope));
}
TEST("scope_target_inline_prepare_and_capture_are_finished")
{
	FGate Started;
	Toolbox::FAtomicCounter Finished;
	Toolbox::FAtomicCounter Released;
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	struct FSubmission
	{
		FTaskDispatcher* Tasks;
		FTaskScope Scope;
		FGate* Started;
		Toolbox::FAtomicCounter* Finished;
		Toolbox::FAtomicCounter* Released;
		bool bAccepted = false;
	};
	FSubmission Data{&Tasks, Scope, &Started, &Finished, &Released};
	Toolbox::FThread Producer;
	REQUIRE(Producer.Start([](void* Raw)
	{
		auto& Value = *static_cast<FSubmission*>(Raw);
		FTaskRequest Request;
		{
			auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
			{
				Value.Released->FetchAdd(1);
			});
			Request = {[&Value, Capture]()
			{
				Value.Started->Open();
				while (!Value.Tasks->IsCanceled(Value.Scope))
				{
					Toolbox::FThread::Yield();
				}
				Value.Finished->Store(1);
				return ETaskPrepare::Canceled;
			}, [Capture]()
			{
				return false;
			}, Value.Scope};
		}
		Value.bAccepted = Value.Tasks->Submit(Toolbox::Move(Request));
	}, &Data));
	Started.Wait();
	const bool bRetired = Tasks.RetireScope(Scope);
	const bool bFinished = Finished.Load() == 1 && Released.Load() == 1;
	Producer.Join();
	REQUIRE(bRetired);
	REQUIRE(bFinished);
	REQUIRE(Data.bAccepted);
}
TEST("scope_rejected_submission_keeps_pin_until_capture_release")
{
	Toolbox::FJobSystem Jobs(1);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	Jobs.Shutdown();
	FTaskScope DuringRelease;
	FTaskRequest Request;
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			Tasks.DestroyScope(Scope);
			DuringRelease = Tasks.CreateScope();
		});
		Request = {{}, [Capture]()
		{
			return false;
		}, Scope};
	}
	REQUIRE(!Tasks.Submit(Toolbox::Move(Request)));
	REQUIRE(DuringRelease.IsValid());
	REQUIRE(DuringRelease.Index != Scope.Index);
	REQUIRE(Tasks.RetireScope(Scope));
	const FTaskScope AfterRelease = Tasks.CreateScope();
	REQUIRE(AfterRelease.Index == Scope.Index);
	REQUIRE(!Tasks.IsCanceled(AfterRelease));
}
TEST("scope_concurrent_producers_retire_beside_blocked_root")
{
	FGate RootStarted;
	FGate ReleaseRoot;
	FGate Start;
	Toolbox::FAtomicCounter Attempted;
	Toolbox::FAtomicCounter Accepted;
	Toolbox::FAtomicCounter Released;
	Toolbox::FAtomicCounter Errors;
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	const FTaskScope Scope = Tasks.CreateScope();
	struct FSubmission
	{
		FTaskDispatcher* Tasks;
		FTaskScope Scope;
		FGate* Start;
		Toolbox::FAtomicCounter* Attempted;
		Toolbox::FAtomicCounter* Accepted;
		Toolbox::FAtomicCounter* Released;
		Toolbox::FAtomicCounter* Errors;
	};
	FSubmission Data{&Tasks, Scope, &Start, &Attempted, &Accepted, &Released, &Errors};
	Toolbox::FThread Producers[3];
	FOpenOnExit RescueRoot(ReleaseRoot);
	FOpenOnExit RescueStart(Start);
	REQUIRE(Tasks.Submit(BlockedRoot_Internal(RootStarted, ReleaseRoot)));
	RootStarted.Wait();
	for (Toolbox::FThread& Producer : Producers)
	{
		REQUIRE(Producer.Start([](void* Raw)
		{
			auto& Value = *static_cast<FSubmission*>(Raw);
			Value.Start->Wait();
			for (Toolbox::int32 Index = 0; Index < 120; ++Index)
			{
				try
				{
					auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
					{
						Value.Released->FetchAdd(1);
					});
					if (Value.Tasks->Submit({[Capture]()
					{
						return ETaskPrepare::Success;
					}, [Capture]()
					{
						return false;
					}, Value.Scope}))
					{
						Value.Accepted->FetchAdd(1);
					}
				}
				catch (...)
				{
					Value.Errors->FetchAdd(1);
				}
				Value.Attempted->FetchAdd(1);
			}
		}, &Data));
	}
	Start.Open();
	while (Attempted.Load() < 20)
	{
		Toolbox::FThread::Yield();
	}
	REQUIRE(Tasks.RetireScope(Scope));
	for (Toolbox::FThread& Producer : Producers)
	{
		Producer.Join();
	}
	REQUIRE(Attempted.Load() == 360);
	REQUIRE(Accepted.Load() > 0);
	REQUIRE(Released.Load() == 360);
	REQUIRE(Errors.Load() == 0);
	REQUIRE(Tasks.PumpCommits().Stalled == 1);
	ReleaseRoot.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Tasks.PumpCommits().Committed == 1);
}
TEST("scope_queued_prepare_is_canceled_without_worker_slot")
{
	FGate Started;
	FGate Release;
	Toolbox::FAtomicCounter Prepared;
	Toolbox::int32 Released = 0;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FOpenOnExit Rescue(Release);
	const FTaskScope Scope = Tasks.CreateScope();
	REQUIRE(Tasks.Submit(BlockedRoot_Internal(Started, Release)));
	Started.Wait();
	{
		auto Capture = Toolbox::MakeShared<FOnRelease>([&]()
		{
			++Released;
		});
		REQUIRE(Tasks.Submit({[&, Capture]()
		{
			Prepared.FetchAdd(1);
			return ETaskPrepare::Success;
		}, [Capture]()
		{
			return false;
		}, Scope}));
	}
	// 唯一のWorkerがRootに占有されている。ScopeのPrepareはまだ開始できない。
	REQUIRE(Tasks.RetireScope(Scope));
	REQUIRE(Released == 1);
	REQUIRE(Prepared.Load() == 0);
	Release.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Prepared.Load() == 0);
	REQUIRE(Tasks.PumpCommits().Committed == 1);
}
TEST("scope_queued_cancel_preserves_pending_bound")
{
	FGate Started;
	FGate Release;
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs, {2});
	FOpenOnExit Rescue(Release);
	const FTaskScope Scope = Tasks.CreateScope();
	REQUIRE(Tasks.Submit(BlockedRoot_Internal(Started, Release)));
	Started.Wait();
	REQUIRE(Tasks.Submit(Healthy_Internal(Scope)));
	REQUIRE(Tasks.RetireScope(Scope));
	// 空のJob通知がQueueに残る間も上限へ数え、取消の反復による無制限なQueue成長を防ぐ。
	REQUIRE(!Tasks.Submit(Healthy_Internal()));
	Release.Open();
	Tasks.WaitForPrepares();
	REQUIRE(Tasks.PumpCommits().Committed == 1);
	REQUIRE(Tasks.Submit(Healthy_Internal()));
}
