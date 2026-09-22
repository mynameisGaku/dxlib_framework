// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/SceneTaskBackend.h"
#include "Dxf/Application.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/SharedPtr.h"
#include "Toolbox/Thread.h"
#include "Toolbox/ConditionVariable.h"
#include "Toolbox/Mutex.h"
#include "Toolbox/Platform.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
// Scene外で観測し、旧版の誤った寿命順序も解放済みポインタを読まずに検出する。
struct FSceneTaskObservation
{
	FApplication* App = nullptr;
	FTaskScope EnterScope{};
	FTaskScope TickScope{};
	Toolbox::FAtomicCounter Entered;
	Toolbox::FAtomicCounter Exited;
	Toolbox::FAtomicCounter Stopped;
	Toolbox::FAtomicCounter Destroyed;
	Toolbox::FAtomicCounter Started;
	Toolbox::FAtomicCounter Prepared;
	Toolbox::FAtomicCounter Released;
	Toolbox::FAtomicCounter Committed;
	bool bScopeAliveInEnter = false;
	bool bContextConnected = false;
	bool bExitAfterPrepare = false;
	bool bStopAfterPrepare = false;
	bool bExitAfterRelease = false;
	bool bStopAfterRelease = false;
	bool bFailInitialize = false;
	bool bThrowInitialize = false;
	bool bTryPostInExit = false;
	bool bExitPostAccepted = false;
	bool bExitSawExpiredHandle = false;
	Toolbox::TFunction<void()> Initialize;
	Toolbox::TFunction<void()> Enter;
	Toolbox::TFunction<void()> Exit;
	Toolbox::TFunction<void()> Destroy;
	Toolbox::TFunction<void(const FSceneActivationContext&)> EnterContext;
	Toolbox::TFunction<void(const FTickContext&)> Tick;
	Toolbox::TFunction<void()> Draw;
};
// 旧版でも同じ回帰を実行し、追加Context入口がない場合はfalseとして観測する。
template <typename T> bool ContextIsConnected_Internal(const T& Context, FApplication& App, FTaskScope Scope)
{
	if constexpr (requires { Context.Tasks; Context.TaskScope; })
	{
		return Context.Tasks == &App.GetTaskDispatcher() && Context.TaskScope == Scope;
	}
	else
	{
		// MSVCの/WXで到達不能コード警告にならないよう、入口がない版の結果を分岐内へ置く。
		return false;
	}
}
// Contextだけから所属を指定して投入できるか調べる。
template <typename T> bool SubmitFromContext_Internal(const T& Context, FSceneTaskObservation& State)
{
	if constexpr (requires { Context.Tasks; Context.TaskScope; })
	{
		if (!Context.Tasks || !Context.TaskScope.IsValid())
		{
			return false;
		}
		FTaskRequest Request;
		Request.Scope = Context.TaskScope;
		Request.Prepare = [&State]()
		{
			State.Prepared.FetchAdd(1);
			return ETaskPrepare::Success;
		};
		Request.Commit = [&State]()
		{
			State.Committed.FetchAdd(1);
			return true;
		};
		return Context.Tasks->Submit(Toolbox::Move(Request));
	}
	else
	{
		// MSVCの/WXで到達不能コード警告にならないよう、入口がない版の結果を分岐内へ置く。
		return false;
	}
}
// 確保済みの共有捕捉が退役で解放されたかを観測する。
class FCaptureProbe
{
public:
	explicit FCaptureProbe(FSceneTaskObservation& Observation) noexcept : m_pObservation(&Observation)
	{
	}
	~FCaptureProbe()
	{
		m_pObservation->Released.FetchAdd(1);
	}
private:
	FSceneTaskObservation* m_pObservation;
};
// 任意の再入を最後の捕捉破棄時だけ実行する。
class FReleaseAction
{
public:
	explicit FReleaseAction(Toolbox::TFunction<void()> Action) : m_Action(Toolbox::Move(Action))
	{
	}
	~FReleaseAction()
	{
		m_Action();
	}
private:
	Toolbox::TFunction<void()> m_Action;
};
// 実Sceneの開始・更新・停止を観測する。noexceptフックでは例外型のREQUIREを使わない。
class DTaskObservedScene final : public DScene
{
public:
	explicit DTaskObservedScene(FSceneTaskObservation& Observation) noexcept : m_pObservation(&Observation)
	{
	}
	~DTaskObservedScene() override
	{
		m_pObservation->Destroyed.FetchAdd(1);
		if (m_pObservation->Destroy)
		{
			m_pObservation->Destroy();
		}
	}
protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		if (m_pObservation->Initialize)
		{
			m_pObservation->Initialize();
		}
		if (m_pObservation->bThrowInitialize)
		{
			throw Toolbox::FException("scene task transition preparation failed");
		}
		return m_pObservation->bFailInitialize
		           ? TResult<void>::Failure(EErrorCode::InitializationFailed, "scene task initialization failed")
		           : TResult<void>{};
	}
	void OnEnter(const FSceneActivationContext& Context) noexcept override
	{
		auto& State = *m_pObservation;
		State.Entered.FetchAdd(1);
		State.EnterScope = State.App->GetSceneScope();
		State.bScopeAliveInEnter = State.App->GetTaskDispatcher().IsScopeAlive(State.EnterScope);
		State.bContextConnected = ContextIsConnected_Internal(Context, *State.App, State.EnterScope);
		if (State.EnterContext)
		{
			State.EnterContext(Context);
		}
		if (State.Enter)
		{
			State.Enter();
		}
	}
	void OnExit() noexcept override
	{
		auto& State = *m_pObservation;
		State.bExitAfterPrepare = State.Prepared.Load() != 0;
		State.bExitAfterRelease = State.Released.Load() != 0;
		State.Exited.FetchAdd(1);
		if (State.bTryPostInExit)
		{
			const FTaskScope Scope = State.App->GetSceneScope();
			State.bExitSawExpiredHandle = Scope.IsValid() && !State.App->GetTaskDispatcher().IsScopeAlive(Scope);
			FTaskRequest Request;
			Request.Scope = Scope;
			State.bExitPostAccepted = State.App->GetTaskDispatcher().Submit(Toolbox::Move(Request));
		}
		if (State.Exit)
		{
			State.Exit();
		}
	}
	void OnDeinitialize() noexcept override
	{
		m_pObservation->bStopAfterPrepare = m_pObservation->Prepared.Load() != 0;
		m_pObservation->bStopAfterRelease = m_pObservation->Released.Load() != 0;
		m_pObservation->Stopped.FetchAdd(1);
	}
	void OnTick(const FTickContext& Context) override
	{
		m_pObservation->TickScope = m_pObservation->App->GetSceneScope();
		if (m_pObservation->Tick)
		{
			m_pObservation->Tick(Context);
		}
	}
	void OnDraw(FRenderContext&) const override
	{
		if (m_pObservation->Draw)
		{
			m_pObservation->Draw();
		}
	}
