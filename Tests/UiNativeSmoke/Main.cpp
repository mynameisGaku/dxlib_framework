// SPDX-License-Identifier: NOASSERTION
// 実UIサンプルを実DxLibで操作する。入力だけを固定し、Present前に文字・クリップの画素を読む。
#include "DisplayScenario.h"
#include "FixedInput.h"
#include "ForwardRenderer.h"
#include "NativePixels.h"
#include "UiSampleScenes.h"
#include "UiTitleScene.h"
#include "UiPlay2DScene.h"
#include "UiPlay3DScene.h"
#include "Dxf/Application.h"
#include "Dxf/NativeBackends.h"
#include "Toolbox/Platform.h"
namespace
{
using namespace Dxf;
using namespace Dxf::UiSample;
using namespace Dxf::UiSmoke;
void Check(bool Value, const char* Error)
{
	if (!Value)
	{
		throw Toolbox::FException(Error);
	}
}

DUiElement* Find(DUiElement& Parent, const char* Name)
{
	if (Parent.GetName() == Name && Parent.IsAttached())
	{
		return &Parent;
	}
	for (Toolbox::size_t I = 0; I < Parent.GetChildCount(); ++I)
	{
		if (auto* Child = Parent.GetChild(I))
		{
			if (auto* Found = Find(*Child, Name))
			{
				return Found;
			}
		}
	}
	return nullptr;
}

FUiSampleShell& Shell(FApplication& App)
{
	auto* Scene = App.GetScenes().GetCurrent();
	if (auto* Title = dynamic_cast<DUiTitleScene*>(Scene))
	{
		return Title->GetUi();
	}
	if (auto* Play = dynamic_cast<DUiPlay2DScene*>(Scene))
	{
		return Play->GetUi();
	}
	if (auto* Play = dynamic_cast<DUiPlay3DScene*>(Scene))
	{
		return Play->GetUi();
	}

	throw Toolbox::FException("UI sample has no active scene");
}

void Step(FApplication& App, Toolbox::uint64& Frame)
{
	const auto Result = App.Step(static_cast<Toolbox::f64>(Frame++) / 60.0);
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	Check(Result.Value(), "UI sample window ended before scenario completed");
}

void Click(FApplication& App, FFixedInput& Input, Toolbox::uint64& Frame, const char* Name)
{
	auto& Root = Shell(App).GetRoot();
	DUiElement* Element = nullptr;
	if (auto* Modal = Root.GetTopModal())
	{
		Element = Find(*Modal, Name);
	}
	else
	{
		for (Toolbox::size_t I = 0; I < static_cast<Toolbox::size_t>(EUiLayer::Count); ++I)
		{
			if (auto* Found = Find(Root.GetLayer(static_cast<EUiLayer>(I)), Name))
			{
				Element = Found;
				break;
			}
		}
	}
	Check(Element != nullptr, "UI control unavailable");
	const auto Rect = Element->GetRect();
	const auto Pixel = Root.GetSurface().ToPixel(FVector2{Rect.X + Rect.Width * 0.5f, Rect.Y + Rect.Height * 0.5f});
	Input.Raw.MouseX = static_cast<Toolbox::int32>(Pixel.X);
	Input.Raw.MouseY = static_cast<Toolbox::int32>(Pixel.Y);
	Input.Raw.MouseButtons[0] = true;
	Step(App, Frame);
	Input.Raw.MouseButtons[0] = false;
	Step(App, Frame);
}
} // namespace

int main(int Count, char** Args)
{
	if (Count != 3)
	{
		Toolbox::Err << "Usage: NativeUiSmoke <ProjectRoot> <output directory>\n";
		return 2;
	}
	try
	{
		const Toolbox::FPath Out(Args[2]);
		Check(Toolbox::IsDirectory(Out) || Toolbox::CreateDirectory(Out), "output parent must exist");
		for (bool ThreeD : {false, true})
		{
			FDxLibBackends Native;
			const auto Original = Native.GetServices();
			FFixedInput Input;
			FForwardRenderer Render(Original.Renderer);
			FBackendServices Services{Original.Platform, Input,  Original.Textures, Original.Sounds,
			                          Original.Fonts,    Render, Original.pModels};
			FApplicationSettings Settings;
			Settings.Window.Width = 1280;
			Settings.Window.Height = 720;
			Settings.Window.bVSync = false;
			Settings.ExecutionThreadCount = 1;
			Settings.ProjectRoot = Args[1];
			auto State = Toolbox::MakeShared<FUiSampleState>();
			FApplication App(Services, Settings);
			Check(static_cast<bool>(App.Start(MakeTitleScene(State))), "UI sample start");
			Toolbox::uint64 Frame = 0;
			Step(App, Frame);
			InstallPixelFixture(Shell(App).GetRoot());
			Render.BeforePresent = [&]()
			{
				VerifyPixels(Out / (ThreeD ? "ui-title-before-3d.png" : "ui-title-before-2d.png"));
			};
			Step(App, Frame);
			Render.BeforePresent = {};
			Click(App, Input, Frame, ThreeD ? "Start3D" : "Start2D");
			Step(App, Frame);
			State->Split.Set(true);
			Step(App, Frame);
			Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::D)] = true;
			for (Toolbox::int32 I = 0; I < 30; ++I)
			{
				Step(App, Frame);
			}
			Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::D)] = false;
			Click(App, Input, Frame, "Pause");
			Check(App.GetScenes().GetCurrent()->GetClock().IsPaused(), "pause");
			Click(App, Input, Frame, "Settings");
			State->Volume.Set(0.25);
			Step(App, Frame);
			Click(App, Input, Frame, "CloseSettings");
			Click(App, Input, Frame, "Resume");
			Check(!App.GetScenes().GetCurrent()->GetClock().IsPaused(), "resume");
			Click(App, Input, Frame, "Pause");
			Click(App, Input, Frame, "Title");
			Step(App, Frame);
			Check(dynamic_cast<DUiTitleScene*>(App.GetScenes().GetCurrent()) != nullptr, "return to title");
			App.Shutdown();
		}
		// 表示先ごとの実画素とクリックの届け先。
		RunDisplayScenario(Args[1], Out);
		Toolbox::Out << "REAL_SDK_UI_SMOKE_PASSED\n";
		return 0;
	}
	catch (const Toolbox::FException& Error)
	{
		Toolbox::Err << Error.What() << "\n";
		return 1;
	}
}
