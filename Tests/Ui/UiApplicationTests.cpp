// SPDX-License-Identifier: NOASSERTION
#include "UiRuntimeTestSupport.h"
#include "UiSampleScenes.h"
#include "UiTitleScene.h"
#include "UiPlay2DScene.h"
#include "UiPlay3DScene.h"
#include "Dxf/Application.h"
#include "Dxf/UiListView.h"
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

// 論理座標の点を押して動かして離す（スライダーのドラッグ）。
void Drag_Internal(FApplication& App, FRecordingRenderer& Backend, Toolbox::uint64& Frame, FVector2 From, FVector2 To)
{
	auto& Surface = Shell_Internal(App).GetRoot().GetSurface();
	auto& Input = Backend.Assets.GetTrace().Input;
	const auto Start = Surface.ToPixel(From);
	const auto End = Surface.ToPixel(To);
	Input.MouseX = static_cast<Toolbox::int32>(Start.X);
	Input.MouseY = static_cast<Toolbox::int32>(Start.Y);
	Step_Internal(App, Backend, Frame);
	Input.MouseButtons[0] = true;
	Step_Internal(App, Backend, Frame);
	Input.MouseX = static_cast<Toolbox::int32>(End.X);
	Input.MouseY = static_cast<Toolbox::int32>(End.Y);
	Step_Internal(App, Backend, Frame);
	Input.MouseButtons[0] = false;
	Step_Internal(App, Backend, Frame);
}

// スライダーのつまみを、範囲の割合の位置まで実際のポインター操作で動かす。
void DragSlider_Internal(FApplication& App, FRecordingRenderer& Backend, Toolbox::uint64& Frame, const char* Name,
                         Toolbox::f64 Fraction)
{
	auto* Slider = dynamic_cast<DUiSlider*>(Find_Internal(Shell_Internal(App).GetRoot(), Name));
	REQUIRE(Slider != nullptr);
	const auto Thumb = Slider->GetThumbRect();
	const auto Content = Slider->GetContentRect();
	const Toolbox::f32 Span = Content.Width - Thumb.Width;
	const FVector2 From{Thumb.X + Thumb.Width * 0.5f, Thumb.Y + Thumb.Height * 0.5f};
	const FVector2 To{Content.X + Thumb.Width * 0.5f + static_cast<Toolbox::f32>(Fraction) * Span, From.Y};
	Drag_Internal(App, Backend, Frame, From, To);
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
		// 状態を直接変えず、実際のスライダーを固定入力のドラッグで0.25へ動かす。
		DragSlider_Internal(App, Backend, Frame, "Volume", 0.25);
		const auto* Slider = dynamic_cast<DUiSlider*>(Find_Internal(Shell_Internal(App).GetRoot(), "Volume"));
		REQUIRE(Slider && Slider->GetValue() == 0.25 && State->Volume.Get() == 0.25);
		// 実際のトグルで2画面表示を切り替え、状態へ届くことを確かめて戻す。
		Click_Internal(App, Backend, Frame, "Split");
		REQUIRE(State->Split.Get());
		Click_Internal(App, Backend, Frame, "Split");
		REQUIRE(!State->Split.Get());
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
		// 一覧の3行目を実際に押すと、安定キー3が選ばれて詳細が変わる。
		auto* Items = dynamic_cast<DUiListView*>(Find_Internal(Shell_Internal(App).GetRoot(), "Items"));
		REQUIRE(Items != nullptr);
		const auto ListRect = Items->GetContentRect();
		Drag_Internal(App, Backend, Frame, {ListRect.X + 20, ListRect.Y + 28 * 2 + 14},
		              {ListRect.X + 20, ListRect.Y + 28 * 2 + 14});
		REQUIRE(Items->GetSelectedKey() && *Items->GetSelectedKey() == 3);
		REQUIRE(State->Detail.Get() == "選択キー: 3");
		// タイトルへ戻る前の確認：いいえで一時停止へ戻り、はいでタイトルへ戻る。
		Click_Internal(App, Backend, Frame, "Pause");
		Click_Internal(App, Backend, Frame, "Title");
		REQUIRE(Find_Internal(Shell_Internal(App).GetRoot(), "ConfirmNo") != nullptr);
		Click_Internal(App, Backend, Frame, "ConfirmNo");
		REQUIRE(App.GetScenes().GetCurrent()->GetClock().IsPaused());
		REQUIRE(dynamic_cast<DUiTitleScene*>(App.GetScenes().GetCurrent()) == nullptr);
		Click_Internal(App, Backend, Frame, "Title");
		Click_Internal(App, Backend, Frame, "ConfirmYes");
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

namespace
{
// 1フレームのゲームの状態（位置と固定更新の回数）。
struct FGameSample
{
	Toolbox::FVector3 Position;
	Toolbox::int64 Steps = 0;
};

FGameSample Sample_Internal(FApplication& App, bool bThreeD)
{
	if (bThreeD)
	{
		const auto& Character = dynamic_cast<DUiPlay3DScene*>(App.GetScenes().GetCurrent())->GetPlayer().GetCharacter();
		return {Character.GetCenter(), Character.GetStepCount()};
	}
	const auto& Character = dynamic_cast<DUiPlay2DScene*>(App.GetScenes().GetCurrent())->GetPlayer().GetCharacter();
	const auto XY = Character.GetCenter();
	return {{XY.X, XY.Y, 0}, Character.GetStepCount()};
}

// ゲームの時刻（止めていたフレームを除く）ごとの固定入力。25まで右、10で跳ぶ。以後は中立。
void ApplyGameInput_Internal(FRecordingRenderer& Backend, Toolbox::int32 GameFrame)
{
	Backend.Assets.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::D)] = GameFrame < 25;
	Backend.Assets.GetTrace().Input.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = GameFrame == 10;
}
} // namespace

