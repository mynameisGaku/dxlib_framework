// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/ViewCoordinates.h"
#include "Support/FakeBackend.h"
#include "Dxf/Application.h"
#include "Dxf/PhysicsScene3D.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Thread.h"

namespace
{
using namespace Dxf;
using namespace Toolbox;

// Application経由で実行された描画とJobの観察値。各テストが所有する。
struct FApplicationRenderTrace
{
	TVector<FString> Events;
	FAtomicCounter Generated;
	FAtomicCounter GeneratedOutsideJob;
	FAtomicCounter ForeignBackendCalls;
	bool FailGeometry = false;
	bool ThrowGeometry = false;
	bool ThrowEnd = false;
	uint32 Deinitialized = 0;
};

// 描画境界だけを記録する。Application、Scene、Renderer、JobSystemは実装本体を使う。
class FApplicationRecordingBackend final : public IRenderBackend
{
public:
	explicit FApplicationRecordingBackend(FApplicationRenderTrace& Trace)
		: m_Trace(Trace), m_OwnerThread(FThread::CurrentThreadId())
	{
	}

	TResult<void> SetTarget(int32, int32, int32) override
	{
		return Record_Internal("target");
	}

	TResult<void> Clear(FColor) override
	{
		return Record_Internal("clear");
	}

	TResult<void> ResetState(int32, int32) override
	{
		return Record_Internal("reset2d");
	}

	TResult<void> DrawSprite(const FSpriteCommand&) override
	{
		return Record_Internal("sprite2d");
	}

	TResult<void> DrawText(const FTextCommand&) override
	{
		return Record_Internal("text2d");
	}

	TResult<void> DrawRectangle(const FRectangleCommand&) override
	{
		return Record_Internal("rectangle2d");
	}

	bool SupportsViewports3D() const noexcept override
	{
		return true;
	}
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}

	TResult<void> BeginView3D(const FRenderView3D&) override
	{
		return Record_Internal("begin3d");
	}

	TResult<void> DrawGeometry3D(const FPreparedGeometry3D&) override
	{
		auto Recorded = Record_Internal("geometry3d");
		if (!Recorded)
		{
			return Recorded;
		}
		if (m_Trace.ThrowGeometry)
		{
			throw FException("expected geometry exception");
		}
		if (m_Trace.FailGeometry)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "expected geometry failure");
		}
		return {};
	}

	TResult<void> EndView3D() override
	{
		auto Recorded = Record_Internal("end3d");
		if (!Recorded)
		{
			return Recorded;
		}
		if (m_Trace.ThrowEnd)
		{
			throw FException("expected cleanup exception");
		}
		return {};
	}

	TResult<void> Present() override
	{
		return Record_Internal("present");
	}

private:
	TResult<void> Record_Internal(const char* Event)
	{
		// 不正スレッドでは履歴配列へ書かず、原子的な失敗記録だけを残す。
		if (FThread::CurrentThreadId() != m_OwnerThread || FJobSystem::IsExecutingJob())
		{
			m_Trace.ForeignBackendCalls.FetchAdd(1);
			return TResult<void>::Failure(EErrorCode::InvalidState, "backend called from a job or foreign thread");
		}
		m_Trace.Events.PushBack(Event);
		return {};
	}

	FApplicationRenderTrace& m_Trace;
	uint64 m_OwnerThread;
};

// Sceneがそのフレームで要求する描画。Sceneより長く生存する観察先だけを借用する。
struct FApplicationScenePlan
{
	FApplicationRenderTrace* Trace = nullptr;
	FSceneNavigator* Scenes = nullptr;
	bool Geometry = true;
	bool Rectangles = true;
	bool Generate = false;
	bool FailGeneration = false;
	bool QuitAfterDraw = false;
};

void RequireRender_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw FException(Result.Error().Message);
	}
}