private:
	FSceneTaskObservation* m_pObservation;
};
// 観測先と窓口を結び付ける。
Toolbox::TUniquePtr<DTaskObservedScene> MakeScene_Internal(FApplication& App, FSceneTaskObservation& State)
{
	State.App = &App;
	return Toolbox::MakeUnique<DTaskObservedScene>(State);
}
// レーン数を明示し、検証機のCPU数に依存させない。
FApplicationSettings Settings_Internal(Toolbox::uint32 Lanes = 1)
{
	FApplicationSettings Settings;
	Settings.ExecutionThreadCount = Lanes;
	return Settings;
}
// 反映待ちの所属Taskを作る。捕捉解放はCommitの有無とは独立して観測する。
void SubmitReady_Internal(FApplication& App, FTaskScope Scope, FSceneTaskObservation& State)
{
	auto Capture = Toolbox::MakeShared<FCaptureProbe>(State);
	FTaskRequest Request;
	Request.Scope = Scope;
	Request.Prepare = [&State]()
	{
		State.Prepared.Store(1);
		return ETaskPrepare::Success;
	};
	Request.Commit = [Capture, &State]()
	{
		State.Committed.FetchAdd(1);
		return true;
	};
	REQUIRE(App.GetTaskDispatcher().Submit(Toolbox::Move(Request)));
}
// 取り消しまで準備を保留する。取消を確認して戻るため、任意sleepによる待機判定を使わない。
void SubmitRunning_Internal(FApplication& App, FTaskScope Scope, FSceneTaskObservation& State)
{
	auto Capture = Toolbox::MakeShared<FCaptureProbe>(State);
	FTaskRequest Request;
	Request.Scope = Scope;
	Request.Prepare = [&App, Scope, &State]()
	{
		State.Started.Store(1);
		while (!App.GetTaskDispatcher().IsCanceled(Scope))
		{
			Toolbox::FThread::Yield();
		}
		State.Prepared.Store(1);
		return ETaskPrepare::Canceled;
	};
	Request.Commit = [Capture, &State]()
	{
		State.Committed.FetchAdd(1);
		return true;
	};
	REQUIRE(App.GetTaskDispatcher().Submit(Toolbox::Move(Request)));
	while (State.Started.Load() == 0)
	{
		Toolbox::FThread::Yield();
	}
}
} // namespace
TEST("scene_task_scope_exists_in_enter_and_first_tick")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FApplication App(Backend.Services(), Settings_Internal());
	bool bTickContext = false;
	First.Tick = [&](const FTickContext& Context)
	{
		bTickContext = ContextIsConnected_Internal(Context, App, First.EnterScope);
	};
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	REQUIRE(First.bScopeAliveInEnter);
	REQUIRE(First.bContextConnected);
	REQUIRE(App.Step(0.0));
	REQUIRE(bTickContext);
	REQUIRE(First.EnterScope == First.TickScope);
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	REQUIRE(App.Step(0.1));
	REQUIRE(Second.bScopeAliveInEnter);
	REQUIRE(Second.bContextConnected);
	REQUIRE(Second.EnterScope == Second.TickScope);
	REQUIRE(!(Second.EnterScope == First.EnterScope));
}
TEST("scene_task_ready_capture_retires_before_exit_and_stop")
{
	for (const Toolbox::uint32 Lanes : {1u, 4u})
	{
		FSceneTaskBackendTrace Trace;
		FSceneTaskBackend Backend(Trace);
		FSceneTaskObservation First;
		FSceneTaskObservation Second;
		FApplication App(Backend.Services(), Settings_Internal(Lanes));
		REQUIRE(App.Start(MakeScene_Internal(App, First)));
		const FTaskScope Scope = App.GetSceneScope();
		SubmitReady_Internal(App, Scope, First);
		App.GetTaskDispatcher().WaitForPrepares();
		REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
		REQUIRE(App.Step(0.0));
		REQUIRE(First.bExitAfterRelease);
		REQUIRE(First.bStopAfterRelease);
		REQUIRE(First.Committed.Load() == 0);
		REQUIRE(First.Destroyed.Load() == 1);
		REQUIRE(!App.GetTaskDispatcher().IsScopeAlive(Scope));
	}
}
TEST("scene_task_running_child_prepares_finish_before_scene_exit")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FApplication App(Backend.Services(), Settings_Internal(4));
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	const FTaskScope Parent = App.GetSceneScope();
	const FTaskScope Child = App.GetTaskDispatcher().CreateScope(Parent);
	SubmitRunning_Internal(App, Child, First);
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	REQUIRE(App.Step(0.0));
	App.GetTaskDispatcher().WaitForPrepares();
	REQUIRE(First.bExitAfterPrepare);
	REQUIRE(First.bStopAfterPrepare);
	REQUIRE(First.bExitAfterRelease);
	REQUIRE(First.Committed.Load() == 0);
	REQUIRE(App.GetTaskDispatcher().IsCanceled(Child));
}
TEST("scene_task_failed_transition_preserves_current_scope_and_commit")
{
	for (const bool bThrow : {false, true})
	{
		FSceneTaskBackendTrace Trace;
		FSceneTaskBackend Backend(Trace);
		FSceneTaskObservation First;
		FSceneTaskObservation Failed;
		Failed.bFailInitialize = !bThrow;
		Failed.bThrowInitialize = bThrow;
		FApplication App(Backend.Services(), Settings_Internal());
		REQUIRE(App.Start(MakeScene_Internal(App, First)));
		const FTaskScope Scope = App.GetSceneScope();
		SubmitReady_Internal(App, Scope, First);
		REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Failed)));
		REQUIRE(App.Step(0.0));
		REQUIRE(App.GetSceneScope() == Scope);
		REQUIRE(App.GetTaskDispatcher().IsScopeAlive(Scope));
		REQUIRE(First.Exited.Load() == 0);
		REQUIRE(First.Committed.Load() == 1);
		REQUIRE(Failed.Entered.Load() == 0);
		REQUIRE(Failed.Stopped.Load() == 1);
		REQUIRE(Failed.Destroyed.Load() == 1);
	}
}
TEST("scene_task_exit_cannot_submit_using_expired_current_handle")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	First.bTryPostInExit = true;
	FApplication App(Backend.Services(), Settings_Internal());
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	REQUIRE(App.Step(0.0));
	REQUIRE(First.bExitSawExpiredHandle);
	REQUIRE(!First.bExitPostAccepted);
}
TEST("scene_task_switch_does_not_cancel_root_work")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FSceneTaskObservation Root;
	FApplication App(Backend.Services(), Settings_Internal());
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	SubmitReady_Internal(App, App.GetSceneScope(), First);
	SubmitReady_Internal(App, App.GetTaskDispatcher().GetRootScope(), Root);
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	REQUIRE(App.Step(0.0));
	REQUIRE(First.Committed.Load() == 0);
	REQUIRE(Root.Committed.Load() == 1);
}
TEST("scene_task_navigator_shutdown_waits_before_ending_scene")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FApplication App(Backend.Services(), Settings_Internal(4));
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	SubmitRunning_Internal(App, App.GetSceneScope(), First);
	App.GetScenes().Shutdown();
	// 旧版でもWorkerを必ず回収してから結果を判定する。
	App.Shutdown();
	REQUIRE(First.bExitAfterPrepare);
	REQUIRE(First.bStopAfterRelease);
	REQUIRE(First.Destroyed.Load() == 1);
	REQUIRE(!App.GetSceneScope().IsValid());
	REQUIRE(Trace.Shutdowns == 1);
}
TEST("scene_task_shutdown_from_tick_waits_after_callback_returns")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FApplication App(Backend.Services(), Settings_Internal(4));
	bool bDestroyedInside = false;
	First.Tick = [&](const FTickContext& Context)
	{
		Context.Scenes->Shutdown();
		bDestroyedInside = First.Destroyed.Load() != 0;
	};
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	SubmitRunning_Internal(App, App.GetSceneScope(), First);
	const auto Step = App.Step(0.0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(!bDestroyedInside);
	REQUIRE(First.bExitAfterPrepare);
	REQUIRE(First.bStopAfterRelease);
	REQUIRE(Trace.Presents == 0);
}
TEST("scene_task_commit_cannot_force_reentrant_scene_switch")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FApplication App(Backend.Services(), Settings_Internal());
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	bool bRequested = false;
	bool bCommitRejected = false;
	FTaskRequest Request;
	Request.Scope = App.GetSceneScope();
	Request.Commit = [&]()
	{
		bRequested = static_cast<bool>(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
		bCommitRejected = !App.GetScenes().Commit();
		return true;
	};
	REQUIRE(App.GetTaskDispatcher().Submit(Toolbox::Move(Request)));
	REQUIRE(App.Step(0.0));
	REQUIRE(bRequested);
	REQUIRE(bCommitRejected);
	REQUIRE(First.Destroyed.Load() == 0);
	REQUIRE(App.Step(0.1));
	REQUIRE(First.Destroyed.Load() == 1);
	REQUIRE(Second.bScopeAliveInEnter);
}
TEST("scene_task_manual_pump_shutdown_defers_application_destruction")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FApplication App(Backend.Services(), Settings_Internal());
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	bool bStillAliveInCommit = false;
	FTaskRequest Request;
	Request.Scope = App.GetSceneScope();
	Request.Commit = [&]()
	{
		App.Shutdown();
		bStillAliveInCommit = First.Destroyed.Load() == 0 && Trace.Shutdowns == 0;
		return true;
	};
	REQUIRE(App.GetTaskDispatcher().Submit(Toolbox::Move(Request)));
	App.GetTaskDispatcher().PumpCommits();
	REQUIRE(bStillAliveInCommit);
	const auto Step = App.Step(0.0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(First.Destroyed.Load() == 1);
	REQUIRE(Trace.Shutdowns == 1);
}
TEST("scene_task_capture_shutdown_is_deferred_and_skips_frame_begin")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FApplication App(Backend.Services(), Settings_Internal());
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	bool bAliveInDestructor = false;
	{
		auto Action = Toolbox::MakeShared<FReleaseAction>([&]()
		{
			App.GetScenes().Shutdown();
			bAliveInDestructor = First.Destroyed.Load() == 0;
		});
		FTaskRequest Request;
		Request.Scope = App.GetSceneScope();
		Request.Commit = [Action]()
		{
			return true;
		};
		REQUIRE(App.GetTaskDispatcher().Submit(Toolbox::Move(Request)));
	}
	const auto Step = App.Step(0.0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(bAliveInDestructor);
	REQUIRE(First.Destroyed.Load() == 1);
	REQUIRE(Trace.Clears == 0);
	REQUIRE(Trace.Presents == 0);
}
TEST("scene_task_exit_quit_does_not_enter_prepared_scene")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FApplication App(Backend.Services(), Settings_Internal());
	First.Exit = [&]()
	{
		App.GetScenes().RequestQuit();
	};
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	SubmitReady_Internal(App, App.GetSceneScope(), First);
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	const auto Step = App.Step(0.0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(First.bExitAfterRelease);
	REQUIRE(First.Stopped.Load() == 1);
	REQUIRE(Second.Entered.Load() == 0);
	REQUIRE(Second.Stopped.Load() == 1);
	REQUIRE(Second.Destroyed.Load() == 1);
}
TEST("scene_task_stopped_dispatcher_prevents_initial_activation")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FApplication App(Backend.Services(), Settings_Internal());
	First.Initialize = [&]()
	{
		App.GetTaskDispatcher().Shutdown();
	};
	REQUIRE(!App.Start(MakeScene_Internal(App, First)));
	REQUIRE(First.Entered.Load() == 0);
	REQUIRE(First.Stopped.Load() == 1);
	REQUIRE(First.Destroyed.Load() == 1);
	REQUIRE(Trace.Shutdowns == 1);
}
TEST("scene_task_direct_transitions_rotate_scope_without_application_step")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FSceneTaskObservation Third;
	FApplication App(Backend.Services(), Settings_Internal());
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	const FTaskScope FirstScope = App.GetSceneScope();
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	REQUIRE(App.GetScenes().Commit());
	const FTaskScope SecondScope = App.GetSceneScope();
	REQUIRE(SecondScope.IsValid() && !(SecondScope == FirstScope));
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Third)));
	REQUIRE(App.GetScenes().Commit());
	const FTaskScope ThirdScope = App.GetSceneScope();
	REQUIRE(ThirdScope.IsValid() && !(ThirdScope == SecondScope));
	REQUIRE(!App.GetTaskDispatcher().IsScopeAlive(FirstScope));
	REQUIRE(!App.GetTaskDispatcher().IsScopeAlive(SecondScope));
	REQUIRE(App.GetTaskDispatcher().IsScopeAlive(ThirdScope));
}

