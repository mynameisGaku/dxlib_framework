// SPDX-License-Identifier: NOASSERTION
#include "UiRuntimeTestSupport.h"
#include "UiSampleScenes.h"
#include "UiTitleScene.h"
#include "UiPlay2DScene.h"
#include "UiPlay3DScene.h"
#include "Dxf/Application.h"
#include "Dxf/UiSlider.h"
using namespace Dxf;
using namespace Dxf::UiSample;
using namespace UiTest;
namespace
{
// 名前検索は自動操作用だけ。ゲームの画面組立ては型付き参照を保持している。
DUiElement* Find_Internal(DUiElement& Parent, const char* Name)
{
	if (Parent.GetName() == Name && Parent.IsAttached())
	{
		return &Parent;
	}
	for (Toolbox::size_t I = 0; I < Parent.GetChildCount(); ++I)
	{
		if (auto* Found = Find_Internal(*Parent.GetChild(I), Name))
		{
			return Found;
		}
	}
	return nullptr;
}

DUiElement* Find_Internal(FUiRoot& Root, const char* Name)
{
	if (auto* Modal = Root.GetTopModal())
	{
		return Find_Internal(*Modal, Name);
	}
	for (Toolbox::size_t I = 0; I < static_cast<Toolbox::size_t>(EUiLayer::Count); ++I)
	{
		if (auto* Found = Find_Internal(Root.GetLayer(static_cast<EUiLayer>(I)), Name))
		{
			return Found;
		}
	}
	return nullptr;
}

FUiSampleShell& Shell_Internal(FApplication& App)
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
	return dynamic_cast<DUiPlay3DScene*>(Scene)->GetUi();
}

void Step_Internal(FApplication& App, FRecordingRenderer& Backend, Toolbox::uint64& Frame)
{
	const auto Result = App.Step(static_cast<Toolbox::f64>(Frame++) / 60.0);
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	REQUIRE(Result.Value());
	Backend.Rectangles.Clear();
	Backend.Quads.Clear();
	Backend.Views.Clear();
}

void Click_Internal(FApplication& App, FRecordingRenderer& Backend, Toolbox::uint64& Frame, const char* Name)
{
	auto& Shell = Shell_Internal(App);
	auto* Element = Find_Internal(Shell.GetRoot(), Name);
	REQUIRE(Element != nullptr);
	const auto Rect = Element->GetRect();
	const auto Point =
	    Shell.GetRoot().GetSurface().ToPixel(FVector2{Rect.X + Rect.Width * 0.5f, Rect.Y + Rect.Height * 0.5f});
	Backend.Assets.GetTrace().Input.MouseX = static_cast<Toolbox::int32>(Point.X);
	Backend.Assets.GetTrace().Input.MouseY = static_cast<Toolbox::int32>(Point.Y);
	Backend.Assets.GetTrace().Input.MouseButtons[0] = true;
	Step_Internal(App, Backend, Frame);
	Backend.Assets.GetTrace().Input.MouseButtons[0] = false;
	Step_Internal(App, Backend, Frame);
}

FApplicationSettings Settings_Internal()
{
	FApplicationSettings Settings;
	Settings.Window.Width = 1280;
	Settings.Window.Height = 720;
	Settings.ExecutionThreadCount = 1;
	return Settings;
}
} // namespace

