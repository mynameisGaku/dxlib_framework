// SPDX-License-Identifier: NOASSERTION
// ModelViewer本体へ固定入力を送り、選択色・投影位置・文字を実D3D11画素で検査する。
#include "../../Examples/ModelViewer/ModelViewerScene.h"
#include "Dxf/Application.h"
#include "Toolbox/Log.h"
#include "Dxf/NativeBackends.h"
#include "Dxf/ViewCoordinates.h"
#include "Toolbox/Platform.h"
#include "DxLib.h"
namespace
{
using namespace Dxf;
class FPickingInput final : public IInputSource
{
public:
	// 次のフレームへ渡す固定入力。
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
}
FRenderView3D View_Internal(bool Split, bool Ortho, Toolbox::int32 Side)
{
	FRenderView3D View;
	View.Eye = Side == 0 ? Toolbox::FVector3{0, 180, -520} : Toolbox::FVector3{400, 260, -450};
	View.Target = {0, 100, 0};
	View.NearPlane = 1;
	View.FarPlane = 3000;
	View.bOrthographic = Ortho;
	View.OrthographicHeight = 400;
	View.bViewport = Split;
	View.Viewport = {Side * 640, 0, (Side + 1) * 640, 720};
	return View;
}
// GPUから一括で読み戻す。中心の選択色と、投影点の印・文字を別々に検査する。
void Capture_Internal(const Toolbox::FPath& Output, bool Split, bool Ortho, Toolbox::int32 Selected)
{
	Check_Internal(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "picking front buffer");
	const Toolbox::int32 Image = DxLib::MakeARGB8ColorSoftImage(1280, 720);
	Check_Internal(Image >= 0, "picking soft image");
	const bool Read = DxLib::GetDrawScreenSoftImage(0, 0, 1280, 720, Image) == 0;
	const bool Saved = DxLib::SaveDrawScreenToPNG(0, 0, 1280, 720, Output.ToUtf8().CStr()) == 0;
	Check_Internal(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "picking back buffer");
	bool Valid = Read && Saved;
	for (Toolbox::int32 Side = 0; Side < (Split ? 2 : 1); ++Side)
	{
		const auto View = View_Internal(Split, Ortho, Side);
		for (Toolbox::int32 Shape = 0; Shape < 2; ++Shape)
		{
			// 右を描いた後でも左ビューを指定して計算する。DxLib状態は変更しない。
			const auto Point = ProjectWorldToScreen(View, 1280, 720, {Shape == 0 ? -100.0f : 100.0f, 100, 0});
			Check_Internal(Point && Point.Value().bInsideView, "picking projected center");
			const Toolbox::int32 X = static_cast<Toolbox::int32>(Point.Value().Screen.X);
			const Toolbox::int32 Y = static_cast<Toolbox::int32>(Point.Value().Screen.Y);
			Toolbox::int32 Yellow = 0;
			Toolbox::int32 Colored = 0;
			Toolbox::int32 Mark = 0;
			Toolbox::int32 Label = 0;
			for (Toolbox::int32 Dy = -12; Dy < 24; ++Dy)
			{
				for (Toolbox::int32 Dx = -12; Dx < 70; ++Dx)
				{
					int R = 0;
					int G = 0;
					int B = 0;
					int A = 0;
					Valid = DxLib::GetPixelSoftImage(Image, X + Dx, Y + Dy, &R, &G, &B, &A) == 0 && Valid;
					if (Dx < 10 && Dy < 10)
					{
						Yellow += R > 20 && G > 15 && G * 3 > R * 2 && B * 3 < G ? 1 : 0;
						Colored += Shape == 0 ? (B > R && G > R ? 1 : 0) : (R > G && R > B ? 1 : 0);
						Mark += R > 230 && G > 230 && B > 230 ? 1 : 0;
					}
					if (Dx >= 10 && Dy >= 0)
					{
						Label += R > 160 && G > 160 && B > 160 ? 1 : 0;
					}
				}
			}
			Toolbox::Out << "picking pixels side=" << Side << " shape=" << Shape << " selected=" << Selected << " yellow=" << Yellow << " colored=" << Colored << " mark=" << Mark << " label=" << Label << "\n";
			Valid = Valid && (Shape == Selected ? Yellow > 100 && Mark > 10 && Label > 10 : Yellow == 0 && Colored > 100 && Mark == 0 && Label == 0);
		}
	}
	DxLib::DeleteSoftImage(Image);
	Check_Internal(Valid, "ModelViewer projected shape selection color or label pixels mismatch");
}
} // namespace
// 既存NativeModelSmokeから呼ぶ。Sceneや入力を代用品へ置き換えない。
void RunPickingExample(const Toolbox::FPath& Root, const Toolbox::FPath& Output, Toolbox::int32 Sequence)
{
	FDxLibBackends Backends;
	FPickingInput Input;
	const auto Services = Backends.GetServices();
	FApplicationSettings Settings;
	Settings.ProjectRoot = Root.ToUtf8();
	Settings.Window.Width = 1280;
	Settings.Window.Height = 720;
	Settings.Window.bVSync = false;
	Settings.ExecutionThreadCount = 1;
	FApplication App({Services.Platform, Input, Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer, Services.pModels}, Settings);
	Check_Internal(static_cast<bool>(App.Start(Toolbox::MakeUnique<ModelViewer::AModelViewerScene>())), "ModelViewer start");
	Toolbox::f64 Time = 0;
	// 選択例も同じプロセス内のApplication番号とフレーム番号を持つ。
	Toolbox::int32 Frame = 0;
	DXF_LOG_INFO("ModelLifecycle", "Picking Application sequence=%d expected=continue", Sequence);
	auto Step = [&]
	{
		const auto Result = App.Step(Time += 1.0 / 60.0);
		if (!Result || !Result.Value())
		{
			DXF_LOG_ERROR("ModelLifecycle", "Picking Step sequence=%d frame=%d result=%s message=%s", Sequence, Frame, Result ? "false" : "error", Result ? "" : Result.Error().Message.CStr());
		}
		++Frame;
		Check_Internal(Result && Result.Value(), "ModelViewer frame");
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
	Step();
	Press(EKey::P);
	for (Toolbox::int32 Mode = 0; Mode < 3; ++Mode)
	{
		const bool Split = Mode != 0;
		const bool Ortho = Mode == 2;
		if (Mode == 1)
		{
			Press(EKey::V);
		}
		if (Mode == 2)
		{
			Press(EKey::O);
		}
		for (Toolbox::int32 Shape = 0; Shape < 2; ++Shape)
		{
			const auto View = View_Internal(Split, Ortho, Split ? Shape : 0);
			const auto Point = ProjectWorldToScreen(View, 1280, 720, {Shape == 0 ? -100.0f : 100.0f, 100, 0});
			Check_Internal(static_cast<bool>(Point), "ModelViewer scripted click projection");
			Click(Point.Value().Screen);
			Capture_Internal(Output / (Toolbox::FString("picking-") + Toolbox::ToString(Mode) + "-" + Toolbox::ToString(Shape) + ".png"), Split, Ortho, Shape);
		}
		// 領域内でも形状のない位置は非選択。
		Click({5, 600});
		Capture_Internal(Output / "picking-miss.png", Split, Ortho, -1);
		// 描画先の右端は半開区間の外。前の選択を保持しない。
		Click({1280, 360});
		Capture_Internal(Output / "picking-outside.png", Split, Ortho, -1);
	}
	Click(ProjectWorldToScreen(View_Internal(true, true, 0), 1280, 720, {-100, 100, 0}).Value().Screen);
	Press(EKey::R);
	Capture_Internal(Output / "picking-reload.png", true, true, -1);
	Press(EKey::Enter);
	Press(EKey::P);
	Capture_Internal(Output / "picking-scene.png", false, false, -1);
	App.Shutdown();
	Check_Internal(!App.IsRunning(), "ModelViewer shutdown");
	Toolbox::Out << "PASS real ModelViewer fixed mouse input sphere box labels full split ortho outside reload scene shutdown\n";
}
