// SPDX-License-Identifier: NOASSERTION
#include "SmokeSupport.h"
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
	Settings.ExecutionThreadCount = 1;
	return Settings;
}
// 固定した入力を使う実行時の窓口。
FBackendServices Services_Internal(FDxLibBackends& Backends, IInputSource& Input)
{
	const auto Services = Backends.GetServices();
	return {Services.Platform, Input, Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer};
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
	Check(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "front buffer selection failed");
	// ABIに合わせたRGB出力先。
	int R = 0;
	int G = 0;
	int B = 0;
	const Toolbox::int32 Color = DxLib::GetPixel(static_cast<int>(Point.X), static_cast<int>(Point.Y));
	DxLib::GetColor2(Color, &R, &G, &B);
	const Toolbox::int32 Saved = DxLib::SaveDrawScreenToPNG(0, 0, 1280, 720, Path.ToUtf8().CStr());
	Check(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "back buffer restoration failed");
	Check(Saved == 0, "capture failed");
	return {static_cast<Toolbox::uint8>(R), static_cast<Toolbox::uint8>(G), static_cast<Toolbox::uint8>(B), 255};
}
} // namespace Dxf::GameplaySmoke