TEST("UI sample uses real Application and 2D 3D gameplay for title pause settings resume and return")
{
	for (bool ThreeD : {false, true})
	{
		FRecordingRenderer Backend;
		auto State = Toolbox::MakeShared<FUiSampleState>();
		FBackendServices Services{Backend.Assets, Backend.Assets, Backend.Assets, Backend.Assets, Backend, Backend};
		FApplication App(Services, Settings_Internal());
		REQUIRE(App.Start(MakeTitleScene(State)));
		Toolbox::uint64 Frame = 0;
		Step_Internal(App, Backend, Frame);
		Click_Internal(App, Backend, Frame, ThreeD ? "Start3D" : "Start2D");
		Step_Internal(App, Backend, Frame);
		REQUIRE(ThreeD ? dynamic_cast<DUiPlay3DScene*>(App.GetScenes().GetCurrent()) != nullptr
		               : dynamic_cast<DUiPlay2DScene*>(App.GetScenes().GetCurrent()) != nullptr);
		auto Count = [&]() -> Toolbox::int64
		{
			if (ThreeD)
			{
				return dynamic_cast<DUiPlay3DScene*>(App.GetScenes().GetCurrent())
				    ->GetPlayer()
				    .GetCharacter()
				    .GetStepCount();
			}
			return dynamic_cast<DUiPlay2DScene*>(App.GetScenes().GetCurrent())
			    ->GetPlayer()
			    .GetCharacter()
			    .GetStepCount();
		};
		Click_Internal(App, Backend, Frame, "Pause");
		REQUIRE(App.GetScenes().GetCurrent()->GetClock().IsPaused());
		const auto Paused = Count();
		for (Toolbox::int32 I = 0; I < 10; ++I)
		{
			Step_Internal(App, Backend, Frame);
		}
		REQUIRE(Count() == Paused);
		Click_Internal(App, Backend, Frame, "Settings");
		REQUIRE(Find_Internal(Shell_Internal(App).GetRoot(), "Volume") != nullptr);
		State->Volume.Set(0.25);
		Step_Internal(App, Backend, Frame);
		const auto* Slider = dynamic_cast<DUiSlider*>(Find_Internal(Shell_Internal(App).GetRoot(), "Volume"));
		REQUIRE(Slider && Slider->GetValue() == 0.25);
		Click_Internal(App, Backend, Frame, "Sound");
		bool VolumeApplied = false;
		for (const auto& Entry : Backend.Assets.GetTrace().Volumes)
		{
			VolumeApplied = VolumeApplied || Entry.Second == 0.25f;
		}
		REQUIRE(VolumeApplied);
		Click_Internal(App, Backend, Frame, "CloseSettings");
		REQUIRE(App.GetScenes().GetCurrent()->GetClock().IsPaused());
		Click_Internal(App, Backend, Frame, "Resume");
		REQUIRE(!App.GetScenes().GetCurrent()->GetClock().IsPaused());
		for (Toolbox::int32 I = 0; I < 3; ++I)
		{
			Step_Internal(App, Backend, Frame);
		}
		REQUIRE(Count() > Paused);
		Click_Internal(App, Backend, Frame, "Pause");
		Click_Internal(App, Backend, Frame, "Title");
		Step_Internal(App, Backend, Frame);
		REQUIRE(dynamic_cast<DUiTitleScene*>(App.GetScenes().GetCurrent()) != nullptr);
		App.Shutdown();
		REQUIRE(Backend.Assets.GetTrace().Textures.IsEmpty() && Backend.Assets.GetTrace().Fonts.IsEmpty());
	}
}

TEST("UI sample gameplay trajectory matches without routing and with one or two displays")
{
	for (bool ThreeD : {false, true})
	{
		Toolbox::TVector<Toolbox::FVector3> Reference;
		for (Toolbox::int32 Variant = 0; Variant < 3; ++Variant)
		{
			FRecordingRenderer Backend;
			auto State = Toolbox::MakeShared<FUiSampleState>();
			State->Split.Set(Variant == 2);
			FBackendServices Services{Backend.Assets, Backend.Assets, Backend.Assets, Backend.Assets, Backend, Backend};
			FApplication App(Services, Settings_Internal());
			REQUIRE(App.Start(MakePlayScene(ThreeD, State)));
			if (Variant == 0)
			{
				App.GetScenes().GetCurrent()->SetInputRouter(nullptr);
			}
			Toolbox::uint64 Frame = 0;
			for (Toolbox::int32 I = 0; I < 90; ++I)
			{
				Backend.Assets.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::D)] = I < 50;
				Backend.Assets.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = I == 20;
				Step_Internal(App, Backend, Frame);
				Toolbox::FVector3 Position;
				if (ThreeD)
				{
					Position = dynamic_cast<DUiPlay3DScene*>(App.GetScenes().GetCurrent())
					               ->GetPlayer()
					               .GetCharacter()
					               .GetCenter();
				}
				else
				{
					const auto XY = dynamic_cast<DUiPlay2DScene*>(App.GetScenes().GetCurrent())
					                    ->GetPlayer()
					                    .GetCharacter()
					                    .GetCenter();
					Position = {XY.X, XY.Y, 0};
				}
				if (Variant == 0)
				{
					Reference.PushBack(Position);
				}
				else
				{
					REQUIRE(Position.X == Reference[static_cast<Toolbox::size_t>(I)].X &&
					        Position.Y == Reference[static_cast<Toolbox::size_t>(I)].Y &&
					        Position.Z == Reference[static_cast<Toolbox::size_t>(I)].Z);
				}
			}
			App.Shutdown();
		}
	}
}