TEST("UI sample pause and resume shift the gameplay trajectory only by the paused frames")
{
	for (bool ThreeD : {false, true})
	{
		for (bool Split : {false, true})
		{
			// 基準：UIを操作しない。
			Toolbox::TVector<FGameSample> Reference;
			{
				FRecordingRenderer Backend;
				auto State = Toolbox::MakeShared<FUiSampleState>();
				State->Split.Set(Split);
				FBackendServices Services{Backend.Assets, Backend.Assets, Backend.Assets,
				                          Backend.Assets, Backend,        Backend};
				FApplication App(Services, Settings_Internal());
				REQUIRE(App.Start(MakePlayScene(ThreeD, State)));
				Toolbox::uint64 Frame = 0;
				for (Toolbox::int32 I = 0; I < 90; ++I)
				{
					ApplyGameInput_Internal(Backend, I);
					Step_Internal(App, Backend, Frame);
					Reference.PushBack(Sample_Internal(App, ThreeD));
					// 基準は毎フレーム固定更新が進む（止まったフレームの判定の前提）。
					REQUIRE(I == 0 || Reference[static_cast<Toolbox::size_t>(I)].Steps >
					                      Reference[static_cast<Toolbox::size_t>(I - 1)].Steps);
				}
				App.Shutdown();
			}
			// 入力が中立の間に一時停止を開き、しばらく後に再開する。
			FRecordingRenderer Backend;
			auto State = Toolbox::MakeShared<FUiSampleState>();
			State->Split.Set(Split);
			FBackendServices Services{Backend.Assets, Backend.Assets, Backend.Assets, Backend.Assets, Backend, Backend};
			FApplication App(Services, Settings_Internal());
			REQUIRE(App.Start(MakePlayScene(ThreeD, State)));
			Toolbox::uint64 Frame = 0;
			Toolbox::int32 PausedFrames = 0;
			Toolbox::int32 GameFrame = 0;
			FGameSample Last;
			// 一時停止の画面を開いてから、再開の押下の次のフレームまでだけ、止まったフレームを認める。
			bool bPauseWindow = false;
			auto Check = [&]()
			{
				const FGameSample Now = Sample_Internal(App, ThreeD);
				if (GameFrame > 0 && Now.Steps == Last.Steps)
				{
					// 固定更新が進まないフレームは、位置も変えない。
					REQUIRE(bPauseWindow);
					REQUIRE(Now.Position.X == Last.Position.X && Now.Position.Y == Last.Position.Y &&
					        Now.Position.Z == Last.Position.Z);
					++PausedFrames;
				}
				else
				{
					// 進んだフレームは、止めていたフレームの分だけずれた基準と完全に一致する。
					const auto& Expected = Reference[static_cast<Toolbox::size_t>(GameFrame)];
					REQUIRE(Now.Steps == Expected.Steps && Now.Position.X == Expected.Position.X &&
					        Now.Position.Y == Expected.Position.Y && Now.Position.Z == Expected.Position.Z);
					++GameFrame;
				}
				Last = Now;
			};
			while (GameFrame < 45)
			{
				ApplyGameInput_Internal(Backend, GameFrame);
				Step_Internal(App, Backend, Frame);
				Check();
			}
			// マウスの操作はゲーム入力へ割り当てていない（押下はUIだけが受ける）。
			auto ClickChecked = [&](const char* Name)
			{
				auto& Shell = Shell_Internal(App);
				auto* Element = Find_Internal(Shell.GetRoot(), Name);
				REQUIRE(Element != nullptr);
				const auto Rect = Element->GetRect();
				const auto Point = Shell.GetRoot().GetSurface().ToPixel(
				    FVector2{Rect.X + Rect.Width * 0.5f, Rect.Y + Rect.Height * 0.5f});
				auto& Input = Backend.Assets.GetTrace().Input;
				Input.MouseX = static_cast<Toolbox::int32>(Point.X);
				Input.MouseY = static_cast<Toolbox::int32>(Point.Y);
				for (bool Down : {true, false})
				{
					Input.MouseButtons[0] = Down;
					ApplyGameInput_Internal(Backend, GameFrame);
					Step_Internal(App, Backend, Frame);
					Check();
				}
			};
			bPauseWindow = true;
			ClickChecked("Pause");
			for (Toolbox::int32 I = 0; I < 20; ++I)
			{
				ApplyGameInput_Internal(Backend, GameFrame);
				Step_Internal(App, Backend, Frame);
				Check();
			}
			ClickChecked("Resume");
			ApplyGameInput_Internal(Backend, GameFrame);
			Step_Internal(App, Backend, Frame);
			Check();
			bPauseWindow = false;
			while (GameFrame < 90)
			{
				ApplyGameInput_Internal(Backend, GameFrame);
				Step_Internal(App, Backend, Frame);
				Check();
			}
			REQUIRE(PausedFrames >= 20);
			App.Shutdown();
		}
	}
}
