// SPDX-License-Identifier: NOASSERTION
#include "../../Examples/RenderDebug/RenderDebugScene.h"
#include "Dxf/Application.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/ViewCoordinates.h"
#include "Toolbox/Platform.h"
#include "DxLib.h"
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#include <stdio.h>
namespace
{
using namespace Dxf;
class FSnapshotInput final : public IInputSource
{
public:
	FRawInput m_State;
	TResult<FRawInput> Poll() override
	{
		return TResult<FRawInput>::Success(m_State);
	}
};
void Check_Internal(bool Good, const char* Text)
{
	if (!Good)
	{
		throw Toolbox::FException(Text);
	}
	printf("PASS %s\n", Text);
	fflush(stdout);
}
// 一括読戻しで選択色・重心の印・下部の詳細文字を確認する。
void Capture_Internal(const Toolbox::FPath& File, FVector2 Point, bool Selected = true)
{
	Check_Internal(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "snapshot front buffer");
	const auto Image = DxLib::MakeARGB8ColorSoftImage(1280, 720);
	Check_Internal(Image >= 0, "snapshot image allocation");
	const bool Read = DxLib::GetDrawScreenSoftImage(0, 0, 1280, 720, Image) == 0;
	const bool Saved = DxLib::SaveDrawScreenToPNG(0, 0, 1280, 720, File.ToUtf8().CStr()) == 0;
	Check_Internal(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "snapshot back buffer");
	Toolbox::int32 Yellow = 0;
	Toolbox::int32 White = 0;
	Toolbox::int32 Details = 0;
	const Toolbox::int32 X = static_cast<Toolbox::int32>(Point.X);
	const Toolbox::int32 Y = static_cast<Toolbox::int32>(Point.Y);
	for (Toolbox::int32 Dy = -10; Selected && Dy <= 10; ++Dy)
	{
		for (Toolbox::int32 Dx = -10; Dx <= 10; ++Dx)
		{
			int R = 0;
			int G = 0;
			int B = 0;
			int A = 0;
			DxLib::GetPixelSoftImage(Image, X + Dx, Y + Dy, &R, &G, &B, &A);
			Yellow += R == 255 && G == 220 && B == 30 ? 1 : 0;
			White += R > 230 && G > 230 && B > 230 ? 1 : 0;
		}
	}
	for (Toolbox::int32 YText = 654; YText < 710; ++YText)
	{
		for (Toolbox::int32 XText = 12; XText < 900; ++XText)
		{
			int R = 0;
			int G = 0;
			int B = 0;
			int A = 0;
			DxLib::GetPixelSoftImage(Image, XText, YText, &R, &G, &B, &A);
			Details += R > 180 && G > 180 && B > 180 ? 1 : 0;
		}
	}
	DxLib::DeleteSoftImage(Image);
	Check_Internal(Read && Saved && (Selected ? Yellow > 100 && White > 15 && Details > 100 : Details == 0), "snapshot selection highlight or label pixels");
}
} // namespace
void RunPhysicsPickingExample(const Toolbox::FPath& Root, const Toolbox::FPath& Output)
{
	using namespace Dxf;
	FDxLibBackends Backends;
	FSnapshotInput Input;
	const auto Services = Backends.GetServices();
	FApplicationSettings Settings;
	Settings.ProjectRoot = Root.ToUtf8();
	Settings.Window.Width = 1280;
	Settings.Window.Height = 720;
	Settings.Window.bVSync = false;
	Settings.ExecutionThreadCount = 1;
	FApplication App({Services.Platform, Input, Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer, Services.pModels}, Settings);
	auto Scene = Toolbox::MakeUnique<RenderDebug::ARenderDebugScene>(App.GetExecutionJobs());
	auto* Observer = Scene.Get();
	Check_Internal(static_cast<bool>(App.Start(Toolbox::Move(Scene))), "real RenderDebug start");
	Toolbox::f64 Time = 0;
	auto Step = [&]
	{
		const auto Result = App.Step(Time);
		Time += 1.0 / 60;
		Check_Internal(Result && Result.Value(), "real RenderDebug frame");
	};
	auto Press = [&](EKey Key)
	{
		Input.m_State.Keys[static_cast<Toolbox::size_t>(Key)] = true;
		Step();
		Input.m_State.Keys[static_cast<Toolbox::size_t>(Key)] = false;
		Step();
	};
	auto Click = [&](FVector2 Point)
	{
		Input.m_State.MouseX = static_cast<Toolbox::int32>(Point.X);
		Input.m_State.MouseY = static_cast<Toolbox::int32>(Point.Y);
		Input.m_State.MouseButtons[0] = true;
		Step();
		Input.m_State.MouseButtons[0] = false;
		Step();
	};
	Press(EKey::P);
	const auto Covered = ProjectWorldToScreen(Observer->GetDisplayView(), 1280, 720, Observer->GetDisplaySnapshot().Items[2].CenterOfMass).Value();
	Check_Internal(Covered.Screen.Y < 208, "sphere lies behind visible explanation panel");
	Click(Covered.Screen);
	Check_Internal(!Observer->GetPickedCollider(), "visible panel blocks 3D selection");
	Press(EKey::Tab);
	Press(EKey::F2); // 照明なしにして選択色を厳密比較する。
	const auto Initial = Observer->GetDisplaySnapshot();
	Check_Internal(Initial.Step == 0 && Initial.Items.Size() == 3, "initial paused snapshot");
	for (Toolbox::size_t Index = 1; Index < 3; ++Index)
	{
		const auto Point = ProjectWorldToScreen(Observer->GetDisplayView(), 1280, 720, Initial.Items[Index].CenterOfMass);
		Check_Internal(Point && Point.Value().bInsideView, "initial collider screen point");
		Click(Point.Value().Screen);
		Check_Internal(Observer->GetPickedCollider() && *Observer->GetPickedCollider() == Initial.Items[Index].Collider, "real RenderDebug selected collider");
		Capture_Internal(Output / (Index == 1 ? "physics-box.png" : "physics-sphere.png"), Point.Value().Screen);
		Check_Internal(Observer->GetDisplaySnapshot().Step == 0 && Observer->GetSimulationSeconds() == 0, "click while paused does not step");
	}
	Click({1279, 0});
	Check_Internal(!Observer->GetPickedCollider(), "empty click clears selection");
	Press(EKey::F9);
	Click({1100, 650});
	Check_Internal(!Observer->GetPickedCollider(), "2D observation panel blocks 3D selection");
	Press(EKey::F9);
	// カメラ移動とクリックで、同じ更新後ビューを参照する。
	Press(EKey::Right);
	auto Point = ProjectWorldToScreen(Observer->GetDisplayView(), 1280, 720, Initial.Items[2].CenterOfMass).Value();
	Click(Point.Screen);
	Check_Internal(Observer->GetPickedCollider() && *Observer->GetPickedCollider() == Initial.Items[2].Collider, "camera movement picking");
	Capture_Internal(Output / "physics-camera.png", Point.Screen);
	Press(EKey::P);
	for (Toolbox::int32 Frame = 0; Frame < 45; ++Frame)
	{
		Step();
	}
	Press(EKey::P);
	const auto Live = Observer->GetDisplaySnapshot();
	Check_Internal(Live.Items[2].CenterOfMass.Y < Initial.Items[2].CenterOfMass.Y - 1, "history needs distinct real positions");
	const auto Seconds = Observer->GetSimulationSeconds();
	const auto HistoryCount = Observer->GetHistoryCount();
	for (Toolbox::size_t Age = 1; Age < HistoryCount; ++Age)
	{
		Press(EKey::Z);
	}
	Check_Internal(!Observer->GetPickedCollider() && Observer->GetDisplaySnapshot().Step == 0, "history switch clears selection");
	Point = ProjectWorldToScreen(Observer->GetDisplayView(), 1280, 720, Initial.Items[2].CenterOfMass).Value();
	Click(Point.Screen);
	Check_Internal(Observer->GetPickedCollider() && *Observer->GetPickedCollider() == Initial.Items[2].Collider, "history picks stored position rather than live position");
	Check_Internal(Observer->GetDisplaySnapshot().Items[2].CenterOfMass == Initial.Items[2].CenterOfMass, "history details stay historical");
	Capture_Internal(Output / "physics-history.png", Point.Screen);
	Press(EKey::F8);
	Press(EKey::Z);
	Click(Point.Screen);
	Check_Internal(!Observer->GetPickedCollider() && Observer->GetDisplaySnapshot().Items.IsEmpty() && Observer->GetHistoryCount() == HistoryCount && Observer->GetSimulationSeconds() == Seconds, "observation OFF ignores clicks and history without capture or step");
	Capture_Internal(Output / "physics-disabled.png", {}, false);
	Press(EKey::F8);
	Check_Internal(!Observer->GetPickedCollider() && Observer->GetDisplaySnapshot().Step == Live.Step, "observation ON does not restore selection");
	Press(EKey::Enter);
	Check_Internal(!Observer->GetPickedCollider() && Observer->GetDisplaySnapshot().World != Initial.World, "World reset invalidates selection");
	// Scene破棄と新しいSceneの採取を通し、古いIDを復活させない。
	auto Next = Toolbox::MakeUnique<RenderDebug::ARenderDebugScene>(App.GetExecutionJobs());
	Observer = Next.Get();
	Check_Internal(static_cast<bool>(App.GetScenes().RequestChange(Toolbox::Move(Next))), "request RenderDebug reentry");
	Step();
	Check_Internal(!Observer->GetPickedCollider() && Observer->GetDisplaySnapshot().World != Initial.World, "Scene reentry invalidates selection");
	App.Shutdown();
	Check_Internal(!App.IsRunning(), "RenderDebug shutdown");
	Toolbox::Out << "PASS real RenderDebug snapshot selection history camera observation reset lifecycle pixels\n";
}

Toolbox::int32 main(Toolbox::int32 Count, char** Args)
{
	if (Count != 3)
	{
		return 2;
	}
	try
	{
		const Toolbox::FPath Output(Args[2]);
		Check_Internal(Toolbox::IsDirectory(Output) || Toolbox::CreateDirectory(Output), "snapshot output directory");
		RunPhysicsPickingExample(Toolbox::FPath(Args[1]), Output);
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