// 通常のOnDrawから描画する実Scene。Applicationへの手動Flushは行わない。
class AApplicationRenderScene final : public DScene
{
public:
	explicit AApplicationRenderScene(FApplicationScenePlan Plan) : m_Plan(Plan)
	{
	}

protected:
	void OnDraw(FRenderContext& Render) const override
	{
		if (m_Plan.Geometry)
		{
			FRenderView3D View;
			View.Id = 1;
			View.Eye = {3, 2, -5};
			View.Target = {0, 0, 0};
			RequireRender_Internal(Render.Get3D().SetView(View));
			RequireRender_Internal(Render.Get3D().DrawLine({0, 0, 0}, {1, 0, 0}));
		}
		if (m_Plan.Generate)
		{
			// 共有Contextは捕捉せず、独立した命令と原子的な観察値だけを書く。
			FApplicationRenderTrace* Trace = m_Plan.Trace;
			const bool Fail = m_Plan.FailGeneration;
			RequireRender_Internal(Render.Get2D().SubmitGenerated(32,
				[Trace, Fail](size_t Index, FRenderCommand& Output) -> TResult<void>
				{
					Trace->Generated.FetchAdd(1);
					if (!FJobSystem::IsExecutingJob())
					{
						Trace->GeneratedOutsideJob.FetchAdd(1);
					}
					if (Fail && Index == 17)
					{
						return TResult<void>::Failure(EErrorCode::InvalidArgument, "expected generation failure");
					}
					const int32 X = static_cast<int32>(Index);
					FRectangleCommand Command;
					Command.Rectangle = {X, 0, X + 1, 1};
					Output = Move(Command);
					return {};
				}, 1));
		}
		else if (m_Plan.Rectangles)
		{
			RequireRender_Internal(Render.Get2D().FillRectangle({0, 0, 10, 10}));
		}
		if (m_Plan.QuitAfterDraw && m_Plan.Scenes != nullptr)
		{
			m_Plan.Scenes->RequestQuit();
		}
	}

	void OnDeinitialize() noexcept override
	{
		if (m_Plan.Trace != nullptr)
		{
			++m_Plan.Trace->Deinitialized;
		}
	}

private:
	FApplicationScenePlan m_Plan;
};

FApplicationSettings MakeSettings_Internal(uint32 Lanes = 1)
{
	FApplicationSettings Settings;
	Settings.ExecutionThreadCount = Lanes;
	Settings.Window.Width = 320;
	Settings.Window.Height = 240;
	return Settings;
}

size_t CountEvent_Internal(const FApplicationRenderTrace& Trace, const char* Name)
{
	size_t Count = 0;
	for (const auto& Event : Trace.Events)
	{
		if (Event == Name)
		{
			++Count;
		}
	}
	return Count;
}

size_t FindEvent_Internal(const FApplicationRenderTrace& Trace, const char* Name, size_t Begin = 0)
{
	for (size_t Index = Begin; Index < Trace.Events.Size(); ++Index)
	{
		if (Trace.Events[Index] == Name)
		{
			return Index;
		}
	}
	return TNumericLimits<size_t>::Max();
}

TEST("application executes 3D then restores 2D then presents")
{
	Testing::FFakeBackend Services;
	FApplicationRenderTrace Trace;
	FApplicationRecordingBackend Renderer(Trace);
	FApplication App({Services, Services, Services, Services, Services, Renderer}, MakeSettings_Internal());
	REQUIRE(App.Start(MakeUnique<AApplicationRenderScene>(FApplicationScenePlan{&Trace})));
	auto Step = App.Step(0.0);
	REQUIRE(Step && Step.Value());
	const size_t Geometry = FindEvent_Internal(Trace, "geometry3d");
	const size_t Restore = FindEvent_Internal(Trace, "reset2d", Geometry == TNumericLimits<size_t>::Max() ? 0 : Geometry);
	const size_t Rectangle = FindEvent_Internal(Trace, "rectangle2d");
	const size_t Present = FindEvent_Internal(Trace, "present");
	REQUIRE(Geometry < Restore);
	REQUIRE(Restore < Rectangle);
	REQUIRE(Rectangle < Present);
	REQUIRE(CountEvent_Internal(Trace, "geometry3d") == 1);
	REQUIRE(CountEvent_Internal(Trace, "present") == 1);
	REQUIRE(Trace.ForeignBackendCalls.Load() == 0);
}

