// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "../Examples/RenderDebug/RenderDebugScene.h"
#include "Dxf/Application.h"
#include "Dxf/ViewCoordinates.h"
using namespace Dxf;
using namespace Toolbox;
namespace
{
// 実Scene/World/Rendererを通し、Native境界だけ記録する。
class FObservationRenderer final : public IRenderBackend
{
public:
	TVector<FString> m_Text;
	uint32 m_Frames = 0;
	TResult<void> SetTarget(int32, int32, int32) override
	{
		return {};
	}
	TResult<void> Clear(FColor) override
	{
		m_Text.Clear();
		return {};
	}
	TResult<void> ResetState(int32, int32) override
	{
		return {};
	}
	TResult<void> DrawSprite(const FSpriteCommand&) override
	{
		return {};
	}
	TResult<void> DrawRectangle(const FRectangleCommand&) override
	{
		return {};
	}
	TResult<void> DrawText(const FTextCommand& Text) override
	{
		m_Text.PushBack(Text.Text);
		return {};
	}
	TResult<void> DrawCircle2D(const FCircleCommand2D&) override
	{
		return {};
	}
	TResult<void> DrawLine2D(const FLineCommand2D&) override
	{
		return {};
	}
	TResult<void> DrawTriangle2D(const FTriangleCommand2D&) override
	{
		return {};
	}
	bool SupportsShapes2D() const noexcept override
	{
		return true;
	}
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	TResult<void> BeginView3D(const FRenderView3D&) override
	{
		return {};
	}
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D&) override
	{
		return {};
	}
	TResult<void> Present() override
	{
		++m_Frames;
		return {};
	}
};
// 比較用の観察値。Native画像試験とは区別する。
struct FMotion
{
	TVector<FPhysicsDebugSnapshot3D> Frames;
	uint32 Presentations = 0;
	size_t History = 0;
	uint32 Picked = 0;
};
FMotion Run_Internal(bool Click)
{
	Testing::FFakeBackend Services;
	FObservationRenderer Renderer;
	FApplicationSettings Settings;
	Settings.Window.Width = 1280;
	Settings.Window.Height = 720;
	Settings.ExecutionThreadCount = 1;
	FApplication App({Services, Services, Services, Services, Services, Renderer}, Settings);
	auto Scene = MakeUnique<RenderDebug::ARenderDebugScene>(App.GetExecutionJobs());
	auto* Observer = Scene.Get();
	REQUIRE(App.Start(Move(Scene)));
	FMotion Trace;
	for (uint32 Frame = 0; Frame < 40; ++Frame)
	{
		auto& Input = Services.GetTrace().Input;
		// 説明を一度閉じ、同じ時刻列でクリックだけを切り替える。
		Input.Keys[static_cast<size_t>(EKey::Tab)] = Frame == 0;
		const auto Point = ProjectWorldToScreen(Observer->GetDisplayView(), 1280, 720, Observer->GetDisplaySnapshot().Items[2].CenterOfMass);
		REQUIRE(Point);
		Input.MouseX = static_cast<int32>(Point.Value().Screen.X);
		Input.MouseY = static_cast<int32>(Point.Value().Screen.Y);
		Input.MouseButtons[0] = Click && Frame % 2 == 0;
		REQUIRE(App.Step(Frame / 60.0));
		Trace.Picked += Observer->GetPickedCollider() ? 1 : 0;
		Trace.Frames.PushBack(Observer->GetDisplaySnapshot());
	}
	Trace.Presentations = Renderer.m_Frames;
	Trace.History = Observer->GetHistoryCount();
	App.Shutdown();
	return Trace;
}
} // namespace
TEST("real RenderDebug clicks leave every frame physics values and history unchanged")
{
	const auto Plain = Run_Internal(false);
	const auto Picking = Run_Internal(true);
	REQUIRE(Picking.Picked > 0 && Plain.Picked == 0);
	REQUIRE(Plain.Presentations == 40 && Picking.Presentations == 40 && Plain.History == Picking.History);
	for (size_t Frame = 0; Frame < 40; ++Frame)
	{
		const auto& A = Plain.Frames[Frame];
		const auto& B = Picking.Frames[Frame];
		REQUIRE(A.Step == B.Step && A.SimulationSeconds == B.SimulationSeconds && A.Items.Size() == B.Items.Size());
		for (size_t Index = 0; Index < A.Items.Size(); ++Index)
		{
			REQUIRE(A.Items[Index].CenterOfMass == B.Items[Index].CenterOfMass);
			REQUIRE(A.Items[Index].Velocity == B.Items[Index].Velocity);
			REQUIRE(A.Items[Index].AngularVelocity == B.Items[Index].AngularVelocity);
			REQUIRE(A.Items[Index].bSleeping == B.Items[Index].bSleeping);
		}
	}
}
