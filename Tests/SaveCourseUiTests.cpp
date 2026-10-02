// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Support/ContentRenderBackend.h"
#include "Support/ContentModelBackend.h"
#include "Ui/UiRuntimeTestSupport.h"
#include "SaveCourseGameInstance.h"
#include "SaveCourseScene2D.h"
#include "SaveCourseScene3D.h"
#include "InteractionScene2D.h"
#include "InteractionScene3D.h"
#include "Dxf/Application.h"
#include "Dxf/SaveStore.h"
#include "Toolbox/JsonWriter.h"
using namespace Dxf;
using namespace Dxf::GameplaySample;
using namespace Toolbox;
namespace
{
// 画面の文字を基準に実クリックする。UI privateやイベントを直接呼ばない。
template <typename TScene>
void RunUi(uint32 Dimension)
{
	const FPath Parent(DXF_SAVE_COURSE_TEST_ROOT);
	(void)CreateDirectory(Parent);
	const FPath Root = Parent / FPath(FString("Ui") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	(void)CreateDirectory(Root);
	Testing::FFakeBackend Backend;
	Testing::FContentRenderBackend Renderer(Backend);
	Testing::FContentModelBackend Models;
	UiTest::FRecordingRenderer Fonts;
	Backend.GetTrace().Window.bKnown = true;
	Backend.GetTrace().Window.ClientWidth = Backend.GetTrace().Window.RenderWidth = 1001;
	Backend.GetTrace().Window.ClientHeight = Backend.GetTrace().Window.RenderHeight = 501;
	FApplicationSettings Settings;
	Settings.ProjectRoot = DXF_CONTENT_SAMPLE_ROOT;
	Settings.ExecutionThreadCount = 1;
	auto Owner = MakeUnique<ASaveCourseGameInstance>(Root, FSceneContentSource{FPath(DXF_CONTENT_SAMPLE_ROOT)}, false);
	auto* Game = Owner.Get();
	FApplication App({Backend, Backend, Backend, Backend, Fonts, Renderer, &Models}, Settings, Move(Owner));
	if constexpr (IsSame<TScene, ASaveCourseScene2D>)
	{
		REQUIRE(App.Start(MakeUnique<DInteraction2DScene>()));
	}
	else
	{
		REQUIRE(App.Start(MakeUnique<DInteraction3DScene>()));
	}
	REQUIRE(App.Step(0));
	f64 Now = 0;
	auto Tick = [&](f64 Seconds = 1.0 / 60.0)
	{
		Renderer.Texts.Clear();
		Renderer.Views.Clear();
		Renderer.Sprites.Clear();
		Renderer.Geometry.Clear();
		Renderer.Triangles.Clear();
		Renderer.Lines.Clear();
		Renderer.Circles.Clear();
		Renderer.ModelHandles.Clear();
		Renderer.ModelTimes.Clear();
		Now += Seconds;
		const auto Result = App.Step(Now);
		if (!Result)
		{
			throw FException(Result.Error().Message);
		}
		REQUIRE(Result.Value());
	};
	auto& Input = Backend.GetTrace().Input;
	auto Press = [&](EKey Key)
	{
		Input.Keys[static_cast<size_t>(Key)] = true;
		Tick();
		Input.Keys[static_cast<size_t>(Key)] = false;
		Tick();
	};
	// 描画とhit testが同じ倍率を使うか、画素位置をクリックして確認する。
	auto Locate = [&](const char* Label, f32 Below = 12)
	{
		for (uint32 Attempt = 0; Attempt < 60; ++Attempt)
		{
			TOptional<FTextCommand> Text;
			for (const auto& Draw : Renderer.Texts)
			{
				if (Draw.Text == Label)
				{
					Text = Draw;
					break;
				}
			}
			if (!Text)
			{
				FString Seen;
				for (const auto& Draw : Renderer.Texts)
				{
					Seen += " [" + Draw.Text.Get() + "]";
				}
				throw FException(FString("UI label not drawn: ") + Label + Seen);
			}
			const auto& Clip = Text->Options.ClipRect;
			if (Attempt >= 54)
			{
				Out << "UI_SCROLL label=" << Label << " y=" << static_cast<f64>(Text->Position.Y) << " clip=" << Clip.Top << "," << Clip.Bottom << " scale=" << static_cast<f64>(Game->GetSettings().UiScale) << '\n';
			}
			// 上端と一致する文字は全体が領域内。不要な余白条件でWheelを往復させない。
			if (Text->Position.Y >= Clip.Top && Text->Position.Y + Below < Clip.Bottom - 2)
			{
				return *Text;
			}
			// 倍率変更後も現在の実描画領域へWheelを送る。固定画素の前提を置かない。
			Input.MouseX = (Clip.Left + Clip.Right) / 2;
			Input.MouseY = (Clip.Top + Clip.Bottom) / 2;
			Input.Wheel = Text->Position.Y < Clip.Top ? 1 : -1;
			Tick(.05);
			Input.Wheel = 0;
			Tick(.05);
		}
		throw FException(FString("UI label not reachable by scroll: ") + Label);
	};
	auto Click = [&](const char* Label)
	{
		const auto Text = Locate(Label);
		Input.MouseX = static_cast<int32>(Text.Position.X) + 3;
		Input.MouseY = static_cast<int32>(Text.Position.Y) + 3;
		Input.MouseButtons[static_cast<size_t>(EMouseButton::Left)] = true;
		Tick();
		Input.MouseButtons[static_cast<size_t>(EMouseButton::Left)] = false;
		Tick();
	};
	// 通常の旧コースの入口から実クリックする。空のContinueでも旧SceneとNewを残す。
	const auto* OldScene = App.GetScenes().GetCurrent();
	Press(EKey::F1);
	Click(Dimension == 2 ? "Continue checkpoint 2D" : "Continue checkpoint 3D");
	Tick(0);
	Tick(0);
	if (Game->GetSaveTicket().GetState() != EGameSaveRequestState::Failed)
	{
		throw FException(FString("Continue click state=") + ToString(static_cast<uint32>(Game->GetSaveTicket().GetState())) + " reason=" + Game->GetDiagnostic());
	}
	REQUIRE(App.GetScenes().GetCurrent() == OldScene);
	bool ErrorDisplayed = false;
	const auto Error = "Continue失敗: " + Game->GetSaveTicket().GetReason();
	for (const auto& Draw : Renderer.Texts)
	{
		ErrorDisplayed = ErrorDisplayed || Draw.Text == Error;
	}
	REQUIRE(ErrorDisplayed);
	Click(Dimension == 2 ? "New checkpoint game 2D" : "New checkpoint game 3D");
	Tick(0);
	Tick(.02);
	Tick();
	auto* Current = App.GetScenes().GetCurrent()->TryCast<TScene>();
	REQUIRE(Current != nullptr && Current->GetResumeTicket().GetState() == ECheckpointSceneState::Ready);
	const auto Body = Current->GetPrefab("player").Get()->GetRigidBody("body").Get()->GetBodyId();
	// 設定は保存からの正準化で並び順が変わる場合も想定する。
	auto Changed = FSaveCourseInput::Defaults();
	Changed.UiScale = 1.5f;
	Game->RequestSettings(Changed);
	Tick();
	Press(EKey::F1);
	REQUIRE(Current->GetClock().IsPaused());
	const auto Steps = Current->GetCompletedPhysicsStepCount();
	Click("Toggle 1 / 2 views");
	Tick();
	REQUIRE(Current->GetCompletedPhysicsStepCount() == Steps);
	if constexpr (IsSame<TScene, ASaveCourseScene3D>)
	{
		REQUIRE(Renderer.Views.Size() == 2);
	}
	Click("Save（選択したスロット）");
	Tick(0);
	Tick(0);
	REQUIRE(Game->GetSaveTicket().GetState() == EGameSaveRequestState::Committed);
	FSaveStore Store(Root);
	REQUIRE(Store.Load("manual1").Document->Generation == 1);
	Tick(0);
	REQUIRE(Game->GetSlots()[0].Status == "正常");
	Click("Save（選択したスロット）");
	Click("取消す");
	REQUIRE(Store.Load("manual1").Document->Generation == 1);
	REQUIRE(Current->GetCompletedPhysicsStepCount() == Steps);
	// Keyboard採取後の押し続けは、閉鎖Frameと次の固定更新にも漏れない。
	Click("Keyboardを採取");
	Input.Keys[static_cast<size_t>(EKey::K)] = true;
	Tick(0);
	Tick(0);
	bool Keyboard = false;
	for (const auto& Action : Game->GetSettings().Input.Actions)
	{
		if (Action.Action == "Jump")
		{
			Keyboard = Action.Bindings.Size() == 1 && Action.Bindings[0].Key == EKey::K;
		}
	}
	REQUIRE(Keyboard);
	// 別ActionのDへ競合する候補は全体拒否し、現在のJump=Kを保つ。
	Input.Keys[static_cast<size_t>(EKey::K)] = false;
	Tick(0);
	const auto SettingsRevision = Game->GetSettingsRevision();
	Click("Keyboardを採取");
	Input.Keys[static_cast<size_t>(EKey::D)] = true;
	Tick(0);
	Tick(0);
	REQUIRE(Game->GetSettingsRevision() == SettingsRevision);
	Input.Keys[static_cast<size_t>(EKey::D)] = false;
	Tick(0);
	Input.Keys[static_cast<size_t>(EKey::K)] = true;
	Tick(0);
	Press(EKey::F1);
	Tick(.06);
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	Input.Keys[static_cast<size_t>(EKey::K)] = false;
	Tick(0);
	Input.Keys[static_cast<size_t>(EKey::K)] = true;
	Tick(.06);
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y - 2) < .02);
	Input.Keys[static_cast<size_t>(EKey::K)] = false;
	Tick();
	Current->GetPhysicsWorld().SetVelocity(Body, {});
	Press(EKey::F1);
	// 採取開始の左クリックをMouseの新しい割当へ誤採用しない。
	Click("Mouseを採取");
	Tick(0);
	Input.MouseButtons[static_cast<size_t>(EMouseButton::Right)] = true;
	Tick(0);
	Tick(0);
	bool Mouse = false;
	for (const auto& Action : Game->GetSettings().Input.Actions)
	{
		if (Action.Action == "Jump")
		{
			Mouse = Action.Bindings[0].Device == EInputBindingDevice::Mouse && Action.Bindings[0].MouseButton == EMouseButton::Right;
		}
	}
	REQUIRE(Mouse);
	Press(EKey::F1);
	Tick();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	Input.MouseButtons[static_cast<size_t>(EMouseButton::Right)] = false;
	Tick(0);
	Input.MouseButtons[static_cast<size_t>(EMouseButton::Right)] = true;
	Tick();
	REQUIRE(Current->GetPhysicsWorld().GetVelocity(Body).Y > 1.9f);
	Input.MouseButtons[static_cast<size_t>(EMouseButton::Right)] = false;
	Tick();
	Current->GetPhysicsWorld().SetVelocity(Body, {});
	Input.Pads[3].bConnected = true;
	Press(EKey::F1);
	Click("Pad 4 を採取");
	Tick(0);
	Input.Pads[3].Buttons[15] = true;
	Tick(0);
	Tick(0);
	bool Pad = false;
	for (const auto& Action : Game->GetSettings().Input.Actions)
	{
		if (Action.Action == "Jump")
		{
			Pad = Action.Bindings[0].Device == EInputBindingDevice::Gamepad && Action.Bindings[0].PadSlot == 3 && Action.Bindings[0].PadButton == 15;
		}
	}
	REQUIRE(Pad);
	// 文字の直下に描かれる既存Sliderへ実入力を送り、音量と倍率を反映する。
	auto FocusSlider = [&](const char* Label)
	{
		const auto Text = Locate(Label, 80);
		const f32 Scale = Clamp(501.0f / 720 * Game->GetSettings().UiScale, .25f, 8.0f);
		const auto Height = Fonts.GetFontLineHeight(Text.Font.GetNativeHandle_Internal());
		REQUIRE(Height);
		Input.MouseX = static_cast<int32>(Text.Position.X + 32 * Scale);
		Input.MouseY = static_cast<int32>(Text.Position.Y + Height.Value() + 18 * Scale);
		Input.MouseButtons[static_cast<size_t>(EMouseButton::Left)] = true;
		Tick();
		Input.MouseButtons[static_cast<size_t>(EMouseButton::Left)] = false;
		Tick();
		Tick(0);
	};
	FocusSlider("音量（適用と保存は別）");
	const auto BeforeVolume = Game->GetSettings().Volume;
	Press(EKey::Right);
	Tick(0);
	REQUIRE(Abs(Game->GetSettings().Volume - BeforeVolume - .05f) < .001);
	FocusSlider("UI倍率（描画とクリック判定）");
	const auto BeforeScale = Game->GetSettings().UiScale;
	Press(EKey::Right);
	Tick(0);
	REQUIRE(Abs(Game->GetSettings().UiScale - BeforeScale - .25f) < .001);
	const auto SavedScale = Game->GetSettings().UiScale;
	Click("設定を保存／保存失敗の再試行");
	Tick(0);
	Tick(0);
	REQUIRE(!Game->IsSettingsDirty());
	REQUIRE(Store.Load("settings").Document->Kind == ESaveDocumentKind::Settings);
	Click("Spawn extra instance");
	REQUIRE(Current->GetProgress().SpawnCounter == 0);
	Press(EKey::F1);
	Tick();
	REQUIRE(Current->GetProgress().SpawnCounter == 1);
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(Body).Y) < .001);
	Input.Pads[3].Buttons[15] = false;
	Tick(0);
	Input.Pads[3].Buttons[15] = true;
	Tick();
	REQUIRE(Current->GetPhysicsWorld().GetVelocity(Body).Y > 1.9f);
	Input.Pads[3].Buttons[15] = false;
	Tick();
	Press(EKey::F1);
	const auto Cargo = Current->GetPrefab("cargo1");
	Click("Destroy selected instance");
	Press(EKey::F1);
	Tick();
	REQUIRE(!Cargo && Current->GetProgress().SpawnCounter == 1);
	// Continue／Loadを選ぶ入力は、別のUI／Action Mapを持つ新Sceneにも漏らさない。
	Press(EKey::F1);
	Input.Pads[3].Buttons[15] = true;
	Click("Load（選択したスロット）");
	for (uint32 Attempt = 0; Attempt < 12 && Game->GetSaveTicket().GetState() != EGameSaveRequestState::Completed; ++Attempt)
	{
		Tick();
	}
	REQUIRE(Game->GetSaveTicket().GetState() == EGameSaveRequestState::Completed);
	Current = App.GetScenes().GetCurrent()->TryCast<TScene>();
	REQUIRE(Current != nullptr && Current->GetProgress().SpawnCounter == 0);
	const auto NewBody = Current->GetPrefab("player").Get()->GetRigidBody("body").Get()->GetBodyId();
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(NewBody).Y) < .001);
	REQUIRE(!Game->IsSettingsDirty() && Game->GetSettings().UiScale == SavedScale);
	Input.Pads[3].Buttons[15] = false;
	Tick(0);
	Input.Pads[3].Buttons[15] = true;
	Tick(.06);
	REQUIRE(Abs(Current->GetPhysicsWorld().GetVelocity(NewBody).Y - 2) < .02);
	App.GetScenes().RequestQuit();
	const auto Quit = App.Step(Now);
	REQUIRE(Quit && !Quit.Value());
}
} // namespace
TEST("Save course 2D scaled UI saves and rebinds without held input leaking")
{
	RunUi<ASaveCourseScene2D>(2);
}
TEST("Save course 3D scaled UI saves and rebinds without held input leaking")
{
	RunUi<ASaveCourseScene3D>(3);
}
