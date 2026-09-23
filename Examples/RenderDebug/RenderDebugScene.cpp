// SPDX-License-Identifier: NOASSERTION
#include "RenderDebugScene.h"
#include "TransparencyDemo.h"
#include "Dxf/AssetService.h"
#include "Dxf/ViewCoordinates.h"
#include <stdio.h>
#include "Dxf/RenderContext.h"
#include "Dxf/SceneNavigator.h"
#include "Toolbox/Log.h"
#include "Toolbox/Platform.h"
namespace Dxf::RenderDebug
{
namespace
{
void Require_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
// 採取の成否だけを確認する。無効中の未採取は成功として扱う。
void Require_Internal(const TResult<bool>& Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
// 2D観察の最小例。床・傾いた箱・円を実FPhysicsWorld2Dへ登録する。
Toolbox::TUniquePtr<FPhysicsWorld2D> MakeWorld2D_Internal()
{
	auto World = Toolbox::MakeUnique<FPhysicsWorld2D>();
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	Toolbox::FOrientedBox2D Floor;
	Floor.HalfExtents = {3, 0.25f};
	FColliderDescription2D FloorCollider;
	FloorCollider.Shape = Floor;
	World->AttachCollider(World->CreateBody(Ground), FloorCollider);
	FBodyDescription2D Crate;
	Crate.Position = {-1, 4};
	Crate.Angle = 0.3f;
	FColliderDescription2D CrateCollider;
	CrateCollider.Shape = Toolbox::FOrientedBox2D{};
	CrateCollider.Friction = 0.6f;
	World->AttachCollider(World->CreateBody(Crate), CrateCollider);
	FBodyDescription2D Ball;
	Ball.Position = {1, 5};
	Toolbox::FCircle2D Circle;
	Circle.Radius = 0.4f;
	FColliderDescription2D BallCollider;
	BallCollider.Shape = Circle;
	BallCollider.Restitution = 0.4f;
	World->AttachCollider(World->CreateBody(Ball), BallCollider);
	return World;
}
Toolbox::f32 Axis_Internal(const FInputSnapshot& Input, EKey Negative, EKey Positive)
{
	return static_cast<Toolbox::f32>(Input.IsDown(Positive)) - static_cast<Toolbox::f32>(Input.IsDown(Negative));
}
}
ARenderDebugScene::ARenderDebugScene(Toolbox::FJobSystem& Jobs) : m_pJobs(&Jobs)
{
	m_View.Id = 1;
	// 画面右下へ2D観察を置く。1メートルを30ピクセルで表示する。
	m_View2D.ScreenOrigin = {1060, 690};
	m_View2D.PixelsPerMeter = 30;
}
TResult<void> ARenderDebugScene::OnInitialize(const FInitContext& Context)
{
	auto Font = Context.Assets.LoadFont();
	if (!Font)
	{
		return TResult<void>::Failure(Font.Error());
	}
	m_Font = Toolbox::Move(Font).Value();
	ResetSimulation_Internal();
	PrepareView_Internal();
	return {};
}
void ARenderDebugScene::ResetSimulation_Internal()
{
	auto World = Toolbox::MakeUnique<FPhysicsWorld3D>();
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = m_pJobs;
	World->SetExecutionSettings(Execution);
	for (Toolbox::uint32 Index = 0; Index < 3; ++Index)
	{
		FBodyDescription3D Body;
		Body.Type = Index == 0 ? EBodyType::Static : EBodyType::Dynamic;
		Body.Position = Index == 0 ? Toolbox::FVector3{0, -0.5f, 0} :
			(Index == 1 ? Toolbox::FVector3{-1.3f, 3, 0} : Toolbox::FVector3{1.3f, 4.5f, 0});
		const auto Id = World->CreateBody(Body);
		FColliderDescription3D Collider;
		Collider.Friction = 0.6f;
		Collider.Restitution = Index == 2 ? 0.45f : 0;
		if (Index == 2)
		{
			Collider.Shape = Toolbox::FSphere{{0, 0, 0}, 0.65f};
		}
		else
		{
			Toolbox::FOBB Shape;
			Shape.HalfExtents = Index == 0 ? Toolbox::FVector3{6, 0.5f, 6} : Toolbox::FVector3{0.6f, 0.6f, 0.6f};
			Collider.Shape = Shape;
		}
		// 表示用の別登録は作らない。形状と運動区分はWorld自身の採取から得る。
		World->AttachCollider(Id, Collider);
	}
	auto World2D = MakeWorld2D_Internal();
	// 観察の有効状態を引き継ぎ、初回採取と履歴確保を完了させてから一括交換する。
	FPhysicsDebugRecorder3D Recorder;
	Recorder.SetEnabled(m_Recorder.IsEnabled());
	Require_Internal(Recorder.Record(*World, 0, true));
	FPhysicsDebugSnapshot2D Live2D;
	if (m_b2D)
	{
		auto Captured = CapturePhysicsDebugSnapshot2D(*World2D, 0);
		if (!Captured)
		{
			throw Toolbox::FException(Captured.Error().Message);
		}
		Live2D = Toolbox::Move(Captured).Value();
	}
	// 新Worldの作成と初回採取が成功してから既存状態を置き換える。
	m_pWorld = Toolbox::Move(World);
	m_pWorld2D = Toolbox::Move(World2D);
	m_Recorder = Toolbox::Move(Recorder);
	m_Live2D = Toolbox::Move(Live2D);
	m_Selected = {};
	m_PickedCollider.Reset();
	m_Simulation.Reset();
	m_HistoryAge = 0;
	m_SimSeconds = 0;
	m_PhysicsMicros = 0;
	m_DroppedSeconds = 0;
	DXF_LOG_INFO("RenderDebug", "Physics worlds reset (3D colliders=%u, observation %s)",
	             static_cast<unsigned>(m_Recorder.GetLive().Items.Size()), m_Recorder.IsEnabled() ? "on" : "off");
}
void ARenderDebugScene::Capture_Internal(bool ForceHistory)
{
	Require_Internal(m_Recorder.Record(*m_pWorld, m_SimSeconds, ForceHistory));
}
void ARenderDebugScene::Capture2D_Internal()
{
	if (!m_b2D)
	{
		return;
	}
	auto Captured = CapturePhysicsDebugSnapshot2D(*m_pWorld2D, m_SimSeconds);
	if (!Captured)
	{
		throw Toolbox::FException(Captured.Error().Message);
	}
	m_Live2D = Toolbox::Move(Captured).Value();
}
void ARenderDebugScene::UpdateCamera_Internal(const FInputSnapshot& Input, Toolbox::f64 Seconds)
{
	const auto& Raw = Input.GetRaw();
	if (!Raw.bFocused)
	{
		m_bMouseTracked = false;
		return;
	}
	const Toolbox::f64 Delta = Toolbox::Clamp(Seconds, 0.0, 0.25);
	Require_Internal(m_Camera.Orbit(Axis_Internal(Input, EKey::Left, EKey::Right) * Delta * 1.5,
		Axis_Internal(Input, EKey::Down, EKey::Up) * Delta * 1.5));
	Require_Internal(m_Camera.MoveLocal({Axis_Internal(Input, EKey::A, EKey::D),
		Axis_Internal(Input, EKey::Q, EKey::E), Axis_Internal(Input, EKey::S, EKey::W)}, Delta,
		Input.IsDown(EKey::LeftShift) ? 12.0 : 4.0));
	if (Input.IsMouseDown(EMouseButton::Right))
	{
		if (m_bMouseTracked)
		{
			// 符号付き整数差分のオーバーフローを避け、ウィンドウ移動時の飛びを制限する。
			const Toolbox::f64 X = Toolbox::Clamp(Toolbox::f64(Raw.MouseX) - m_PreviousMouseX, -400.0, 400.0);
			const Toolbox::f64 Y = Toolbox::Clamp(Toolbox::f64(Raw.MouseY) - m_PreviousMouseY, -400.0, 400.0);
			Require_Internal(m_Camera.Orbit(X * 0.005, -Y * 0.005));
		}
		m_PreviousMouseX = Raw.MouseX;
		m_PreviousMouseY = Raw.MouseY;
		m_bMouseTracked = true;
	}
	else
	{
		m_bMouseTracked = false;
	}
	const auto WheelSteps = static_cast<Toolbox::uint32>(Toolbox::Min(Toolbox::Abs(Toolbox::f64(Raw.Wheel)), 8.0));
	for (Toolbox::uint32 Step = 0; Step < WheelSteps; ++Step)
	{
		Require_Internal(m_Camera.Zoom(Raw.Wheel > 0 ? 1.0 / 1.15 : 1.15));
	}
	if (Input.WasPressed(EKey::R))
	{
		Require_Internal(m_Camera.SetPose({}));
	}
}
void ARenderDebugScene::OnTick(const FTickContext& Context)
{
	const auto& Input = Context.Input;
	const auto PreviousAge = m_HistoryAge;
	if (Input.WasPressed(EKey::F7))
	{
		m_bTransparencyDemo = !m_bTransparencyDemo;
	}
	if (Input.WasPressed(EKey::Escape) && Context.Scenes != nullptr)
	{
		Context.Scenes->RequestQuit();
		return;
	}
	UpdateCamera_Internal(Input, Context.Time.DeltaSeconds);
	if (Input.WasPressed(EKey::F1))
	{
		m_View.Debug.Surface = static_cast<ESurfaceMode3D>((static_cast<Toolbox::uint32>(m_View.Debug.Surface) + 1) % 3);
	}
	if (Input.WasPressed(EKey::F2))
	{
		m_View.Debug.Lighting = static_cast<ELightingMode3D>((static_cast<Toolbox::uint32>(m_View.Debug.Lighting) + 1) % 3);
	}
	if (Input.WasPressed(EKey::F3))
	{
		m_View.bLightEnabled = !m_View.bLightEnabled;
	}
	if (Input.WasPressed(EKey::F4))
	{
		m_bOverlay = !m_bOverlay;
	}
	if (Input.WasPressed(EKey::F5))
	{
		m_Display.bVelocities = !m_Display.bVelocities;
	}
	if (Input.WasPressed(EKey::F6))
	{
		m_Display.bAlwaysVisible = !m_Display.bAlwaysVisible;
	}
	if (Input.WasPressed(EKey::Tab))
	{
		m_bPanel = !m_bPanel;
	}
	if (Input.WasPressed(EKey::F8))
	{
		// 無効中は採取も履歴保存も行わない。有効化時は現在の状態を一度だけ採取する。
		m_Recorder.SetEnabled(!m_Recorder.IsEnabled());
		DXF_LOG_INFO("RenderDebug", "3D physics observation %s", m_Recorder.IsEnabled() ? "enabled" : "disabled");
		m_HistoryAge = 0;
		m_Selected = {};
		m_PickedCollider.Reset();
		Capture_Internal(true);
	}
	if (Input.WasPressed(EKey::F9))
	{
		m_b2D = !m_b2D;
		DXF_LOG_INFO("RenderDebug", "2D physics observation %s", m_b2D ? "shown" : "hidden");
		m_Live2D = {};
		Capture2D_Internal();
	}
	if (Input.WasPressed(EKey::P))
	{
		m_Simulation.SetPaused(!m_Simulation.IsPaused());
		m_HistoryAge = 0;
		Capture_Internal(true);
	}
	if (Input.WasPressed(EKey::O))
	{
		m_bSlow = !m_bSlow;
		Require_Internal(m_Simulation.SetTimeScale(m_bSlow ? 0.25 : 1.0));
	}
	if (Input.WasPressed(EKey::Enter))
	{
		ResetSimulation_Internal();
	}
	if (Input.WasPressed(EKey::N) && m_Simulation.RequestSingleStep())
	{
		m_HistoryAge = 0;
	}
	auto Plan = m_Simulation.Plan(Context.Time.DeltaSeconds);
	if (!Plan)
	{
		throw Toolbox::FException(Plan.Error().Message);
	}
	m_DroppedSeconds = Plan.Value().DroppedSeconds;
	for (Toolbox::uint32 Index = 0; Index < Plan.Value().StepCount; ++Index)
	{
		const Toolbox::uint64 Begin = Toolbox::MonotonicNanoseconds();
		m_pWorld->Step(Plan.Value().StepSeconds);
		m_PhysicsMicros = (Toolbox::MonotonicNanoseconds() - Begin) / 1000;
		m_pWorld2D->Step(Plan.Value().StepSeconds);
		// 正常完了したStepの秒数だけを時計へ加える。Step番号との積から求めない。
		m_SimSeconds += Plan.Value().StepSeconds;
		Capture_Internal(m_Simulation.IsPaused());
		Capture2D_Internal();
	}
	const auto& History = m_Recorder.GetHistory();
	if (m_Recorder.IsEnabled() && m_Simulation.IsPaused() && History.GetCount() != 0)
	{
		if (Input.WasPressed(EKey::Z))
		{
			m_HistoryAge = Toolbox::Min(m_HistoryAge + 1, History.GetCount() - 1);
		}
		if (Input.WasPressed(EKey::X) && m_HistoryAge != 0)
		{
			--m_HistoryAge;
		}
		if (m_HistoryAge != 0)
		{
			auto Selected = History.ReadAge(m_HistoryAge);
			if (!Selected)
			{
				throw Toolbox::FException(Selected.Error().Message);
			}
			m_Selected = Toolbox::Move(Selected).Value();
		}
	}
	if (PreviousAge != m_HistoryAge)
	{
		m_PickedCollider.Reset();
	}
	PrepareView_Internal();
	UpdatePicking_Internal(Input);
}
void ARenderDebugScene::OnDraw(FRenderContext& Render) const
{
	Require_Internal(Render.Get3D().SetView(m_DrawView));
	// 固定更新直後の値を描く。履歴選択は描画だけを変更し、Worldへ書き戻さない。
	// 観察無効中は空の値になり、Worldを直接読んで代わりに描くことはしない。
	const auto& Snapshot = GetDisplaySnapshot();
	for (Toolbox::size_t Index = 0; Index < Snapshot.Items.Size(); ++Index)
	{
		const auto& Item = Snapshot.Items[Index];
		FDrawStyle3D Style;
		Style.Color = Item.Type == EBodyType::Static ? FColor{100, 100, 100, 255} :
			(Index % 2 == 0 ? FColor{255, 150, 60, 255} : FColor{80, 160, 255, 255});
		const bool Picked = m_PickedCollider && *m_PickedCollider == Item.Collider;
		if (Picked)
		{
			Style.Color = {255, 220, 30, 255};
			// 印は重心であり、交点やCollider中心とは限らない。観察用に遮蔽を無視する。
			const auto Point = ProjectWorldToScreen(m_DrawView, 1280, 720, Item.CenterOfMass);
			if (Point && Point.Value().bInsideView)
			{
				Require_Internal(Render.Get2D().DrawCircle(Point.Value().Screen, 7));
				Require_Internal(Render.Get2D().DrawText(m_Font, "Collider COM", {Point.Value().Screen.X + 12, Point.Value().Screen.Y}));
			}
			char Identity[256];
			snprintf(Identity, sizeof(Identity), "%s Step=%llu World=%llu Body=%llu:%llu Collider=%llu:%llu", m_HistoryAge ? "HISTORY" : "LIVE", Snapshot.Step, Snapshot.World, static_cast<Toolbox::uint64>(Item.Collider.Body.Index), Item.Collider.Body.Generation, static_cast<Toolbox::uint64>(Item.Collider.Index), Item.Collider.Generation);
			char State[256];
			snprintf(State, sizeof(State), "COM=(%.2f,%.2f,%.2f) Velocity=(%.2f,%.2f,%.2f) Sleeping=%s", Item.CenterOfMass.X, Item.CenterOfMass.Y, Item.CenterOfMass.Z, Item.Velocity.X, Item.Velocity.Y, Item.Velocity.Z, Item.bSleeping ? "yes" : "no");
			FDrawStyle Details;
			Details.Layer = 1100;
			Details.Color = {0, 0, 0, 255};
			Require_Internal(Render.Get2D().FillRectangle({0, 650, 950, 720}, Details));
			Details.Layer = 1101;
			Details.Color = {255, 255, 255, 255};
			Require_Internal(Render.Get2D().DrawText(m_Font, Identity, {12, 654}, Details));
			Require_Internal(Render.Get2D().DrawText(m_Font, State, {12, 682}, Details));
		}
		Item.Shape.Visit([&](const auto& Shape)
		{
			if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Shape)>, Toolbox::FSphere>)
			{
				Require_Internal(Render.Get3D().DrawSphere(Shape, Style));
			}
			else
			{
				Require_Internal(Render.Get3D().DrawBox(Shape, Style));
			}
		});
	}
	FDrawStyle3D Grid;
	Grid.Color = {150, 150, 150, 255};
	Grid.Depth = EDepthMode3D::TestOnly;
	Grid.Layer = ERenderLayer3D::Overlay;
	for (Toolbox::int32 Index = -6; Index <= 6; ++Index)
	{
		const Toolbox::f32 Offset = static_cast<Toolbox::f32>(Index);
		Require_Internal(Render.Get3D().DrawLine({Offset, 0.003f, -6}, {Offset, 0.003f, 6}, Grid));
		Require_Internal(Render.Get3D().DrawLine({-6, 0.003f, Offset}, {6, 0.003f, Offset}, Grid));
	}
	if (m_bTransparencyDemo)
	{
		Require_Internal(SubmitTransparencyDemo(Render.Get3D()));
	}
	if (m_bOverlay)
	{
		Require_Internal(SubmitPhysicsDebugSnapshot3D(Snapshot, m_Display, Render.Get3D()));
	}
	if (m_b2D)
	{
		FDrawStyle Back;
		Back.Color = {20, 20, 28, 255};
		Back.Layer = 899;
		Require_Internal(Render.Get2D().FillRectangle({950, 520, 1270, 710}, Back));
		Require_Internal(SubmitPhysicsDebugSnapshot2D(m_Live2D, m_View2D, {}, Render.Get2D()));
	}
	if (m_bPanel)
	{
		DrawPanel_Internal(Render.Get2D());
	}
}
void ARenderDebugScene::DrawPanel_Internal(FRender2DContext& Render) const
{
	FDrawStyle Panel;
	Panel.Color = {0, 0, 0, 255};
	Panel.Layer = 1000;
	Require_Internal(Render.FillRectangle({0, 0, 1280, 208}, Panel));
	FDrawStyle Text;
	Text.Layer = 1001;
	const char* Surfaces[] = {"Solid", "Wireframe", "Solid+Edges"};
	const char* Lights[] = {"Normal (CPU flat)", "Unlit", "LightsOff"};
	const Toolbox::FString Mode = Toolbox::FString("RenderDebug | ") + Surfaces[static_cast<Toolbox::size_t>(m_View.Debug.Surface)] +
		" | " + Lights[static_cast<Toolbox::size_t>(m_View.Debug.Lighting)] + (m_View.bLightEnabled ? " | light ON" : " | light OFF");
	Require_Internal(Render.DrawText(m_Font, Mode, {16, 10}, Text));
	Require_Internal(Render.DrawText(m_Font, "F1:面表示  F2:照明方式  F3:方向光  F4:Collider  F5:速度線  F6:透視  F7:半透明比較  Tab:説明", {16, 36}, Text));
	Require_Internal(Render.DrawText(m_Font, "矢印/右ドラッグ:回転  WASD/QE:平行移動  ホイール:距離  Shift:加速  R:カメラ初期化", {16, 62}, Text));
	Require_Internal(Render.DrawText(m_Font, "P:物理停止  N:固定更新1回  O:0.25倍速  Z/X:停止中の履歴  Enter:物理初期化  F8:観察ON/OFF  F9:2D観察  Esc:終了", {16, 88}, Text));
	const auto& Live = m_Recorder.GetLive();
	const Toolbox::FString Shown = m_Recorder.HasLive() ?
		Toolbox::ToString(m_HistoryAge == 0 ? Live.Step : m_Selected.Step) : Toolbox::FString("-");
	// Stepは3D Worldが数えた正常Step数。時計はサンプルが実際にStepへ渡した秒数の合計。
	const Toolbox::FString Status = Toolbox::FString(m_Simulation.IsPaused() ? "PAUSED" : "RUNNING") +
		(m_bSlow ? " x0.25" : " x1.0") + " | displayed world step=" + Shown +
		" | sample clock(ms)=" + Toolbox::ToString(static_cast<Toolbox::uint64>(m_SimSeconds * 1000.0)) +
		" | history=" + Toolbox::ToString(m_Recorder.GetHistory().GetCount()) +
		" | last live physics CPU wall(us)=" + Toolbox::ToString(m_PhysicsMicros);
	Require_Internal(Render.DrawText(m_Font, Status, {16, 114}, Text));
	const Toolbox::FString Observed = m_Recorder.HasLive() ?
		Toolbox::FString("観察ON: World全件を採取 Body=") + Toolbox::ToString(Live.BodyCount) + " Collider=" +
			Toolbox::ToString(Live.Items.Size()) + "。表示用の別登録なし。" :
		Toolbox::FString("観察OFF: 採取と履歴保存を停止中。Worldの更新は継続。");
	Require_Internal(Render.DrawText(m_Font, Observed, {16, 140}, Text));
	Require_Internal(Render.DrawText(m_Font, m_DroppedSeconds > 0 ? "更新上限でゲーム時間を破棄。GPU時間は未計測。" : "左クリック:表示Colliderを選択。履歴は観察専用。接触点/Impulse/GPU時間は未計測。", {16, 166}, Text));
}
void ARenderDebugScene::OnDeinitialize() noexcept
{
	m_Recorder.Clear();
	m_Selected = {};
	m_PickedCollider.Reset();
	m_Live2D = {};
	m_pWorld2D.Reset();
	m_pWorld.Reset();
	m_Font = {};
}
}

