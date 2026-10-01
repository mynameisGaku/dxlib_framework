// SPDX-License-Identifier: NOASSERTION
#include "InteractionScene2D.h"
#include "CharacterSample2DScene.h"
#include "InteractionHud.h"
#include "JointCourseOverlay.h"
#include "MechanismOverlay.h"
#include "MechanismPanel.h"
#include "InteractionScene3D.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderContext.h"
#include "Dxf/SceneNavigator.h"
namespace Dxf::GameplaySample
{
namespace
{
// 1画面の縮尺（ピクセル毎メートル）。
constexpr Toolbox::f32 PixelsPerMeter = 40;
// 2画面の縮尺。
constexpr Toolbox::f32 SplitPixelsPerMeter = 24;

// 2Dの操作（切替・復帰・一時停止・2画面・終了）。一時停止中も受け付ける。
class DInteractionDirector2D final : public DGameObject
{
public:
	explicit DInteractionDirector2D(DInteraction2DScene& Scene) : m_pScene(&Scene)
	{
		SetTickWhenPaused(true);
	}

protected:
	void OnTick(const FTickContext& Context) override
	{
		const FInputSnapshot& Input = Context.Input;
		m_pScene->GetJointCourse().HandleInput(Input, m_pScene->GetClock().IsPaused());
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::Escape))
		{
			Context.Scenes->RequestQuit();
			return;
		}
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::Tab))
		{
			RequireSample(Context.Scenes->RequestChange<DInteraction3DScene>());
			return;
		}
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::I))
		{
			RequireSample(Context.Scenes->RequestChange<DCharacterSample2DScene>());
			return;
		}
		if (Input.WasPressed(EKey::P))
		{
			FSceneClock& Clock = m_pScene->GetClock();
			Clock.SetPaused(!Clock.IsPaused());
		}
		if (Input.WasPressed(EKey::V))
		{
			m_pScene->ToggleSplit();
		}
		if (Input.WasPressed(EKey::M))
		{
			m_pScene->RemoveSupport();
		}
		if (Input.WasPressed(EKey::R))
		{
			m_pScene->Respawn();
		}
	}

private:
	// 所有するシーン（シーンはオブジェクトより長く生存する）。
	DInteraction2DScene* m_pScene;
};

// 回転した矩形を塗る。
void FillBox_Internal(FRender2DContext& Draw, const Toolbox::FVector2 (&Corners)[4], FColor Color, Toolbox::int32 Layer,
                      FIntRect Clip)
{
	FDrawStyle Style;
	Style.bClip = true;
	Style.ClipRect = Clip;
	Style.Color = Color;
	Style.Layer = Layer;
	RequireSample(Draw.FillTriangle({Corners[0].X, Corners[0].Y}, {Corners[1].X, Corners[1].Y},
	                                {Corners[2].X, Corners[2].Y}, Style));
	RequireSample(Draw.FillTriangle({Corners[0].X, Corners[0].Y}, {Corners[2].X, Corners[2].Y},
	                                {Corners[3].X, Corners[3].Y}, Style));
}
} // namespace