void CheckSharedJobs_Internal(uint32 Lanes)
{
	Testing::FFakeBackend Services;
	FApplicationRenderTrace Trace;
	FApplicationRecordingBackend Renderer(Trace);
	FApplication App({Services, Services, Services, Services, Services, Renderer}, MakeSettings_Internal(Lanes));
	FApplicationScenePlan Plan;
	Plan.Trace = &Trace;
	Plan.Generate = true;
	REQUIRE(App.Start(MakeUnique<AApplicationRenderScene>(Plan)));
	const uint64 Before = App.GetExecutionJobs().GetSubmittedJobCount();
	auto Step = App.Step(0.0);
	REQUIRE(Step && Step.Value());
	REQUIRE(Trace.Generated.Load() == 32);
	REQUIRE(Trace.GeneratedOutsideJob.Load() == 0);
	REQUIRE(App.GetExecutionJobs().GetSubmittedJobCount() > Before);
	REQUIRE(App.GetExecutionJobs().GetCompletedJobCount() == App.GetExecutionJobs().GetSubmittedJobCount());
	REQUIRE(CountEvent_Internal(Trace, "rectangle2d") == 32);
	REQUIRE(CountEvent_Internal(Trace, "geometry3d") == 1);
	REQUIRE(Trace.ForeignBackendCalls.Load() == 0);
}

TEST("application automatically shares its single execution lane with rendering")
{
	CheckSharedJobs_Internal(1);
}

TEST("application automatically shares its worker pool with rendering")
{
	CheckSharedJobs_Internal(4);
}

TEST("application does not present after a 3D backend failure")
{
	Testing::FFakeBackend Services;
	FApplicationRenderTrace Trace;
	Trace.FailGeometry = true;
	FApplicationRecordingBackend Renderer(Trace);
	FApplication App({Services, Services, Services, Services, Services, Renderer}, MakeSettings_Internal());
	REQUIRE(App.Start(MakeUnique<AApplicationRenderScene>(FApplicationScenePlan{&Trace})));
	auto Step = App.Step(0.0);
	REQUIRE(!Step);
	REQUIRE(Step.Error().Message == "expected geometry failure");
	REQUIRE(CountEvent_Internal(Trace, "end3d") == 1);
	REQUIRE(CountEvent_Internal(Trace, "rectangle2d") == 0);
	REQUIRE(CountEvent_Internal(Trace, "present") == 0);
	REQUIRE(!App.IsRunning());
	REQUIRE(Trace.Deinitialized == 1);
}

TEST("application preserves the first 3D error when cleanup throws")
{
	Testing::FFakeBackend Services;
	FApplicationRenderTrace Trace;
	Trace.FailGeometry = true;
	Trace.ThrowEnd = true;
	FApplicationRecordingBackend Renderer(Trace);
	FApplication App({Services, Services, Services, Services, Services, Renderer}, MakeSettings_Internal());
	REQUIRE(App.Start(MakeUnique<AApplicationRenderScene>(FApplicationScenePlan{&Trace})));
	auto Step = App.Step(0.0);
	REQUIRE(!Step);
	REQUIRE(Step.Error().Message == "expected geometry failure");
	REQUIRE(CountEvent_Internal(Trace, "present") == 0);
	REQUIRE(FindEvent_Internal(Trace, "reset2d", FindEvent_Internal(Trace, "end3d")) < Trace.Events.Size());
	REQUIRE(Trace.Deinitialized == 1);
}

TEST("application discards both dimensions when a scene requests quit during drawing")
{
	Testing::FFakeBackend Services;
	FApplicationRenderTrace Trace;
	FApplicationRecordingBackend Renderer(Trace);
	FApplication App({Services, Services, Services, Services, Services, Renderer}, MakeSettings_Internal());
	FApplicationScenePlan Plan;
	Plan.Trace = &Trace;
	Plan.Scenes = &App.GetScenes();
	Plan.QuitAfterDraw = true;
	REQUIRE(App.Start(MakeUnique<AApplicationRenderScene>(Plan)));
	auto Step = App.Step(0.0);
	REQUIRE(Step && !Step.Value());
	REQUIRE(CountEvent_Internal(Trace, "geometry3d") == 0);
	REQUIRE(CountEvent_Internal(Trace, "rectangle2d") == 0);
	REQUIRE(CountEvent_Internal(Trace, "present") == 0);
	REQUIRE(Trace.Deinitialized == 1);
}

