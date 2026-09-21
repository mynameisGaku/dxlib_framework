// SPDX-License-Identifier: NOASSERTION
#include "RenderDebugScene.h"
#include "TransparencyDemo.h"
#include "Dxf/AssetService.h"
#include "Dxf/SceneNavigator.h"
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
Toolbox::f32 Axis_Internal(const FInputSnapshot& Input, EKey Negative, EKey Positive)
{
	return static_cast<Toolbox::f32>(Input.IsDown(Positive)) - static_cast<Toolbox::f32>(Input.IsDown(Negative));
}
}
ARenderDebugScene::ARenderDebugScene(Toolbox::FJobSystem& Jobs) : m_pJobs(&Jobs)
{
	m_View.Id = 1;
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
	return {};
}
void ARenderDebugScene::ResetSimulation_Internal()
{
	auto World = Toolbox::MakeUnique<FPhysicsWorld3D>();
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = m_pJobs;
	World->SetExecutionSettings(Execution);
	Toolbox::TVector<FWatch> Watches;
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
		FWatch Watch;
		Watch.Collider = World->AttachCollider(Id, Collider);
		Watch.LocalShape = Collider.Shape;
		Watch.Motion = Index == 0 ? EDebugBodyMotion::Static : EDebugBodyMotion::Dynamic;
		Watches.PushBack(Toolbox::Move(Watch));
	}
	auto Initial = CapturePhysicsDebugSnapshot3D(*World, Watches, 0, 0);
	if (!Initial)
	{
		throw Toolbox::FException(Initial.Error().Message);
	}
	// 履歴の初期確保も完了させてから、例外のない移動で状態を一括交換する。
	FDebugSnapshotHistory History;
	Require_Internal(History.Push(Initial.Value()));
	// 新Worldの作成と初回採取が成功してから既存状態を置き換える。
	m_pWorld = Toolbox::Move(World);
	m_Watches = Toolbox::Move(Watches);
	m_Live = Toolbox::Move(Initial).Value();
	m_Selected = {};
	m_History = Toolbox::Move(History);
	m_Simulation.Reset();
	m_Tick = 0;
	m_HistoryTick = 0;
	m_HistoryAge = 0;
	m_SimSeconds = 0;
	m_PhysicsMicros = 0;
	m_DroppedSeconds = 0;
}
void ARenderDebugScene::Capture_Internal(bool ForceHistory)
{
	auto Snapshot = CapturePhysicsDebugSnapshot3D(*m_pWorld, m_Watches, m_Tick, m_SimSeconds);
	if (!Snapshot)
	{
		throw Toolbox::FException(Snapshot.Error().Message);
	}
	m_Live = Toolbox::Move(Snapshot).Value();
	if (m_Tick > m_HistoryTick && (ForceHistory || m_Tick % 6 == 0))
	{
		Require_Internal(m_History.Push(m_Live));
		m_HistoryTick = m_Tick;
	}
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
		if (m_Tick == Toolbox::TNumericLimits<Toolbox::uint64>::Max())
		{
			throw Toolbox::FException("Debug fixed tick overflow");
		}
		const Toolbox::uint64 Begin = Toolbox::MonotonicNanoseconds();
		m_pWorld->Step(Plan.Value().StepSeconds);
		m_PhysicsMicros = (Toolbox::MonotonicNanoseconds() - Begin) / 1000;
		++m_Tick;
		m_SimSeconds += Plan.Value().StepSeconds;
		Capture_Internal(m_Simulation.IsPaused());
	}
	if (m_Simulation.IsPaused() && m_History.GetCount() != 0)
	{
		if (Input.WasPressed(EKey::Z))
		{
			m_HistoryAge = Toolbox::Min(m_HistoryAge + 1, m_History.GetCount() - 1);
		}
		if (Input.WasPressed(EKey::X) && m_HistoryAge != 0)
		{
			--m_HistoryAge;
		}
		if (m_HistoryAge != 0)
		{
			auto Selected = m_History.ReadAge(m_HistoryAge);
			if (!Selected)
			{
				throw Toolbox::FException(Selected.Error().Message);
			}
			m_Selected = Toolbox::Move(Selected).Value();
		}
	}
}
void ARenderDebugScene::OnDraw(FRenderContext& Render) const
{
	auto View = m_Camera.MakeView(m_View);
	if (!View)
	{
		throw Toolbox::FException(View.Error().Message);
	}
	Require_Internal(Render.Get3D().SetView(View.Value()));
	// 固定更新直後の値を描く。履歴選択は描画だけを変更し、Worldへ書き戻さない。
	const auto& Snapshot = m_HistoryAge == 0 ? m_Live : m_Selected;
	for (Toolbox::size_t Index = 0; Index < Snapshot.Items.Size(); ++Index)
	{
		const auto& Item = Snapshot.Items[Index];
		FDrawStyle3D Style;
		Style.Color = Item.Motion == EDebugBodyMotion::Static ? FColor{100, 100, 100, 255} :
			(Index % 2 == 0 ? FColor{255, 150, 60, 255} : FColor{80, 160, 255, 255});
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
	Require_Internal(Render.FillRectangle({0, 0, 1280, 182}, Panel));
	FDrawStyle Text;
	Text.Layer = 1001;
	const char* Surfaces[] = {"Solid", "Wireframe", "Solid+Edges"};
	const char* Lights[] = {"Normal (CPU flat)", "Unlit", "LightsOff"};
	const Toolbox::FString Mode = Toolbox::FString("RenderDebug | ") + Surfaces[static_cast<Toolbox::size_t>(m_View.Debug.Surface)] +
		" | " + Lights[static_cast<Toolbox::size_t>(m_View.Debug.Lighting)] + (m_View.bLightEnabled ? " | light ON" : " | light OFF");
	Require_Internal(Render.DrawText(m_Font, Mode, {16, 10}, Text));
	Require_Internal(Render.DrawText(m_Font, "F1:面表示  F2:照明方式  F3:方向光  F4:Collider  F5:速度線  F6:透視  F7:半透明比較  Tab:説明", {16, 36}, Text));
	Require_Internal(Render.DrawText(m_Font, "矢印/右ドラッグ:回転  WASD/QE:平行移動  ホイール:距離  Shift:加速  R:カメラ初期化", {16, 62}, Text));
	Require_Internal(Render.DrawText(m_Font, "P:物理停止  N:固定更新1回  O:0.25倍速  Z/X:停止中の履歴  Enter:物理初期化  Esc:終了", {16, 88}, Text));
	const Toolbox::uint64 ShownTick = m_HistoryAge == 0 ? m_Live.Step : m_Selected.Step;
	const Toolbox::FString Status = Toolbox::FString(m_Simulation.IsPaused() ? "PAUSED" : "RUNNING") +
		(m_bSlow ? " x0.25" : " x1.0") + " | live tick=" + Toolbox::ToString(m_Tick) +
		" | displayed tick=" + Toolbox::ToString(ShownTick) + " | history=" + Toolbox::ToString(m_History.GetCount()) +
		" | last live physics CPU wall(us)=" + Toolbox::ToString(m_PhysicsMicros);
	Require_Internal(Render.DrawText(m_Font, Status, {16, 114}, Text));
	Require_Internal(Render.DrawText(m_Font, m_DroppedSeconds > 0 ? "更新上限でゲーム時間を破棄。GPU時間は未計測。" :
		"履歴は観察専用。Colliderは登録した3件を採取。接触点/Impulse/GPU時間は未計測。", {16, 140}, Text));
}
void ARenderDebugScene::OnDeinitialize() noexcept
{
	m_History.Clear();
	m_Selected = {};
	m_Live = {};
	m_Watches.Clear();
	m_pWorld.Reset();
	m_Font = {};
}
}
