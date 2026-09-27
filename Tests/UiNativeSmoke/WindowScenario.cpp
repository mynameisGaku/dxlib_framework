// SPDX-License-Identifier: NOASSERTION
// ウィンドウの拡縮・最小化の実DxLibでの確認。期待値は既知の色・描画先の寸法・元画像から求める。
#include "WindowScenario.h"
#include "WindowScene.h"
#include "FixedInput.h"
#include "ForwardRenderer.h"
#include "ScreenCapture.h"
#include "Dxf/Application.h"
#include "Dxf/NativeBackends.h"
#include "DxLib.h"
namespace Dxf::UiSmoke
{
namespace
{
void Require_Internal(bool Value, const char* Error)
{
	if (!Value)
	{
		throw Toolbox::FException(Error);
	}
}

bool SameRgb_Internal(FColor A, FColor B)
{
	return A.R == B.R && A.G == B.G && A.B == B.B;
}

void Step_Internal(FApplication& App, Toolbox::uint64& Frame)
{
	const auto Result = App.Step(static_cast<Toolbox::f64>(Frame++) / 60.0);
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	Require_Internal(Result.Value(), "window scene ended before scenario completed");
}

// 画面の右下の端・実画像・文字の画素を、描画先の寸法から確かめる。
void VerifyFrame_Internal(const FScreenCapture& Capture, const char* ProjectRoot, Toolbox::int32 Width,
                          Toolbox::int32 Height)
{
	using S = DWindowScene;
	Require_Internal(Capture.GetWidth() == Width && Capture.GetHeight() == Height, "captured render size");
	Require_Internal(SameRgb_Internal(Capture.Pixel(Width - 1, Height - 1), S::ButtonColor),
	                 "UI reaches the new right bottom edge");
	Require_Internal(SameRgb_Internal(Capture.Pixel(Width / 2, Height - 5), S::ButtonColor), "UI fills the new width");
	// 画面モードの変更の前に読んだ画像が、同じ内容で描かれる（元画像の(20,20)と(40,12)）。
	const Toolbox::FPath Path = Toolbox::FPath(ProjectRoot) / "Assets/player.bmp";
	const Toolbox::int32 Source = DxLib::LoadSoftImage(Path.ToUtf8().CStr());
	Require_Internal(Source >= 0, "source image load");
	const Toolbox::TArray<Toolbox::TArray<Toolbox::int32, 2>, 2> Points{Toolbox::TArray<Toolbox::int32, 2>{20, 20},
	                                                                    Toolbox::TArray<Toolbox::int32, 2>{40, 12}};
	for (const auto& Point : Points)
	{
		int R = 0;
		int G = 0;
		int B = 0;
		int A = 0;
		(void)DxLib::GetPixelSoftImage(Source, Point[0], Point[1], &R, &G, &B, &A);
		const FColor Actual = Capture.Pixel(S::ImageRect.Left + Point[0], S::ImageRect.Top + Point[1]);
		Require_Internal(Toolbox::Abs(Actual.R - R) <= 2 && Toolbox::Abs(Actual.G - G) <= 2 &&
		                     Toolbox::Abs(Actual.B - B) <= 2,
		                 "image loaded before the mode change keeps its pixels");
	}
	(void)DxLib::DeleteSoftImage(Source);
	Toolbox::int32 White = 0;
	for (Toolbox::int32 Y = 22; Y < 58; ++Y)
	{
		for (Toolbox::int32 X = 102; X < 200; ++X)
		{
			const FColor C = Capture.Pixel(X, Y);
			White += C.R > 200 && C.G > 200 && C.B > 200 ? 1 : 0;
		}
	}
	Require_Internal(White > 10, "font keeps drawing after the mode change");
}

// 自アプリのウィンドウのクライアント領域を指定の寸法へ変え、数フレーム進める。
void ResizeClient_Internal(FApplication& App, Toolbox::uint64& Frame, Toolbox::int32 Width, Toolbox::int32 Height)
{
	Require_Internal(DxLib::SetWindowSize(Width, Height) == 0, "window client resize");
	for (Toolbox::int32 I = 0; I < 3; ++I)
	{
		Step_Internal(App, Frame);
	}
}

void RunMode_Internal(const char* ProjectRoot, const Toolbox::FPath& Out, EWindowResizeMode Mode)
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
	Settings.Window.Resize = Mode;
	Settings.ExecutionThreadCount = 1;
	Settings.ProjectRoot = ProjectRoot;
	Settings.ClearColor = FColor{20, 20, 20, 255};
	FApplication App(Services, Settings);
	auto Owned = Toolbox::MakeUnique<DWindowScene>();
	DWindowScene* Scene = Owned.Get();
	Require_Internal(static_cast<bool>(App.Start(Toolbox::Move(Owned))), "window scene start");
	Toolbox::uint64 Frame = 0;
	Step_Internal(App, Frame);
	FScreenCapture Capture;
	Render.BeforePresent = [&]()
	{
		Capture.Capture();
	};
	Step_Internal(App, Frame);
	VerifyFrame_Internal(Capture, ProjectRoot, 1280, 720);
	Require_Internal(Scene->Window.bKnown && Scene->Window.RenderWidth == 1280, "initial window state");
	// manifestで宣言したPer-Monitor V2のDPI認識で動いており、報告するDPIはOSのウィンドウのDPIと同じ。
	Require_Internal(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),
	                                              DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE,
	                 "process runs per-monitor v2 DPI aware");
	Require_Internal(Scene->Window.Dpi == static_cast<Toolbox::int32>(GetDpiForWindow(DxLib::GetMainWindowHandle())),
	                 "reported DPI equals the window DPI");
	// 奇数の寸法へ変える。Resizableは描画先を同じ画素数へ、Stretchは描画先を保って表示を引き伸ばす。
	ResizeClient_Internal(App, Frame, 1001, 501);
	const bool bResizable = Mode == EWindowResizeMode::Resizable;
	const Toolbox::int32 Width = bResizable ? 1001 : 1280;
	const Toolbox::int32 Height = bResizable ? 501 : 720;
	Require_Internal(Scene->Window.ClientWidth == 1001 && Scene->Window.ClientHeight == 501,
	                 "client size after resize");
	Require_Internal(Scene->Window.RenderWidth == Width && Scene->Window.RenderHeight == Height,
	                 "render size after resize");
	VerifyFrame_Internal(Capture, ProjectRoot, Width, Height);
	Capture.Save(Out / (bResizable ? "ui-window-resizable.png" : "ui-window-stretch.png"));
	// 描画先の画素の右下の端のクリックは、新しい寸法の配置で届く。
	Render.BeforePresent = {};
	Input.Raw.MouseX = Width - 3;
	Input.Raw.MouseY = Height - 3;
	Step_Internal(App, Frame);
	Input.Raw.MouseButtons[0] = true;
	Step_Internal(App, Frame);
	Input.Raw.MouseButtons[0] = false;
	Step_Internal(App, Frame);
	Require_Internal(Scene->Clicks == 1, "click at the new edge");
	// UIが押下を捕捉している間だけ、自アプリのウィンドウがOSのマウスの捕捉を持つ。ホバー中は操作できる対象のカーソル。
	const HWND Owner = DxLib::GetMainWindowHandle();
	Step_Internal(App, Frame);
	Require_Internal(GetCursor() == LoadCursorW(nullptr, MAKEINTRESOURCEW(32649)), "hand cursor over the button");
	if (GetForegroundWindow() == Owner)
	{
		Input.Raw.MouseButtons[0] = true;
		Step_Internal(App, Frame);
		Step_Internal(App, Frame);
		Require_Internal(GetCapture() == Owner && Scene->Window.bPointerCaptured, "OS pointer capture while pressed");
		Input.Raw.MouseButtons[0] = false;
		Step_Internal(App, Frame);
		Require_Internal(GetCapture() != Owner, "OS pointer capture released");
		Require_Internal(Scene->Clicks == 2, "click with OS capture");
		Toolbox::Out << "OS_POINTER_CAPTURE_VERIFIED\n";
		Toolbox::Out << "DXF_CHECK os_pointer_capture." << (bResizable ? "resizable" : "stretch") << "=verified\n";
	}
	else
	{
		// 前面でないウィンドウは捕捉を取得しない（強制的に前面へ移さない）。この実行では確かめていないことを残す。
		Toolbox::Out << "OS_POINTER_CAPTURE_NOT_VERIFIED_WINDOW_NOT_FOREGROUND\n";
		Toolbox::Out << "DXF_CHECK os_pointer_capture." << (bResizable ? "resizable" : "stretch") << "=not_exercised\n";
	}
	// 最小化の間は描かず、更新は続ける（既定）。復帰後は同じ寸法で描く。
	const HWND Window = DxLib::GetMainWindowHandle();
	(void)ShowWindow(Window, SW_MINIMIZE);
	Step_Internal(App, Frame);
	const Toolbox::int32 DrawsBefore = Scene->Draws;
	const Toolbox::int32 TicksBefore = Scene->Ticks;
	for (Toolbox::int32 I = 0; I < 5; ++I)
	{
		Step_Internal(App, Frame);
	}
	Require_Internal(Scene->Window.bMinimized && Scene->Draws == DrawsBefore, "no drawing while minimized");
	Require_Internal(Scene->Ticks == TicksBefore + 5, "updates continue while minimized");
	(void)ShowWindow(Window, SW_RESTORE);
	Render.BeforePresent = [&]()
	{
		Capture.Capture();
	};
	for (Toolbox::int32 I = 0; I < 3; ++I)
	{
		Step_Internal(App, Frame);
	}
	Require_Internal(!Scene->Window.bMinimized && Scene->Draws > DrawsBefore, "drawing resumes after restore");
	VerifyFrame_Internal(Capture, ProjectRoot, Width, Height);
	Render.BeforePresent = {};
	App.Shutdown();
}
} // namespace

void RunWindowScenario(const char* ProjectRoot, const Toolbox::FPath& Out)
{
	RunMode_Internal(ProjectRoot, Out, EWindowResizeMode::Resizable);
	RunMode_Internal(ProjectRoot, Out, EWindowResizeMode::Stretch);
}
} // namespace Dxf::UiSmoke
