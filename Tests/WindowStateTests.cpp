// SPDX-License-Identifier: NOASSERTION
// P3: フレームの境界で確定するウィンドウの状態、最小化中の描画・更新、描画先の寸法。
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/Application.h"
#include "Dxf/Scene.h"
using namespace Dxf;
using namespace Dxf::Testing;
namespace
{
// 更新・描画の回数と、最後に受け取ったウィンドウの状態・時間。
struct FWindowObservation
{
	Toolbox::int32 Ticks = 0;
	Toolbox::int32 Draws = 0;
	FWindowState Window;
	Toolbox::f64 LastDelta = 0;
	Toolbox::int32 TargetWidth = 0;
};

class DWindowScene final : public DScene
{
public:
	explicit DWindowScene(FWindowObservation& Observation) : m_pObservation(&Observation)
	{
	}

protected:
	void OnTick(const FTickContext& Context) override
	{
		++m_pObservation->Ticks;
		m_pObservation->Window = Context.Window;
		m_pObservation->LastDelta = Context.Time.DeltaSeconds;
	}
	void OnDraw(FRenderContext& Render) const override
	{
		++m_pObservation->Draws;
		m_pObservation->TargetWidth = Render.GetTargetWidth();
	}

private:
	FWindowObservation* m_pObservation;
};

FBackendServices MakeServices(FFakeBackend& Backend)
{
	return {Backend, Backend, Backend, Backend, Backend, Backend};
}

FWindowState Visible(Toolbox::int32 Width, Toolbox::int32 Height)
{
	FWindowState State;
	State.bKnown = true;
	State.ClientWidth = Width;
	State.ClientHeight = Height;
	State.RenderWidth = Width;
	State.RenderHeight = Height;
	return State;
}

void Step(FApplication& App, Toolbox::int32& Frame)
{
	const auto Result = App.Step(static_cast<Toolbox::f64>(Frame++) / 60.0);
	REQUIRE(Result && Result.Value());
}
} // namespace

TEST("Application draws with the render size fixed at the frame boundary and exposes it to scenes")
{
	FFakeBackend Backend;
	FWindowObservation Observation;
	FApplication App(MakeServices(Backend));
	REQUIRE(App.Start(Toolbox::MakeUnique<DWindowScene>(Observation)));
	Toolbox::int32 Frame = 0;
	// 取得できない実装では起動時の寸法で描く。
	Step(App, Frame);
	REQUIRE(Observation.TargetWidth == 1280 && !Observation.Window.bKnown);
	// 描画先の寸法が変わったフレームから、その寸法で描き、同じ値を更新へ渡す。
	Backend.GetTrace().Window = Visible(1001, 501);
	Backend.GetTrace().Window.Revision = 3;
	Step(App, Frame);
	REQUIRE(Observation.TargetWidth == 1001 && Backend.GetTrace().TargetWidth == 1001);
	REQUIRE(Observation.Window.bKnown && Observation.Window.RenderHeight == 501 && Observation.Window.Revision == 3);
	REQUIRE(Backend.GetTrace().SyncWindows == 2);
}

TEST("Application skips drawing while minimized and keeps updating unless pausing is configured")
{
	for (bool bPause : {false, true})
	{
		FFakeBackend Backend;
		Backend.GetTrace().Window = Visible(1280, 720);
		FWindowObservation Observation;
		FApplicationSettings Settings;
		Settings.Window.bPauseWhenMinimized = bPause;
		FApplication App(MakeServices(Backend), Settings);
		REQUIRE(App.Start(Toolbox::MakeUnique<DWindowScene>(Observation)));
		Toolbox::int32 Frame = 0;
		Step(App, Frame);
		Step(App, Frame);
		REQUIRE(Observation.Draws == 2 && Backend.GetTrace().Presentations == 2);
		// 最小化の間は描画先を作らず、描画も提示もしない。更新は設定どおり。
		Backend.GetTrace().Window.bMinimized = true;
		Backend.GetTrace().Window.ClientWidth = 0;
		Backend.GetTrace().Window.ClientHeight = 0;
		const Toolbox::int32 TicksBefore = Observation.Ticks;
		for (Toolbox::int32 I = 0; I < 30; ++I)
		{
			Step(App, Frame);
		}
		REQUIRE(Observation.Draws == 2 && Backend.GetTrace().Presentations == 2);
		REQUIRE(Observation.Ticks == (bPause ? TicksBefore : TicksBefore + 30));
		// 復帰した最初のフレームの時間は1フレーム分（止めていた時間を追い付きへ変えない）。
		Backend.GetTrace().Window = Visible(1280, 720);
		Step(App, Frame);
		REQUIRE(Observation.Draws == 3 && Backend.GetTrace().Presentations == 3);
		REQUIRE(Toolbox::Abs(Observation.LastDelta - 1.0 / 60.0) < 1.0e-9);
		// 0x0の描画先（取得できたが表示できない）も同じく描かない。
		Backend.GetTrace().Window = Visible(0, 0);
		Step(App, Frame);
		REQUIRE(Observation.Draws == 3);
	}
}

TEST("Application stops with the first error when the window state cannot be synchronized")
{
	class FFailingWindow final : public IPlatform
	{
	public:
		TResult<void> Initialize(const FWindowSettings&) override
		{
			return {};
		}
		void Shutdown() noexcept override
		{
		}
		TResult<bool> PumpEvents() override
		{
			return TResult<bool>::Success(true);
		}
		TResult<FWindowState> SyncWindow() override
		{
			return TResult<FWindowState>::Failure(EErrorCode::BackendFailure, "Render size change to 10x10 failed");
		}
	};
	FFakeBackend Backend;
	FFailingWindow Platform;
	FWindowObservation Observation;
	FApplication App({Platform, Backend, Backend, Backend, Backend, Backend});
	REQUIRE(App.Start(Toolbox::MakeUnique<DWindowScene>(Observation)));
	const auto Result = App.Step(0);
	REQUIRE(!Result && Result.Error().Message == "Render size change to 10x10 failed");
	REQUIRE(Observation.Draws == 0 && Backend.GetTrace().Presentations == 0);
}