TEST("scene_task_direct_navigator_callback_defers_application_shutdown")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FApplication App(Backend.Services(), Settings_Internal(4));
	bool bAliveInside = false;
	bool bRecursiveStepRejected = false;
	First.Tick = [&](const FTickContext&)
	{
		bRecursiveStepRejected = !App.Step(0.0);
		App.Shutdown();
		bAliveInside = First.Destroyed.Load() == 0 && Trace.Shutdowns == 0;
	};
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	SubmitRunning_Internal(App, App.GetSceneScope(), First);
	FInputSnapshot Input;
	REQUIRE(App.GetScenes().Tick({}, Input));
	App.Shutdown();
	REQUIRE(bAliveInside);
	REQUIRE(bRecursiveStepRejected);
	REQUIRE(First.bStopAfterPrepare);
	REQUIRE(First.bStopAfterRelease);
	REQUIRE(First.Destroyed.Load() == 1);
	REQUIRE(Trace.Shutdowns == 1);
}
TEST("scene_task_old_destructor_quit_prevents_new_scene_enter")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FApplication App(Backend.Services(), Settings_Internal());
	bool bExpiredInside = false;
	First.Destroy = [&]()
	{
		const auto Scope = App.GetSceneScope();
		bExpiredInside = Scope.IsValid() && !App.GetTaskDispatcher().IsScopeAlive(Scope);
		App.GetScenes().RequestQuit();
	};
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	const auto Step = App.Step(0.0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(bExpiredInside);
	REQUIRE(First.Destroyed.Load() == 1);
	REQUIRE(Second.Entered.Load() == 0);
	REQUIRE(Second.Stopped.Load() == 1);
	REQUIRE(Second.Destroyed.Load() == 1);
	REQUIRE(Trace.Presents == 0);
}
TEST("scene_task_enter_and_tick_submit_through_context_on_one_and_four_lanes")
{
	for (const Toolbox::uint32 Lanes : {1u, 4u})
	{
		FSceneTaskBackendTrace Trace;
		FSceneTaskBackend Backend(Trace);
		FSceneTaskObservation First;
		FApplication App(Backend.Services(), Settings_Internal(Lanes));
		bool bEnterAccepted = false;
		bool bTickAccepted = false;
		First.EnterContext = [&](const FSceneActivationContext& Context)
		{
			bEnterAccepted = SubmitFromContext_Internal(Context, First);
		};
		First.Tick = [&](const FTickContext& Context)
		{
			if (!bTickAccepted)
			{
				bTickAccepted = SubmitFromContext_Internal(Context, First);
			}
		};
		REQUIRE(App.Start(MakeScene_Internal(App, First)));
		App.GetTaskDispatcher().WaitForPrepares();
		REQUIRE(App.Step(0.0));
		App.GetTaskDispatcher().WaitForPrepares();
		REQUIRE(App.Step(0.1));
		REQUIRE(bEnterAccepted);
		REQUIRE(bTickAccepted);
		REQUIRE(First.Prepared.Load() == 2);
		REQUIRE(First.Committed.Load() == 2);
	}
}
TEST("scene_task_draw_shutdown_finishes_worker_without_presenting")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FApplication App(Backend.Services(), Settings_Internal(4));
	bool bAliveInside = false;
	First.Draw = [&]()
	{
		App.Shutdown();
		bAliveInside = First.Destroyed.Load() == 0;
	};
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	SubmitRunning_Internal(App, App.GetSceneScope(), First);
	const auto Step = App.Step(0.0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(bAliveInside);
	REQUIRE(First.bExitAfterPrepare);
	REQUIRE(First.bStopAfterRelease);
	REQUIRE(Trace.Presents == 0);
}
namespace
{
// Prepareを外から開放するまで止めるゲート。条件変数で待ち、時間ではなく開放状態で判定する。
struct FPrepareGate
{
	Toolbox::FMutex Mutex;
	Toolbox::FConditionVariable Changed;
	bool bOpen = false;
	bool bFinished = false;
	bool bWatchdogOpened = false;
	Toolbox::FAtomicCounter Started;
	Toolbox::FAtomicCounter Returned;
	// 開放までPrepareを止める。
	void Wait() noexcept
	{
		Started.FetchAdd(1);
		Toolbox::FScopedLock Lock(Mutex);
		while (!bOpen)
		{
			Changed.Wait(Mutex);
		}
	}
	// 待機中のPrepareを全て進める。
	void Open() noexcept
	{
		Toolbox::FScopedLock Lock(Mutex);
		bOpen = true;
		Changed.NotifyAll();
	}
	// 監視スレッドへ、判定が終わったことを伝える。
	void Finish() noexcept
	{
		Toolbox::FScopedLock Lock(Mutex);
		bFinished = true;
	}
	// 監視スレッドからの開放か。
	bool WasOpenedByWatchdog() noexcept
	{
		Toolbox::FScopedLock Lock(Mutex);
		return bWatchdogOpened;
	}
};
// 待機が戻らない不具合でもテスト自身が終わるよう、上限時間後にゲートを開放する監視。
// 時間は停止防止の上限であり、成否は開放した主体（監視かテストか）で判定する。
void GateWatchdog_Internal(void* Context)
{
	auto& Gate = *static_cast<FPrepareGate*>(Context);
	const Toolbox::uint64 Deadline = Toolbox::MonotonicNanoseconds() + 5000000000ull;
	for (;;)
	{
		{
			Toolbox::FScopedLock Lock(Gate.Mutex);
			if (Gate.bFinished || Gate.bOpen)
			{
				return;
			}
			if (Toolbox::MonotonicNanoseconds() >= Deadline)
			{
				Gate.bWatchdogOpened = true;
				Gate.bOpen = true;
				Gate.Changed.NotifyAll();
				return;
			}
		}
		Toolbox::FThread::Yield();
	}
}
// ゲートで止まるPrepareと、捕捉の解放を観測する要求を投入する。
void SubmitGated_Internal(FApplication& App, FTaskScope Scope, FPrepareGate& Gate, FSceneTaskObservation& State)
{
	auto Capture = Toolbox::MakeShared<FCaptureProbe>(State);
	FTaskRequest Request;
	Request.Scope = Scope;
	Request.Prepare = [&Gate, &State]()
	{
		Gate.Wait();
		State.Prepared.FetchAdd(1);
		Gate.Returned.FetchAdd(1);
		return ETaskPrepare::Success;
	};
	Request.Commit = [Capture, &State]()
	{
		State.Committed.FetchAdd(1);
		return true;
	};
	REQUIRE(App.GetTaskDispatcher().Submit(Toolbox::Move(Request)));
}
} // namespace
TEST("scene_task_switch_completes_while_root_and_sibling_prepares_are_blocked")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FSceneTaskObservation Root;
	FSceneTaskObservation Sibling;
	FPrepareGate Gate;
	FApplication App(Backend.Services(), Settings_Internal(4));
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	const FTaskScope OldScope = App.GetSceneScope();
	// Sceneと無関係なRoot直下の仕事と、Sceneの兄弟Scopeの仕事を実行中のまま止める。
	const FTaskScope SiblingScope = App.GetTaskDispatcher().CreateScope();
	REQUIRE(SiblingScope.IsValid() && !(SiblingScope == OldScope));
	SubmitGated_Internal(App, App.GetTaskDispatcher().GetRootScope(), Gate, Root);
	SubmitGated_Internal(App, SiblingScope, Gate, Sibling);
	while (Gate.Started.Load() < 2)
	{
		Toolbox::FThread::Yield();
	}
	Toolbox::FThread Watchdog;
	REQUIRE(Watchdog.Start(&GateWatchdog_Internal, &Gate));
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	REQUIRE(App.Step(0.0));
	// 切替の完了時点で、対象外のPrepareはまだゲートで止まっている。
	const bool bSwitchedWhileBlocked = Gate.Returned.Load() == 0 && !Gate.WasOpenedByWatchdog();
	const bool bOldExited = First.Exited.Load() == 1 && First.Destroyed.Load() == 1;
	const bool bNewEntered = Second.Entered.Load() == 1;
	const bool bOldRetired = !App.GetTaskDispatcher().IsScopeAlive(OldScope);
	const bool bSiblingAlive = App.GetTaskDispatcher().IsScopeAlive(SiblingScope);
	Gate.Finish();
	Gate.Open();
	Watchdog.Join();
	App.GetTaskDispatcher().WaitForPrepares();
	REQUIRE(bSwitchedWhileBlocked);
	REQUIRE(bOldExited && bNewEntered && bOldRetired && bSiblingAlive);
	// 退役対象外の仕事は取り消されず、通常どおり反映される。
	REQUIRE(App.Step(0.0));
	REQUIRE(Root.Committed.Load() == 1);
	REQUIRE(Sibling.Committed.Load() == 1);
	REQUIRE(App.GetTaskDispatcher().RetireScope(SiblingScope));
}
TEST("scene_task_exit_waits_for_gated_scene_prepare_and_capture_release")
{
	FSceneTaskBackendTrace Trace;
	FSceneTaskBackend Backend(Trace);
	FSceneTaskObservation First;
	FSceneTaskObservation Second;
	FPrepareGate Gate;
	FApplication App(Backend.Services(), Settings_Internal(4));
	REQUIRE(App.Start(MakeScene_Internal(App, First)));
	const FTaskScope Scope = App.GetSceneScope();
	// 取消を見ないPrepareを実行中のまま止める。退役はその終了と捕捉解放を待つ必要がある。
	SubmitGated_Internal(App, Scope, Gate, First);
	while (Gate.Started.Load() < 1)
	{
		Toolbox::FThread::Yield();
	}
	// 開放直前に終了フックがまだ走っていないことを記録してから開放する。
	struct FOpener
	{
		FPrepareGate* pGate = nullptr;
		FSceneTaskObservation* pState = nullptr;
		Toolbox::FAtomicCounter Stepping;
		bool bExitedBeforeOpen = true;
	};
	FOpener Opener;
	Opener.pGate = &Gate;
	Opener.pState = &First;
	Toolbox::FThread Helper;
	REQUIRE(Helper.Start(
	    [](void* Context)
	    {
		    auto& Self = *static_cast<FOpener*>(Context);
		    while (Self.Stepping.Load() == 0)
		    {
			    Toolbox::FThread::Yield();
		    }
		    // 主スレッドが切替へ入った後、終了フックが先行しないかを短い上限の間観測する。
		    const Toolbox::uint64 Until = Toolbox::MonotonicNanoseconds() + 50000000ull;
		    while (Toolbox::MonotonicNanoseconds() < Until && Self.pState->Exited.Load() == 0)
		    {
			    Toolbox::FThread::Yield();
		    }
		    Self.bExitedBeforeOpen = Self.pState->Exited.Load() != 0 || Self.pState->Stopped.Load() != 0;
		    Self.pGate->Open();
	    },
	    &Opener));
	Toolbox::FThread Watchdog;
	REQUIRE(Watchdog.Start(&GateWatchdog_Internal, &Gate));
	REQUIRE(App.GetScenes().RequestChange(MakeScene_Internal(App, Second)));
	Opener.Stepping.Store(1);
	const bool bStepped = static_cast<bool>(App.Step(0.0));
	Gate.Finish();
	Gate.Open();
	Helper.Join();
	Watchdog.Join();
	REQUIRE(bStepped);
	REQUIRE(!Gate.WasOpenedByWatchdog());
	// 実行中Prepareと捕捉解放の完了後に、OnExit・OnDeinitialize・破棄へ進む。
	REQUIRE(!Opener.bExitedBeforeOpen);
	REQUIRE(First.bExitAfterPrepare && First.bExitAfterRelease);
	REQUIRE(First.bStopAfterPrepare && First.bStopAfterRelease);
	REQUIRE(First.Destroyed.Load() == 1);
	REQUIRE(First.Committed.Load() == 0);
	REQUIRE(Second.Entered.Load() == 1);
	REQUIRE(!App.GetTaskDispatcher().IsScopeAlive(Scope));
}
