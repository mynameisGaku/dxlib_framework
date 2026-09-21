// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/TaskDispatcher.h"
#include "Dxf/Application.h"
#include "SandboxGame.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Thread.h"
using namespace Dxf;
using namespace Dxf::Testing;
using namespace Dxf::Sandbox;
namespace
{
// 検証用のサービス参照をまとめる。
FBackendServices TaskServices_Internal(FFakeBackend& Backend)
{
	return {Backend, Backend, Backend, Backend, Backend, Backend};
}
} // namespace
TEST("task commits run in submission order")
{
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	// 反映順の記録。
	Toolbox::TVector<Toolbox::int32> Order;
	for (Toolbox::int32 Index = 0; Index < 5; ++Index)
	{
		REQUIRE(Tasks.Submit({[]()
		                      {
			                      return ETaskPrepare::Success;
		                      },
		                      [&Order, Index]()
		                      {
			                      Order.PushBack(Index);
			                      return true;
		                      }}));
	}
	Toolbox::uint64 CommittedTotal = 0;
	Toolbox::uint64 FailedTotal = 0;
	Toolbox::uint64 CanceledTotal = 0;
	for (Toolbox::int32 Attempt = 0; Attempt < 1000 && CommittedTotal < 5; ++Attempt)
	{
		const FCommitSummary Step = Tasks.PumpCommits();
		CommittedTotal += Step.Committed;
		FailedTotal += Step.Failed;
		CanceledTotal += Step.Canceled;
		Toolbox::FThread::Yield();
	}
	REQUIRE(CommittedTotal == 5);
	REQUIRE(FailedTotal == 0);
	REQUIRE(CanceledTotal == 0);
	REQUIRE(Order.Size() == 5);
	for (Toolbox::size_t Index = 0; Index < Order.Size(); ++Index)
	{
		REQUIRE(Order[Index] == static_cast<Toolbox::int32>(Index));
	}
}
TEST("unready head blocks later commits until released")
{
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	// 先頭の準備を止める旗。
	Toolbox::FAtomicCounter Release;
	// 反映順の記録。
	Toolbox::TVector<Toolbox::int32> Order;
	REQUIRE(Tasks.Submit({[&Release]()
	                      {
		                      while (Release.Load() == 0)
		                      {
			                      Toolbox::FThread::Yield();
		                      }
		                      return ETaskPrepare::Success;
	                      },
	                      [&Order]()
	                      {
		                      Order.PushBack(0);
		                      return true;
	                      }}));
	for (Toolbox::int32 Index = 1; Index < 3; ++Index)
	{
		REQUIRE(Tasks.Submit({[]()
		                      {
			                      return ETaskPrepare::Success;
		                      },
		                      [&Order, Index]()
		                      {
			                      Order.PushBack(Index);
			                      return true;
		                      }}));
	}
	FCommitSummary Blocked = Tasks.PumpCommits();
	REQUIRE(Blocked.Committed == 0);
	REQUIRE(Order.IsEmpty());
	Release.Store(1);
	Toolbox::uint64 DoneTotal = 0;
	for (Toolbox::int32 Attempt = 0; Attempt < 1000 && DoneTotal < 3; ++Attempt)
	{
		DoneTotal += Tasks.PumpCommits().Committed;
		Toolbox::FThread::Yield();
	}
	REQUIRE(DoneTotal == 3);
	REQUIRE(Order.Size() == 3);
	REQUIRE(Order[0] == 0 && Order[1] == 1 && Order[2] == 2);
}
TEST("canceling a parent suppresses its subtree but not the root")
{
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	FTaskScope Parent = Tasks.CreateScope();
	FTaskScope Child = Tasks.CreateScope(Parent);
	REQUIRE(Parent.IsValid() && Child.IsValid());
	// 反映した所属の記録。
	Toolbox::TVector<Toolbox::int32> Committed;
	auto MakeRequest = [&](FTaskScope Scope, Toolbox::int32 Tag)
	{
		FTaskRequest Request;
		Request.Scope = Scope;
		Request.Prepare = Toolbox::TFunction<ETaskPrepare()>([]()
		{
			return ETaskPrepare::Success;
		});
		Request.Commit = Toolbox::TFunction<bool()>([&Committed, Tag]()
		{
			Committed.PushBack(Tag);
			return true;
		});
		return Request;
	};
	REQUIRE(Tasks.Submit(MakeRequest(Parent, 1)));
	REQUIRE(Tasks.Submit(MakeRequest(Child, 2)));
	REQUIRE(Tasks.Submit(MakeRequest(FTaskScope{}, 3)));
	Tasks.Cancel(Parent);
	Toolbox::uint64 CommittedTotal = 0;
	Toolbox::uint64 CanceledTotal = 0;
	for (Toolbox::int32 Attempt = 0; Attempt < 1000 && CommittedTotal + CanceledTotal < 3; ++Attempt)
	{
		const FCommitSummary Step = Tasks.PumpCommits();
		CommittedTotal += Step.Committed;
		CanceledTotal += Step.Canceled;
		Toolbox::FThread::Yield();
	}
	REQUIRE(CommittedTotal == 1);
	REQUIRE(CanceledTotal == 2);
	REQUIRE(Committed.Size() == 1 && Committed[0] == 3);
}
TEST("cancel during preparation skips the commit")
{
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FTaskScope Scope = Tasks.CreateScope();
	// 準備の開始を知らせる旗。
	Toolbox::FAtomicCounter Started;
	// 準備の続行を許す旗。
	Toolbox::FAtomicCounter Release;
	// 反映したか。
	Toolbox::FAtomicCounter Committed;
	REQUIRE(Tasks.Submit({[&Started, &Release]()
	                      {
		                      Started.FetchAdd(1);
		                      while (Release.Load() == 0)
		                      {
			                      Toolbox::FThread::Yield();
		                      }
		                      return ETaskPrepare::Success;
	                      },
	                      [&Committed]()
	                      {
		                      Committed.FetchAdd(1);
		                      return true;
	                      },
	                      Scope}));
	while (Started.Load() == 0)
	{
		Toolbox::FThread::Yield();
	}
	Tasks.Cancel(Scope);
	Release.Store(1);
	Toolbox::uint64 CanceledTotal = 0;
	Toolbox::uint64 CommittedTotal = 0;
	for (Toolbox::int32 Attempt = 0; Attempt < 1000 && CanceledTotal + CommittedTotal < 1; ++Attempt)
	{
		const FCommitSummary Step = Tasks.PumpCommits();
		CanceledTotal += Step.Canceled;
		CommittedTotal += Step.Committed;
		Toolbox::FThread::Yield();
	}
	REQUIRE(CanceledTotal == 1);
	REQUIRE(CommittedTotal == 0);
	REQUIRE(Committed.Load() == 0);
}
TEST("prepare and commit failures never run later commits out of order")
{
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	// 反映順の記録。
	Toolbox::TVector<Toolbox::int32> Order;
	REQUIRE(Tasks.Submit({[]() -> ETaskPrepare
	                      {
		                      throw Toolbox::FException("prepare failure");
	                      },
	                      [&Order]()
	                      {
		                      Order.PushBack(-1);
		                      return true;
	                      }}));
	REQUIRE(Tasks.Submit({[]()
	                      {
		                      return ETaskPrepare::Success;
	                      },
	                      []()
	                      {
		                      return false;
	                      }}));
	REQUIRE(Tasks.Submit({[]()
	                      {
		                      return ETaskPrepare::Success;
	                      },
	                      []() -> bool
	                      {
		                      throw Toolbox::FException("commit failure");
	                      }}));
	REQUIRE(Tasks.Submit({[]()
	                      {
		                      return ETaskPrepare::Success;
	                      },
	                      [&Order]()
	                      {
		                      Order.PushBack(3);
		                      return true;
	                      }}));
	Toolbox::uint64 CommittedTotal = 0;
	Toolbox::uint64 FailedTotal = 0;
	for (Toolbox::int32 Attempt = 0; Attempt < 1000 && CommittedTotal + FailedTotal < 4; ++Attempt)
	{
		const FCommitSummary Step = Tasks.PumpCommits();
		CommittedTotal += Step.Committed;
		FailedTotal += Step.Failed;
		Toolbox::FThread::Yield();
	}
	REQUIRE(CommittedTotal == 1);
	REQUIRE(FailedTotal == 3);
	REQUIRE(Order.Size() == 1 && Order[0] == 3);
}
TEST("pending bound rejects overflow and recovers after pump")
{
	Toolbox::FJobSystem Jobs(2);
	FTaskSettings Settings;
	Settings.MaxPending = 2;
	FTaskDispatcher Tasks(Jobs, Settings);
	auto MakeHealthy = []()
	{
		FTaskRequest Request;
		Request.Prepare = Toolbox::TFunction<ETaskPrepare()>([]()
		{
			return ETaskPrepare::Success;
		});
		Request.Commit = Toolbox::TFunction<bool()>([]()
		{
			return true;
		});
		return Request;
	};
	REQUIRE(Tasks.Submit(MakeHealthy()));
	REQUIRE(Tasks.Submit(MakeHealthy()));
	REQUIRE(!Tasks.Submit(MakeHealthy()));
	Toolbox::uint64 CommittedTotal = 0;
	for (Toolbox::int32 Attempt = 0; Attempt < 1000 && CommittedTotal < 2; ++Attempt)
	{
		CommittedTotal += Tasks.PumpCommits().Committed;
		Toolbox::FThread::Yield();
	}
	REQUIRE(CommittedTotal == 2);
	REQUIRE(Tasks.Submit(MakeHealthy()));
}
TEST("destroyed scopes reject submits and old handles stay dead")
{
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	FTaskScope Scope = Tasks.CreateScope();
	REQUIRE(Tasks.IsScopeAlive(Scope));
	Tasks.DestroyScope(Scope);
	REQUIRE(!Tasks.IsScopeAlive(Scope));
	FTaskRequest Request;
	Request.Scope = Scope;
	Request.Prepare = Toolbox::TFunction<ETaskPrepare()>([]()
	{
		return ETaskPrepare::Success;
	});
	Request.Commit = Toolbox::TFunction<bool()>([]()
	{
		return true;
	});
	REQUIRE(!Tasks.Submit(Toolbox::Move(Request)));
	FTaskScope Reused = Tasks.CreateScope();
	REQUIRE(Tasks.IsScopeAlive(Reused));
	REQUIRE(!Tasks.IsScopeAlive(Scope));
	Tasks.DestroyScope(Scope);
	Tasks.DestroyScope(Reused);
	REQUIRE(!Tasks.IsScopeAlive(Reused));
}
TEST("commits run on the pumping thread")
{
	Toolbox::FJobSystem Jobs(4);
	FTaskDispatcher Tasks(Jobs);
	// 呼び出し側スレッドの識別子。
	const Toolbox::uint64 Caller = Toolbox::FThread::CurrentThreadId();
	// 反映を実行したスレッドの識別子。
	Toolbox::uint64 Committer = 0;
	REQUIRE(Tasks.Submit({[]()
	                      {
		                      return ETaskPrepare::Success;
	                      },
	                      [&Committer]()
	                      {
		                      Committer = Toolbox::FThread::CurrentThreadId();
		                      return true;
	                      }}));
	Toolbox::uint64 CommittedTotal = 0;
	for (Toolbox::int32 Attempt = 0; Attempt < 1000 && CommittedTotal < 1; ++Attempt)
	{
		CommittedTotal += Tasks.PumpCommits().Committed;
		Toolbox::FThread::Yield();
	}
	REQUIRE(CommittedTotal == 1);
	REQUIRE(Committer == Caller);
}
TEST("shutdown discards without committing")
{
	Toolbox::FJobSystem Jobs(2);
	FTaskDispatcher Tasks(Jobs);
	// 反映したか。
	Toolbox::FAtomicCounter Committed;
	REQUIRE(Tasks.Submit({[]()
	                      {
		                      return ETaskPrepare::Success;
	                      },
	                      [&Committed]()
	                      {
		                      Committed.FetchAdd(1);
		                      return true;
	                      }}));
	Tasks.Shutdown();
	FCommitSummary Summary = Tasks.PumpCommits();
	REQUIRE(Summary.Committed == 0);
	REQUIRE(Committed.Load() == 0);
	REQUIRE(!Tasks.Submit({[]()
	                       {
		                       return ETaskPrepare::Success;
	                       },
	                       []()
	                       {
		                       return true;
	                       }}));
	Tasks.Shutdown();
}
TEST("application pumps scene tasks and rotates scopes on switch")
{
	// 検証用のバックエンド。
	FFakeBackend Backend;
	// 検証するアプリケーション。
	FApplication App(TaskServices_Internal(Backend), {}, Toolbox::MakeUnique<DSandboxGameInstance>());
	REQUIRE(App.Start(Toolbox::MakeUnique<DSandboxScene>("Assets")));
	// 開始SceneのScope。
	const FTaskScope FirstScope = App.GetSceneScope();
	REQUIRE(FirstScope.IsValid());
	REQUIRE(App.GetTaskDispatcher().IsScopeAlive(FirstScope));
	// Sceneへ付けた要求の反映記録。
	Toolbox::FAtomicCounter Committed;
	FTaskRequest Request;
	Request.Scope = FirstScope;
	Request.Prepare = Toolbox::TFunction<ETaskPrepare()>([]()
	{
		return ETaskPrepare::Success;
	});
	Request.Commit = Toolbox::TFunction<bool()>([&Committed]()
	{
		Committed.FetchAdd(1);
		return true;
	});
	REQUIRE(App.GetTaskDispatcher().Submit(Toolbox::Move(Request)));
	// 単調に進むフレーム時刻。
	Toolbox::f64 Now = 0;
	for (Toolbox::int32 Attempt = 0; Attempt < 100 && Committed.Load() < 1; ++Attempt)
	{
		Now += 0.001;
		REQUIRE(App.Step(Now));
	}
	REQUIRE(Committed.Load() == 1);
	// Sceneを切り替える入力。
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Enter)] = true;
	Now += 0.016;
	REQUIRE(App.Step(Now));
	Backend.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Enter)] = false;
	Now += 0.016;
	REQUIRE(App.Step(Now));
	// 切り替え後のScope。
	const FTaskScope SecondScope = App.GetSceneScope();
	REQUIRE(SecondScope.IsValid());
	REQUIRE(!(SecondScope == FirstScope));
	REQUIRE(!App.GetTaskDispatcher().IsScopeAlive(FirstScope));
	REQUIRE(App.GetTaskDispatcher().IsScopeAlive(SecondScope));
	FTaskRequest SecondRequest;
	SecondRequest.Scope = SecondScope;
	SecondRequest.Prepare = Toolbox::TFunction<ETaskPrepare()>([]()
	{
		return ETaskPrepare::Success;
	});
	SecondRequest.Commit = Toolbox::TFunction<bool()>([&Committed]()
	{
		Committed.FetchAdd(1);
		return true;
	});
	REQUIRE(App.GetTaskDispatcher().Submit(Toolbox::Move(SecondRequest)));
	for (Toolbox::int32 Attempt = 0; Attempt < 100 && Committed.Load() < 2; ++Attempt)
	{
		Now += 0.001;
		REQUIRE(App.Step(Now));
	}
	REQUIRE(Committed.Load() == 2);
}