namespace Dxf::RenderDebug
{
const FPhysicsDebugSnapshot3D& ARenderDebugScene::GetDisplaySnapshot() const noexcept
{
	return m_Recorder.IsEnabled() && m_HistoryAge != 0 ? m_Selected : m_Recorder.GetLive();
}
const FRenderView3D& ARenderDebugScene::GetDisplayView() const noexcept
{
	return m_DrawView;
}
Toolbox::TOptional<FColliderId3D> ARenderDebugScene::GetPickedCollider() const
{
	return m_PickedCollider;
}
Toolbox::f64 ARenderDebugScene::GetSimulationSeconds() const noexcept
{
	return m_SimSeconds;
}
Toolbox::size_t ARenderDebugScene::GetHistoryCount() const noexcept
{
	return m_Recorder.GetHistory().GetCount();
}
void ARenderDebugScene::PrepareView_Internal()
{
	const auto View = m_Camera.MakeView(m_View);
	if (!View)
	{
		throw Toolbox::FException(View.Error().Message);
	}
	m_DrawView = View.Value();
}
void ARenderDebugScene::UpdatePicking_Internal(const FInputSnapshot& Input)
{
	if (!m_Recorder.IsEnabled())
	{
		m_PickedCollider.Reset();
		return;
	}
	const auto& Snapshot = GetDisplaySnapshot();
	bool Found = false;
	for (const auto& Item : Snapshot.Items)
	{
		Found = Found || (m_PickedCollider && *m_PickedCollider == Item.Collider);
	}
	if (!Found)
	{
		m_PickedCollider.Reset();
	}
	if (!Input.WasMousePressed(EMouseButton::Left))
	{
		return;
	}
	const auto& Raw = Input.GetRaw();
	// 可視パネルと2D観察域のクリックは3Dへ渡さない。汎用UIルーターは作らない。
	if ((m_bPanel && Raw.MouseX >= 0 && Raw.MouseX < 1280 && Raw.MouseY >= 0 && Raw.MouseY < 208) ||
	    (m_b2D && Raw.MouseX >= 950 && Raw.MouseX < 1270 && Raw.MouseY >= 520 && Raw.MouseY < 710) ||
	    (m_PickedCollider && Raw.MouseX >= 0 && Raw.MouseX < 950 && Raw.MouseY >= 650 && Raw.MouseY < 720))
	{
		return;
	}
	const auto Ray = MakeViewPickSegment(m_DrawView, 1280, 720, {static_cast<Toolbox::f32>(Raw.MouseX), static_cast<Toolbox::f32>(Raw.MouseY)});
	if (!Ray)
	{
		throw Toolbox::FException(Ray.Error().Message);
	}
	m_PickedCollider.Reset();
	if (!Ray.Value())
	{
		return;
	}
	const auto Pick = PickPhysicsDebugSnapshot3D(Snapshot, *Ray.Value());
	if (!Pick)
	{
		throw Toolbox::FException(Pick.Error().Message);
	}
	if (Pick.Value())
	{
		m_PickedCollider = Pick.Value()->Collider;
	}
}
} // namespace Dxf::RenderDebug
