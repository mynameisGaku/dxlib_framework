// SPDX-License-Identifier: NOASSERTION
#include "InteractionScene3D.h"
#include "CharacterSample3DScene.h"
#include "InteractionHud.h"
#include "InteractionScene2D.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderContext.h"
#include "Dxf/SceneNavigator.h"
namespace Dxf::GameplaySample
{
namespace
{
// 3Dの操作（切替・復帰・一時停止・2画面・終了）。一時停止中も受け付ける。
class DInteractionDirector3D final : public DGameObject
{
public:
	explicit DInteractionDirector3D(DInteraction3DScene& Scene) : m_pScene(&Scene)
	{
		SetTickWhenPaused(true);
	}

protected:
	void OnTick(const FTickContext& Context) override
	{
		const FInputSnapshot& Input = Context.Input;
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::Escape))
		{
			Context.Scenes->RequestQuit();
			return;
		}
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::Tab))
		{
			RequireSample(Context.Scenes->RequestChange<DInteraction2DScene>());
			return;
		}
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::I))
		{
			RequireSample(Context.Scenes->RequestChange<DCharacterSample3DScene>());
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
	DInteraction3DScene* m_pScene;
};

// 位置・向き・半幅の箱。
Toolbox::FOBB Box_Internal(Toolbox::FVector3 Center, Toolbox::FQuaternion Rotation, Toolbox::FVector3 Half) noexcept
{
	Toolbox::FOBB Box{Center, Half};
	Box.Axes[0] = Rotation.Rotate({1, 0, 0});
	Box.Axes[1] = Rotation.Rotate({0, 1, 0});
	Box.Axes[2] = Rotation.Rotate({0, 0, 1});
	return Box;
}
} // namespace