void DInteraction2DScene::Respawn()
{
	if (DPlayer2D* Player = GetPlayer())
	{
		const Toolbox::int32 Index = m_Rules.GetCheckpoint();
		// 記録した中心は円／球の高さなので、カプセルでは半高だけ上げて足元を合わせる。
		const auto& Settings = Player->GetCharacter().GetSettings();
		const Toolbox::f64 Lift = Settings.Shape == ECharacterShape::Capsule ? Settings.HalfHeight : 0;
		Player->GetCharacter().Teleport({InteractionLayout::CheckpointX[Index],
		                                 static_cast<Toolbox::f32>(InteractionLayout::CheckpointY[Index] + Lift)});
		m_Rules.Respawned();
	}
}
void DInteraction2DScene::RemoveSupport()
{
	const DPlayer2D* Player = GetPlayer();
	if (Player == nullptr || !Player->IsInitialized())
	{
		return;
	}
	const auto Ground = Player->GetCharacter().GetGround().Collider;
	if (!Ground)
	{
		return;
	}
	for (const auto& Handle : m_Course.Platforms)
	{
		auto* Platform = Handle.Get();
		if (Platform != nullptr && Platform->GetMover() != nullptr)
		{
			const auto Body = Platform->GetMover()->GetBodyId();
			if (Body && *Body == Ground->Body)
			{
				Platform->Destroy();
				return;
			}
		}
	}
}
Toolbox::TOptional<FBodyId2D> DInteraction2DScene::GetPlayerBody() const noexcept
{
	if (const DPlayer2D* Player = GetPlayer(); Player != nullptr && Player->IsInitialized())
	{
		return Player->GetCharacter().GetBodyId();
	}
	return {};
}
TResult<void> DInteraction2DScene::OnInitialize(const FInitContext& Context)
{
	// 接触・Triggerのイベントを使うのはこのシーンの責任で有効化する。
	FWorldEventSettings Events;
	Events.bEnabled = true;
	Events.MaxPairs = 256;
	// 接触を表示する範囲。キャラクターのSkinWidth（0.02m）の外側にも0.005mの余裕を設ける。
	Events.ContactMargin = 0.025f;
	GetPhysicsWorld().SetEventSettings(Events);
	auto Font = Context.Assets.LoadFont();
	if (!Font)
	{
		return TResult<void>::Failure(Font.Error());
	}
	m_Font = Font.Value();
	m_Ui.Initialize(
	    *this, Context.Assets,
	    [this]()
	    {
		    ToggleSplit();
	    },
	    [this]()
	    {
		    ToggleShape();
	    },
	    [this]()
	    {
		    TogglePush();
	    },
	    [this](DUiPanel& Panel, FUiScope& Scope)
	    {
		    BuildMechanismPanel(Panel, Scope, m_MechanismCourse.GetController());
	    });
	if (auto Spawned = SpawnInteractionCourse<FInteraction2D>(*this, *this, m_Course); !Spawned)
	{
		return Spawned;
	}
	m_JointCourse.Initialize(*this);
	m_MechanismCourse.Initialize(*this);
	auto Director = Spawn<DInteractionDirector2D>(*this);
	if (!Director)
	{
		return TResult<void>::Failure(Director.Error());
	}
	return {};
}
FVector2 DInteraction2DScene::ToScreen(Toolbox::FVector2 World, Toolbox::int32 Side) const
{
	// 1画面と分割画面で共通の投影。物理位置を変更しない。
	const Toolbox::int32 LeftPixel = m_bSplit ? m_Ui.GetWidth() * Side / 2 : 0;
	const Toolbox::int32 RightPixel = m_bSplit ? m_Ui.GetWidth() * (Side + 1) / 2 : m_Ui.GetWidth();
	const Toolbox::f32 Width = static_cast<Toolbox::f32>(RightPixel - LeftPixel);
	const Toolbox::f32 Scale = m_bSplit ? SplitPixelsPerMeter : PixelsPerMeter;
	const Toolbox::f32 Left = static_cast<Toolbox::f32>(LeftPixel);
	const DPlayer2D* Player = GetPlayer();
	const Toolbox::f32 CameraX =
	    Player != nullptr && Player->IsInitialized() ? Player->GetCharacter().GetRenderCenter().X : 0;
	return {Left + Width * 0.5f + (World.X - CameraX) * Scale,
	        static_cast<Toolbox::f32>(m_Ui.GetHeight()) * (560.0f / 720.0f) - World.Y * Scale};
}
void DInteraction2DScene::DrawView_Internal(FRenderContext& Render, Toolbox::int32 Side) const
{
	auto& Draw = Render.Get2D();
	// 画面外の形状を隣のViewへ描かない。
	const Toolbox::int32 Left = m_bSplit ? m_Ui.GetWidth() * Side / 2 : 0;
	const Toolbox::int32 Right = m_bSplit ? m_Ui.GetWidth() * (Side + 1) / 2 : m_Ui.GetWidth();
	const FIntRect Clip{Left, 0, Right, m_Ui.GetHeight()};
	const Toolbox::f32 Scale = m_bSplit ? SplitPixelsPerMeter : PixelsPerMeter;
	auto Point = [&](Toolbox::FVector2 World) -> Toolbox::FVector2
	{
		const FVector2 Screen = ToScreen(World, Side);
		return {Screen.X, Screen.Y};
	};
	auto Box =
	    [&](Toolbox::FVector2 Center, Toolbox::FVector2 Half, Toolbox::f32 Angle, FColor Color, Toolbox::int32 Layer)
	{
		FLevelBox Shape;
		Shape.Center = Center;
		Shape.Half = Half;
		Shape.Angle = Angle;
		Toolbox::FVector2 Corners[4];
		LevelBoxCorners(Shape, Corners);
		for (Toolbox::FVector2& Corner : Corners)
		{
			Corner = Point(Corner);
		}
		FillBox_Internal(Draw, Corners, Color, Layer, Clip);
	};
	auto Circle = [&](Toolbox::FVector2 Center, Toolbox::f32 Radius, FColor Color, Toolbox::int32 Layer)
	{
		FDrawStyle Style;
		Style.Color = Color;
		Style.Layer = Layer;
		Style.bClip = true;
		Style.ClipRect = Clip;
		const Toolbox::FVector2 At = Point(Center);
		RequireSample(Draw.FillCircle({At.X, At.Y}, Radius * Scale, Style));
	};
	DrawJointCourse2D(Render, *this, Side);
	DrawMechanismCourse2D(Render, *this, Side);
	const auto& Ground = InteractionLayout::GetGround();
	for (Toolbox::size_t Index = 0; Index < Ground.Size(); ++Index)
	{
		Box(Ground[Index].Center, Ground[Index].Half, 0, {60, 70, 90, 255}, 0);
	}
	Box({InteractionLayout::HazardX, InteractionLayout::HazardY}, {InteractionLayout::HazardHalfX, 0.2f}, 0,
	    {150, 40, 40, 255}, 0);
	Box({InteractionLayout::PlateX, InteractionLayout::PlateY},
	    {InteractionLayout::PlateHalfX, InteractionLayout::PlateHalfY}, 0,
	    m_Rules.GetPlateOccupants() > 0 ? FColor{240, 200, 60, 255} : FColor{120, 110, 60, 255}, 2);
	for (const auto& Handle : m_Course.Checkpoints)
	{
		if (const auto* Checkpoint = Handle.Get())
		{
			Box(Checkpoint->GetPosition(), {0.08f, InteractionLayout::CheckpointHalf}, 0, {90, 200, 120, 255}, 2);
		}
	}
	for (const auto& Handle : m_Course.Pickups)
	{
		if (const auto* Pickup = Handle.Get())
		{
			Circle(Pickup->GetPosition(), InteractionLayout::PickupRadius, {250, 220, 70, 255}, 3);
		}
	}
	if (const auto* Door = m_Course.Door.Get(); Door != nullptr && Door->GetMover() != nullptr)
	{
		Box(Door->GetMover()->GetRenderPosition(), {InteractionLayout::DoorHalfX, InteractionLayout::DoorHalfY}, 0,
		    {170, 120, 70, 255}, 4);
	}
	for (const auto& Handle : m_Course.Platforms)
	{
		if (const auto* Platform = Handle.Get(); Platform != nullptr && Platform->GetMover() != nullptr)
		{
			Box(Platform->GetMover()->GetRenderPosition(), Platform->GetHalf(),
			    Platform->GetMover()->GetRenderRotation(), {80, 150, 200, 255}, 4);
		}
	}
	if (const auto* Crate = m_Course.Crate.Get(); Crate != nullptr && Crate->GetRigid() != nullptr)
	{
		Box(Crate->GetRigid()->GetRenderPosition(), {InteractionLayout::CrateHalf, InteractionLayout::CrateHalf},
		    Crate->GetRigid()->GetRenderAngle(),
		    m_Rules.IsTouchingCrate() ? FColor{230, 130, 90, 255} : FColor{160, 110, 80, 255}, 5);
	}
	// 重い箱と、昇降床に載せた箱。
	const TObjectHandle<TInteractionCrate<FInteraction2D>> Extra[2] = {m_Course.HeavyCrate, m_Course.LiftCrate};
	for (const auto& Handle : Extra)
	{
		if (const auto* Extra2D = Handle.Get(); Extra2D != nullptr && Extra2D->GetRigid() != nullptr)
		{
			Box(Extra2D->GetRigid()->GetRenderPosition(), {Extra2D->GetHalf(), Extra2D->GetHalf()},
			    Extra2D->GetRigid()->GetRenderAngle(),
			    Extra2D->GetRole() == EInteractionCrate::Heavy ? FColor{90, 70, 60, 255} : FColor{200, 150, 90, 255},
			    5);
		}
	}
	Box(TInteractionLowCeiling<FInteraction2D>::Center(),
	    {InteractionLayout::LowCeilingHalfX, InteractionLayout::LowCeilingHalfY}, 0, {60, 70, 90, 255}, 0);
	if (const DPlayer2D* Player = GetPlayer(); Player != nullptr && Player->IsInitialized())
	{
		const DCharacterMovement2DComponent& Character = Player->GetCharacter();
		const FCharacterMoveSettings2D& Settings = Character.GetSettings();
		const Toolbox::FVector2 Center = Character.GetRenderCenter();
		if (Settings.Shape == ECharacterShape::Capsule)
		{
			// 中心線は上向き（Up）。両端の円と、その間の矩形で示す。
			const Toolbox::f32 Half = static_cast<Toolbox::f32>(Settings.HalfHeight);
			Circle(Center + Toolbox::FVector2{0, -Half}, Settings.Radius, PlayerColor, 10);
			Circle(Center + Toolbox::FVector2{0, Half}, Settings.Radius, PlayerColor, 10);
			Box(Center, {Settings.Radius, Half}, 0, PlayerColor, 10);
		}
		else
		{
			Circle(Center, Settings.Radius, PlayerColor, 10);
		}
	}
}
void DInteraction2DScene::OnDraw(FRenderContext& Render) const
{
	auto& Draw = Render.Get2D();
	FDrawStyle Background;
	Background.Color = {18, 20, 26, 255};
	Background.Layer = -10;
	RequireSample(Draw.FillRectangle({0, 0, m_Ui.GetWidth(), m_Ui.GetHeight()}, Background));
	// 描画の数は、更新・物理Step・イベントの配送の回数に影響しない。
	if (m_bSplit)
	{
		DrawView_Internal(Render, 0);
		DrawView_Internal(Render, 1);
	}
	else
	{
		DrawView_Internal(Render, 0);
	}
	FDrawStyle Text;
	Text.Layer = 100;
	RequireSample(
	    Draw.DrawText(m_Font, "Interaction 2D  [A/D] move  [Space] jump  [R] checkpoint  [V] split", {16, 12}, Text));
	RequireSample(Draw.DrawText(
	    m_Font, "[P] pause  [F1] settings  [H] UI  [M] remove floor  [Tab] 2D/3D  [I] character  [Esc] quit",
	    {16, static_cast<Toolbox::f32>(m_Ui.GetHeight() - 28)}, Text));
	char Line[256];
	InteractionRulesText(m_Rules, Line);
	m_Ui.Draw(Render, Line);
	const DPlayer2D* Player = GetPlayer();
	if (Player == nullptr || !Player->IsInitialized())
	{
		return;
	}
	const DCharacterMovement2DComponent& Character = Player->GetCharacter();
	InteractionSupportText(Character.GetLastStep(), 1.0 / 60.0, Line);
	RequireSample(Draw.DrawText(m_Font, Line, {16, 68}, Text));
	const Toolbox::FVector2 Center = Character.GetRenderCenter();
	snprintf(Line, sizeof(Line), "pos (%.2f, %.2f)  ground %s  horizontal %s  vertical %s  %s", Center.X, Center.Y,
	         GroundName(Character.GetGround().State), MoveStopName(Character.GetLastStep().Horizontal.Stop),
	         MoveStopName(Character.GetLastStep().Vertical.Stop), GetClock().IsPaused() ? "[PAUSED]" : "");
	RequireSample(Draw.DrawText(m_Font, Line, {16, 96}, Text));
	DrawJointStatus(Render, *this, m_Font);
	DrawMechanismStatus(Render, *this, m_Font);
	InteractionModeText(m_Mode, Line);
	RequireSample(Draw.DrawText(m_Font, Line, {16, 124}, Text));
}
} // namespace Dxf::GameplaySample
