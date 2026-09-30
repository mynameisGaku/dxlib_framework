// SPDX-License-Identifier: NOASSERTION
#include "SmokeSupport.h"
#include "Dxf/NativeHandle.h"
#include "DxLib.h"
namespace Dxf::GameplaySmoke
{
namespace
{
// Applicationの設定（1280x720、垂直同期なし、実行レーン1）。
FApplicationSettings Settings_Internal(const char* ProjectRoot)
{
	FApplicationSettings Settings;
	Settings.ProjectRoot = ProjectRoot;
	Settings.Window.Width = 1280;
	Settings.Window.Height = 720;
	Settings.Window.bVSync = false;
	Settings.Window.Resize = EWindowResizeMode::Resizable;
	Settings.ExecutionThreadCount = 1;
	return Settings;
}
// 固定した入力を使う実行時の窓口。
FBackendServices Services_Internal(FDxLibBackends& Backends, IInputSource& Input)
{
	const auto Services = Backends.GetServices();
	return {Services.Platform, Input, Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer};
}
// 一括読戻し用のCPU画像を解放する。
void ReleaseImage_Internal(void*, Toolbox::int32 Handle) noexcept
{
	(void)DxLib::DeleteSoftImage(Handle);
}
} // namespace

TResult<FRawInput> FScriptedInput::Poll()
{
	return TResult<FRawInput>::Success(State);
}
void Check(bool bOk, const char* Message)
{
	if (!bOk)
	{
		throw Toolbox::FException(Message);
	}
}
FSmokeApp::FSmokeApp(FDxLibBackends& Backends, const char* ProjectRoot)
    : m_App(Services_Internal(Backends, m_Input), Settings_Internal(ProjectRoot))
{
}
void FSmokeApp::Start()
{
	Check(static_cast<bool>(m_App.Start(Toolbox::MakeUnique<GameplaySample::DCharacterSample2DScene>())),
	      "start failed");
	// 最初のStepの時刻が基準になる。その後は固定更新の境界から4分の1ずらした時刻で進める。
	m_Time = 0;
	const auto First = m_App.Step(m_Time);
	Check(First && First.Value(), "first step failed");
	m_Time += 0.25 / 60.0;
}
void FSmokeApp::Step()
{
	const auto Result = m_App.Step(m_Time += 1.0 / 60.0);
	Check(Result && Result.Value(), "game step failed");
}
void FSmokeApp::StartInteraction(bool b3D)
{
	if (b3D)
	{
		Check(static_cast<bool>(m_App.Start(Toolbox::MakeUnique<GameplaySample::DInteraction3DScene>())),
		      "interaction 3D start failed");
	}
	else
	{
		Check(static_cast<bool>(m_App.Start(Toolbox::MakeUnique<GameplaySample::DInteraction2DScene>())),
		      "interaction 2D start failed");
	}
	// 既存の試験と同じ、固定更新の境界から4分の1ずらしたフレーム時刻。
	m_Time = 0;
	const auto First = m_App.Step(m_Time);
	Check(First && First.Value(), "interaction first step failed");
	m_Time += 0.25 / 60.0;
}
void FSmokeApp::Press(EKey Key)
{
	m_Input.State.Keys[static_cast<Toolbox::size_t>(Key)] = true;
	Step();
	m_Input.State.Keys[static_cast<Toolbox::size_t>(Key)] = false;
}
void FSmokeApp::Hold(EKey Key, bool bDown)
{
	m_Input.State.Keys[static_cast<Toolbox::size_t>(Key)] = bDown;
}
void FSmokeApp::SwitchDimension()
{
	Press(EKey::Tab);
	const auto Result = m_App.Step(m_Time += 1.25 / 60.0);
	Check(Result && Result.Value(), "game step failed");
}
void FSmokeApp::Quit()
{
	m_Input.State = {};
	m_Input.State.Keys[static_cast<Toolbox::size_t>(EKey::Escape)] = true;
	const auto Result = m_App.Step(m_Time += 1.0 / 60.0);
	m_Input.State = {};
	Check(Result && !Result.Value(), "quit failed");
}
GameplaySample::DCharacterSample2DScene& FSmokeApp::Scene2D()
{
	auto* Scene = m_App.GetScenes().GetCurrent()->TryCast<GameplaySample::DCharacterSample2DScene>();
	Check(Scene != nullptr, "2D scene missing");
	return *Scene;
}
GameplaySample::DCharacterSample3DScene& FSmokeApp::Scene3D()
{
	auto* Scene = m_App.GetScenes().GetCurrent()->TryCast<GameplaySample::DCharacterSample3DScene>();
	Check(Scene != nullptr, "3D scene missing");
	return *Scene;
}
FColor FSmokeApp::Capture(const Toolbox::FPath& Path, FVector2 Point)
{
	FColor Color;
	CapturePoints(Path, &Point, &Color, 1);
	return Color;
}
GameplaySample::DInteraction2DScene& FSmokeApp::Interaction2D()
{
	auto* Scene = m_App.GetScenes().GetCurrent()->TryCast<GameplaySample::DInteraction2DScene>();
	Check(Scene != nullptr, "interaction 2D scene missing");
	return *Scene;
}
GameplaySample::DInteraction3DScene& FSmokeApp::Interaction3D()
{
	auto* Scene = m_App.GetScenes().GetCurrent()->TryCast<GameplaySample::DInteraction3DScene>();
	Check(Scene != nullptr, "interaction 3D scene missing");
	return *Scene;
}
void FSmokeApp::CapturePoints(const Toolbox::FPath& Path, const FVector2* Points, FColor* Colors, Toolbox::size_t Count)
{
	// GPUからの読み戻しは一度だけ行い、保存と画素の確認は同じCPU画像を使う。
	// 現在の描画サイズ。Resize後も固定寸法を読戻しに使わない。
	int Width = 0;
	int Height = 0;
	Check(DxLib::GetDrawScreenSize(&Width, &Height) == 0, "capture render size failed");
	FNativeHandle Image(DxLib::MakeARGB8ColorSoftImage(Width, Height), nullptr, &ReleaseImage_Internal);
	Check(Image.Get() >= 0, "capture image allocation failed");
	Check(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "front buffer selection failed");
	const Toolbox::int32 Read = DxLib::GetDrawScreenSoftImage(0, 0, Width, Height, Image.Get());
	Check(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "back buffer restoration failed");
	Check(Read == 0, "capture one-shot readback failed");
	Check(DxLib::SaveSoftImageToPng(Path.ToUtf8().CStr(), Image.Get(), 1) == 0, "capture failed");
	for (Toolbox::size_t Index = 0; Index < Count; ++Index)
	{
		// 描画領域から外れた投影を端へ丸めて成功にしない。
		Check(Points[Index].X >= 0 && Points[Index].Y >= 0 && Points[Index].X < Width && Points[Index].Y < Height,
		      "capture point outside image");
		// DxLibのABIが要求するRGBA出力先。
		int R = 0;
		int G = 0;
		int B = 0;
		int A = 0;
		Check(DxLib::GetPixelSoftImage(Image.Get(), static_cast<int>(Points[Index].X),
		                               static_cast<int>(Points[Index].Y), &R, &G, &B, &A) == 0,
		      "capture CPU pixel read failed");
		Colors[Index] = {static_cast<Toolbox::uint8>(R), static_cast<Toolbox::uint8>(G), static_cast<Toolbox::uint8>(B),
		                 255};
	}
}
} // namespace Dxf::GameplaySmoke