TEST("application scene replacement does not replay the old 3D commands")
{
	Testing::FFakeBackend Services;
	FApplicationRenderTrace Trace;
	FApplicationRecordingBackend Renderer(Trace);
	FApplication App({Services, Services, Services, Services, Services, Renderer}, MakeSettings_Internal());
	REQUIRE(App.Start(MakeUnique<AApplicationRenderScene>(FApplicationScenePlan{&Trace})));
	REQUIRE(App.Step(0.0));
	REQUIRE(CountEvent_Internal(Trace, "geometry3d") == 1);
	Trace.Events.Clear();
	FApplicationScenePlan Next;
	Next.Trace = &Trace;
	Next.Geometry = false;
	REQUIRE(App.GetScenes().RequestChange<AApplicationRenderScene>(Next));
	auto Step = App.Step(1.0 / 60.0);
	REQUIRE(Step && Step.Value());
	REQUIRE(Trace.Deinitialized == 1);
	REQUIRE(CountEvent_Internal(Trace, "geometry3d") == 0);
	REQUIRE(CountEvent_Internal(Trace, "rectangle2d") == 1);
	REQUIRE(CountEvent_Internal(Trace, "present") == 1);
}

TEST("application waits for a failed generated batch and does not present queued geometry")
{
	Testing::FFakeBackend Services;
	FApplicationRenderTrace Trace;
	FApplicationRecordingBackend Renderer(Trace);
	FApplication App({Services, Services, Services, Services, Services, Renderer}, MakeSettings_Internal(4));
	FApplicationScenePlan Plan;
	Plan.Trace = &Trace;
	Plan.Generate = true;
	Plan.FailGeneration = true;
	REQUIRE(App.Start(MakeUnique<AApplicationRenderScene>(Plan)));
	auto Step = App.Step(0.0);
	REQUIRE(!Step);
	REQUIRE(Trace.Generated.Load() == 32);
	REQUIRE(App.GetExecutionJobs().GetCompletedJobCount() == App.GetExecutionJobs().GetSubmittedJobCount());
	REQUIRE(CountEvent_Internal(Trace, "geometry3d") == 0);
	REQUIRE(CountEvent_Internal(Trace, "rectangle2d") == 0);
	REQUIRE(CountEvent_Internal(Trace, "present") == 0);
	REQUIRE(Trace.Deinitialized == 1);
}

// 実物理SceneのStepIndexを描画前後で比較する。Backendだけ記録用に置き換える。
class AViewportPhysicsScene final : public DPhysicsScene3D
{
public:
	explicit AViewportPhysicsScene(bool Split, bool Picking) : m_bSplit(Split), m_bPicking(Picking)
	{
	}
	mutable Toolbox::uint64 Before = 0;
	mutable Toolbox::uint64 After = 0;

protected:
	void OnDraw(FRenderContext& Render) const override
	{
		Before = GetPhysicsWorld().CaptureSnapshot().StepIndex;
		for (int32 Side = 0; Side < (m_bSplit ? 2 : 1); ++Side)
		{
			FRenderView3D View;
			View.bViewport = m_bSplit;
			View.Viewport = {Side * 160, 0, (Side + 1) * 160, 240};
			RequireRender_Internal(Render.Get3D().SetView(View));
			RequireRender_Internal(Render.Get3D().DrawLine({0, 0, 0}, {1, 0, 0}));
			if (m_bPicking)
			{
				REQUIRE(MakeViewPickSegment(View, 320, 240, {static_cast<f32>(Side * 160 + 80), 120}));
				REQUIRE(ProjectWorldToScreen(View, 320, 240, {0, 0, 0}));
			}
		}
		After = GetPhysicsWorld().CaptureSnapshot().StepIndex;
	}

private:
	bool m_bSplit;
	bool m_bPicking;
};
TEST("real Application physics advances once while two viewports only draw")
{
	for (int32 Mode = 0; Mode < 4; ++Mode)
	{
		const bool Split = (Mode & 1) != 0;
		Testing::FFakeBackend Services;
		FApplicationRenderTrace Trace;
		FApplicationRecordingBackend Renderer(Trace);
		FApplication App({Services, Services, Services, Services, Services, Renderer}, MakeSettings_Internal());
		auto Scene = MakeUnique<AViewportPhysicsScene>(Split, (Mode & 2) != 0);
		auto* Observer = Scene.Get();
		REQUIRE(App.Start(Move(Scene)));
		REQUIRE(App.Step(0));
		for (uint64 Frame = 1; Frame <= 3; ++Frame)
		{
			REQUIRE(App.Step(static_cast<f64>(Frame) / 60.0));
			REQUIRE(Observer->Before == Frame);
			REQUIRE(Observer->After == Frame);
		}
		REQUIRE(CountEvent_Internal(Trace, "begin3d") == (Split ? 8 : 4));
		REQUIRE(CountEvent_Internal(Trace, "present") == 4);
	}
}
}