void DInteraction3DScene::Respawn()
{
	if (DPlayer3D* Player = GetPlayer())
	{
		const Toolbox::int32 Index = m_Rules.GetCheckpoint();
		// 記録した中心は円／球の高さなので、カプセルでは半高だけ上げて足元を合わせる。
		const auto& Settings = Player->GetCharacter().GetSettings();
		const Toolbox::f64 Lift = Settings.Shape == ECharacterShape::Capsule ? Settings.HalfHeight : 0;
		Player->GetCharacter().Teleport({InteractionLayout::CheckpointX[Index],
		                                 static_cast<Toolbox::f32>(InteractionLayout::CheckpointY[Index] + Lift), 0});
		m_Rules.Respawned();
	}
}
void DInteraction3DScene::RemoveSupport()
{
	const DPlayer3D* Player = GetPlayer();
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
Toolbox::TOptional<FBodyId3D> DInteraction3DScene::GetPlayerBody() const noexcept
{
	if (const DPlayer3D* Player = GetPlayer(); Player != nullptr && Player->IsInitialized())
	{
		return Player->GetCharacter().GetBodyId();
	}
	return {};
}
FRenderView3D DInteraction3DScene::GetView(Toolbox::int32 Side) const
{
	Toolbox::FVector3 Center{0, 0.52f, 0};
	if (const DPlayer3D* Player = GetPlayer(); Player != nullptr && Player->IsInitialized())
	{
		Center = Player->GetCharacter().GetRenderCenter();
	}
	FRenderView3D View;
	View.Id = static_cast<Toolbox::uint64>(Side + 1);
	View.Eye = Center + Toolbox::FVector3{0, 4, -10};
	View.Target = Center + Toolbox::FVector3{0, 0.5f, 0};
	View.NearPlane = 0.1f;
	View.FarPlane = 200;
	if (m_bSplit)
	{
		View.bViewport = true;
		View.Viewport = {m_Ui.GetWidth() * Side / 2, 0, m_Ui.GetWidth() * (Side + 1) / 2, m_Ui.GetHeight()};
		if (Side == 1)
		{
			View.Eye = Center + Toolbox::FVector3{5, 9, -4};
		}
	}
	return View;
}
TResult<void> DInteraction3DScene::OnInitialize(const FInitContext& Context)
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
	    });
	if (auto Spawned = SpawnInteractionCourse<FInteraction3D>(*this, *this, m_Course); !Spawned)
	{
		return Spawned;
	}
	auto Director = Spawn<DInteractionDirector3D>(*this);
	if (!Director)
	{
		return TResult<void>::Failure(Director.Error());
	}
	return {};
}
void DInteraction3DScene::OnDraw(FRenderContext& Render) const
{
	auto& Draw3D = Render.Get3D();
	// 描画の数は、更新・物理Step・イベントの配送の回数に影響しない。
	for (Toolbox::int32 Side = 0; Side < (m_bSplit ? 2 : 1); ++Side)
	{
		RequireSample(Draw3D.SetView(GetView(Side)));
		FDrawStyle3D Style;
		const auto& Ground = InteractionLayout::GetGround();
		Style.Color = {60, 70, 90, 255};
		for (Toolbox::size_t Index = 0; Index < Ground.Size(); ++Index)
		{
			RequireSample(Draw3D.DrawBox(LevelBoxShape3D(Ground[Index]), Style));
		}
		// 穴の下の危険領域を、実Sensorと同じ中心・半幅で示す。物理の登録はCourseが所有する。
		Style.Color = {150, 40, 40, 255};
		RequireSample(Draw3D.DrawBox(Box_Internal({InteractionLayout::HazardX, InteractionLayout::HazardY, 0}, {},
		                                          {InteractionLayout::HazardHalfX, InteractionLayout::HazardHalfY,
		                                           InteractionLayout::DepthHalf}),
		                             Style));
		Style.Color = m_Rules.GetPlateOccupants() > 0 ? FColor{240, 200, 60, 255} : FColor{120, 110, 60, 255};
		RequireSample(Draw3D.DrawBox(
		    Box_Internal({InteractionLayout::PlateX, InteractionLayout::PlateY, 0}, {},
		                 {InteractionLayout::PlateHalfX, InteractionLayout::PlateHalfY, InteractionLayout::PlateHalfX}),
		    Style));
		Style.Color = {90, 200, 120, 255};
		for (const auto& Handle : m_Course.Checkpoints)
		{
			if (const auto* Checkpoint = Handle.Get())
			{
				RequireSample(Draw3D.DrawBox(
				    Box_Internal(Checkpoint->GetPosition(), {}, {0.08f, InteractionLayout::CheckpointHalf, 0.08f}),
				    Style));
			}
		}
		Style.Color = {250, 220, 70, 255};
		for (const auto& Handle : m_Course.Pickups)
		{
			if (const auto* Pickup = Handle.Get())
			{
				RequireSample(Draw3D.DrawSphere({Pickup->GetPosition(), InteractionLayout::PickupRadius}, Style, 12));
			}
		}
		if (const auto* Door = m_Course.Door.Get(); Door != nullptr && Door->GetMover() != nullptr)
		{
			Style.Color = {170, 120, 70, 255};
			RequireSample(Draw3D.DrawBox(Box_Internal(Door->GetMover()->GetRenderPosition(), {},
			                                          {InteractionLayout::DoorHalfX, InteractionLayout::DoorHalfY,
			                                           InteractionLayout::DepthHalf}),
			                             Style));
		}
		Style.Color = {80, 150, 200, 255};
		for (const auto& Handle : m_Course.Platforms)
		{
			if (const auto* Platform = Handle.Get(); Platform != nullptr && Platform->GetMover() != nullptr)
			{
				const Toolbox::FVector2 Half = Platform->GetHalf();
				const Toolbox::f32 Depth =
				    Platform->GetKind() == EInteractionPlatform::Turning ? Half.X : InteractionLayout::DepthHalf;
				RequireSample(
				    Draw3D.DrawBox(Box_Internal(Platform->GetMover()->GetRenderPosition(),
				                                Platform->GetMover()->GetRenderRotation(), {Half.X, Half.Y, Depth}),
				                   Style));
			}
		}
		if (const auto* Crate = m_Course.Crate.Get(); Crate != nullptr && Crate->GetRigid() != nullptr)
		{
			Style.Color = m_Rules.IsTouchingCrate() ? FColor{230, 130, 90, 255} : FColor{160, 110, 80, 255};
			const Toolbox::f32 Half = InteractionLayout::CrateHalf;
			RequireSample(Draw3D.DrawBox(Box_Internal(Crate->GetRigid()->GetRenderPosition(),
			                                          Crate->GetRigid()->GetRenderOrientation(), {Half, Half, Half}),
			                             Style));
		}
		// 重い箱と、昇降床に載せた箱。
		const TObjectHandle<TInteractionCrate<FInteraction3D>> Extra[2] = {m_Course.HeavyCrate, m_Course.LiftCrate};
		for (const auto& Handle : Extra)
		{
			if (const auto* Extra3D = Handle.Get(); Extra3D != nullptr && Extra3D->GetRigid() != nullptr)
			{
				Style.Color = Extra3D->GetRole() == EInteractionCrate::Heavy ? FColor{90, 70, 60, 255}
				                                                             : FColor{200, 150, 90, 255};
				const Toolbox::f32 Half = Extra3D->GetHalf();
				RequireSample(
				    Draw3D.DrawBox(Box_Internal(Extra3D->GetRigid()->GetRenderPosition(),
				                                Extra3D->GetRigid()->GetRenderOrientation(), {Half, Half, Half}),
				                   Style));
			}
		}
		Style.Color = {60, 70, 90, 255};
		RequireSample(Draw3D.DrawBox(Box_Internal(TInteractionLowCeiling<FInteraction3D>::Center(), {},
		                                          {InteractionLayout::LowCeilingHalfX,
		                                           InteractionLayout::LowCeilingHalfY, InteractionLayout::DepthHalf}),
		                             Style));
		if (const DPlayer3D* Player = GetPlayer(); Player != nullptr && Player->IsInitialized())
		{
			Style.Color = PlayerColor;
			const DCharacterMovement3DComponent& Character = Player->GetCharacter();
			const FCharacterMoveSettings3D& Settings = Character.GetSettings();
			const Toolbox::FVector3 Center = Character.GetRenderCenter();
			if (Settings.Shape == ECharacterShape::Capsule)
			{
				// 中心線はUpに沿う。両端の球と、その間を埋める球で示す。
				const Toolbox::FVector3 Axis =
				    Toolbox::Normalize(Settings.Up) * static_cast<Toolbox::f32>(Settings.HalfHeight);
				RequireSample(Draw3D.DrawSphere({Center - Axis, Settings.Radius}, Style, 20));
				RequireSample(Draw3D.DrawSphere({Center + Axis, Settings.Radius}, Style, 20));
			}
			RequireSample(Draw3D.DrawSphere({Center, Settings.Radius}, Style, 20));
		}
	}
	auto& Draw = Render.Get2D();
	FDrawStyle Text;
	Text.Layer = 100;
	RequireSample(Draw.DrawText(m_Font, "Interaction 3D  [A/D/W/S] move  [Space] jump  [R] checkpoint  [V] split",
	                            {16, 12}, Text));
	RequireSample(Draw.DrawText(
	    m_Font, "[P] pause  [F1] settings  [H] UI  [M] remove floor  [Tab] 2D/3D  [I] character  [Esc] quit",
	    {16, static_cast<Toolbox::f32>(m_Ui.GetHeight() - 28)}, Text));
	char Line[256];
	InteractionRulesText(m_Rules, Line);
	m_Ui.Draw(Render, Line);
	const DPlayer3D* Player = GetPlayer();
	if (Player == nullptr || !Player->IsInitialized())
	{
		return;
	}
	const DCharacterMovement3DComponent& Character = Player->GetCharacter();
	InteractionSupportText(Character.GetLastStep(), 1.0 / 60.0, Line);
	RequireSample(Draw.DrawText(m_Font, Line, {16, 68}, Text));
	const Toolbox::FVector3 Center = Character.GetRenderCenter();
	snprintf(Line, sizeof(Line), "pos (%.2f, %.2f, %.2f)  ground %s  horizontal %s  vertical %s  %s", Center.X,
	         Center.Y, Center.Z, GroundName(Character.GetGround().State),
	         MoveStopName(Character.GetLastStep().Horizontal.Stop), MoveStopName(Character.GetLastStep().Vertical.Stop),
	         GetClock().IsPaused() ? "[PAUSED]" : "");
	RequireSample(Draw.DrawText(m_Font, Line, {16, 96}, Text));
	InteractionModeText(m_Mode, Line);
	RequireSample(Draw.DrawText(m_Font, Line, {16, 124}, Text));
}
} // namespace Dxf::GameplaySample
