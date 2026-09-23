// SPDX-License-Identifier: NOASSERTION
// 入力だけを固定し、Sandbox本体を実Application・実DxLibでプレイする。
#include "SandboxMenuScene.h"
#include "SandboxGame.h"
#include "Dxf/Application.h"
#include "Dxf/NativeBackends.h"
#include "Toolbox/Platform.h"
#include "DxLib.h"
// Windowsの文字種マクロとToolboxの同名関数を分離する。
#ifdef CreateDirectory
#undef CreateDirectory
#endif
namespace
{
using namespace Dxf;
using namespace Dxf::Sandbox;
// OS入力の揺れを排除する。更新と押下判定は本物のInputSystemへ任せる。
class FScriptedInput final : public IInputSource
{
public:
	// 次のフレームで押すキー。
	FRawInput State;
	TResult<FRawInput> Poll() override
	{
		return TResult<FRawInput>::Success(State);
	}
};
// 失敗内容を実行結果へ伝える。
void Check(bool bOk, const char* Message)
{
	if (!bOk)
	{
		throw Toolbox::FException(Message);
	}
}
// 実際に表示した画面を保存し、ゴール帯の画素を照合する。
void Capture(const Toolbox::FPath& Path, bool bPlay)
{
	Check(DxLib::SetDrawScreen(DX_SCREEN_FRONT) == 0, "front buffer selection failed");
	// ABIに合わせたRGB出力先。
	int R = 0;
	int G = 0;
	int B = 0;
	const Toolbox::int32 Color = DxLib::GetPixel(1150, 400);
	DxLib::GetColor2(Color, &R, &G, &B);
	const Toolbox::int32 Saved = DxLib::SaveDrawScreenToPNG(0, 0, 1280, 720, Path.ToUtf8().CStr());
	Check(DxLib::SetDrawScreen(DX_SCREEN_BACK) == 0, "back buffer restoration failed");
	Check(Saved == 0, "capture failed");
	Check(bPlay ? R == 40 && G == 160 && B == 80 : R == 0 && G == 0 && B == 0,
	      "goal pixels or menu background mismatch");
}
} // namespace
Toolbox::int32 main(Toolbox::int32 Count, char** Args)
{
	if (Count != 3)
	{
		return 2;
	}
	try
	{
		// ネイティブ窓口はApplicationより長く生存する。
		FDxLibBackends Backends;
		FScriptedInput Input;
		const auto Services = Backends.GetServices();
		FApplicationSettings Settings;
		Settings.ProjectRoot = Args[1];
		Settings.Window.Width = 1280;
		Settings.Window.Height = 720;
		Settings.Window.bVSync = false;
		Settings.ExecutionThreadCount = 1;
		FApplication App(
		    {Services.Platform, Input, Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer},
		    Settings);
		const Toolbox::FPath Output(Args[2]);
		Check(Toolbox::IsDirectory(Output) || Toolbox::CreateDirectory(Output), "output directory failed");
		Check(static_cast<bool>(App.Start(Toolbox::MakeUnique<ASandboxMenuScene>())), "start failed");
		Toolbox::f64 Time = 0;
		auto Step = [&]
		{
			const auto Result = App.Step(Time += 0.1);
			Check(Result && Result.Value(), "game step failed");
		};
		auto Press = [&](EKey Key)
		{
			Input.State.Keys[static_cast<Toolbox::size_t>(Key)] = true;
			Step();
			Input.State.Keys[static_cast<Toolbox::size_t>(Key)] = false;
			Step();
		};
		Step();
		Capture(Output / "title.png", false);
		Press(EKey::Enter);
		Check(App.GetScenes().GetCurrent()->TryCast<DSandboxScene>() != nullptr, "play scene missing");
		Capture(Output / "play.png", true);
		Press(EKey::Space);
		Input.State.Keys[static_cast<Toolbox::size_t>(EKey::D)] = true;
		for (Toolbox::int32 Frame = 0; Frame < 40; ++Frame)
		{
			Step();
		}
		Input.State.Keys[static_cast<Toolbox::size_t>(EKey::D)] = false;
		Check(App.GetScenes().GetCurrent()->TryCast<ASandboxMenuScene>() != nullptr, "result scene missing");
		Capture(Output / "result.png", false);
		Press(EKey::Enter);
		auto* Replay = App.GetScenes().GetCurrent()->TryCast<DSandboxScene>();
		Check(Replay && Replay->GetPlayer().Get()->GetPosition().X == 320, "retry did not reset player");
		Capture(Output / "retry.png", true);
		Input.State.Keys[static_cast<Toolbox::size_t>(EKey::Escape)] = true;
		const auto Quit = App.Step(Time += 0.1);
		Check(Quit && !Quit.Value(), "quit failed");
		Toolbox::Out << "SANDBOX_GAME_FLOW_PASSED\n";
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
