// SPDX-License-Identifier: NOASSERTION
#include "CharacterSample.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderContext.h"
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/RigidBodyComponent3D.h"
#include "Dxf/SceneNavigator.h"
#include <stdio.h>
namespace Dxf::GameplaySample
{
namespace
{
// 画面1ピクセルあたりの長さの逆数（2Dの縮尺、ピクセル毎メートル）。
constexpr Toolbox::f32 PixelsPerMeter = 28;
// 失敗を例外へ変え、Applicationの失敗として報告させる。
void Require_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
// 箱の色（番号ごと）。プレイヤーの色とは重ならない。
FColor BoxColor_Internal(Toolbox::size_t Index) noexcept
{
	constexpr FColor Colors[LevelBoxCount] = {{60, 70, 90, 255},   {90, 110, 150, 255},  {120, 90, 150, 255},
	                                          {150, 80, 80, 255},  {80, 140, 90, 255},   {90, 120, 100, 255},
	                                          {160, 120, 60, 255}, {100, 100, 120, 255}, {110, 110, 130, 255}};
	return Colors[Index];
}
// プレイヤーの色（画素の確認に使う）。
constexpr FColor PlayerColor = {255, 200, 40, 255};
// 停止理由の表示名。
const char* MoveStopName_Internal(ECharacterMoveStop Stop) noexcept
{
	switch (Stop)
	{
	case ECharacterMoveStop::NoMovement:
		return "none";
	case ECharacterMoveStop::Completed:
		return "completed";
	case ECharacterMoveStop::Slid:
		return "slid";
	case ECharacterMoveStop::Blocked:
		return "blocked";
	case ECharacterMoveStop::MissingNormal:
		return "no-normal";
	case ECharacterMoveStop::AmbiguousContact:
		return "ambiguous";
	case ECharacterMoveStop::PrecisionLimit:
		return "precision";
	case ECharacterMoveStop::IterationLimit:
		return "iterations";
	case ECharacterMoveStop::ContactLimit:
		return "contacts";
	case ECharacterMoveStop::QueryLimit:
		return "queries";
	}
	return "?";
}
// 足元の表示名。
const char* GroundName_Internal(ECharacterGroundState State) noexcept
{
	switch (State)
	{
	case ECharacterGroundState::Airborne:
		return "airborne";
	case ECharacterGroundState::Walkable:
		return "walkable";
	case ECharacterGroundState::Steep:
		return "steep";
	}
	return "?";
}
// 重なりの解消の表示名。
const char* RecoveryName_Internal(ECharacterRecoveryStatus Status) noexcept
{
	switch (Status)
	{
	case ECharacterRecoveryStatus::NoOverlap:
		return "none";
	case ECharacterRecoveryStatus::Resolved:
		return "resolved";
	case ECharacterRecoveryStatus::Ambiguous:
		return "ambiguous";
	case ECharacterRecoveryStatus::TooDeep:
		return "too-deep";
	case ECharacterRecoveryStatus::Blocked:
		return "blocked";
	case ECharacterRecoveryStatus::IterationLimit:
		return "iterations";
	case ECharacterRecoveryStatus::ContactLimit:
		return "contacts";
	case ECharacterRecoveryStatus::QueryLimit:
		return "queries";
	}
	return "?";
}
// 直近の固定更新の変化（着地・離地・天井・段差・吸い付き・ジャンプ）の表示。
template <typename TStep> void EventText_Internal(const TStep& Step, char (&Text)[64])
{
	snprintf(Text, sizeof(Text), "%s%s%s%s%s%s", Step.bJumped ? "jump " : "", Step.bLanded ? "landed " : "",
	         Step.bLeftGround ? "left-ground " : "", Step.bHitCeiling ? "ceiling " : "",
	         Step.bSteppedUp ? "step-up " : "", Step.bSnapped ? "snap " : "");
}
// 左右（と前後）の入力。押されていない方向は0。
Toolbox::f32 Axis_Internal(const FInputSnapshot& Input, EKey Negative, EKey Positive) noexcept
{
	return (Input.IsDown(Positive) ? 1.0f : 0.0f) - (Input.IsDown(Negative) ? 1.0f : 0.0f);
}

// 2Dの地形。一つのStaticのBodyに、箱ごとのColliderを付ける。
class DLevel2D final : public DGameObject
{
protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		FBodyDescription2D Body;
		Body.Type = EBodyType::Static;
		auto Rigid = AddComponent<DRigidBody2DComponent>(Body);
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		const auto& Boxes = GetLevelBoxes();
		for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
		{
			if (Boxes[Index].bOnly3D)
			{
				continue;
			}
			FColliderDescription2D Collider;
			Collider.Shape = Toolbox::FOrientedBox2D{Boxes[Index].Center, Boxes[Index].Half, Boxes[Index].Angle};
			auto Attached = AddComponent<DCollider2DComponent>(Collider);
			if (!Attached)
			{
				return TResult<void>::Failure(Attached.Error());
			}
		}
		return {};
	}
};
// 3Dの地形。
class DLevel3D final : public DGameObject
{
protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		FBodyDescription3D Body;
		Body.Type = EBodyType::Static;
		auto Rigid = AddComponent<DRigidBody3DComponent>(Body);
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		const auto& Boxes = GetLevelBoxes();
		for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
		{
			const FLevelBox& Box = Boxes[Index];
			Toolbox::FOBB Shape{{Box.Center.X, Box.Center.Y, Box.CenterZ}, {Box.Half.X, Box.Half.Y, Box.HalfZ}};
			const Toolbox::f32 Cosine = static_cast<Toolbox::f32>(Toolbox::Cos(Toolbox::f64(Box.Angle)));
			const Toolbox::f32 Sine = static_cast<Toolbox::f32>(Toolbox::Sin(Toolbox::f64(Box.Angle)));
			Shape.Axes = {Toolbox::FVector3{Cosine, Sine, 0}, Toolbox::FVector3{-Sine, Cosine, 0},
			              Toolbox::FVector3{0, 0, 1}};
			FColliderDescription3D Collider;
			Collider.Shape = Shape;
			auto Attached = AddComponent<DCollider3DComponent>(Collider);
			if (!Attached)
			{
				return TResult<void>::Failure(Attached.Error());
			}
		}
		return {};
	}
};
// 2Dの操作（切替・リセット・重なりの確認・一時停止・終了）。一時停止中も受け付ける。
class DDirector2D final : public DGameObject
{
public:
	explicit DDirector2D(TObjectHandle<DPlayer2D> Player) : m_Player(Player)
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
			Require_Internal(Context.Scenes->RequestChange<DCharacterSample3DScene>());
			return;
		}
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::P) && Context.Scenes->GetCurrent() != nullptr)
		{
			FSceneClock& Clock = Context.Scenes->GetCurrent()->GetClock();
			Clock.SetPaused(!Clock.IsPaused());
		}
		DPlayer2D* Player = m_Player.Get();
		if (Player == nullptr)
		{
			return;
		}
		if (Input.WasPressed(EKey::R))
		{
			Player->GetCharacter().Teleport({StartX, StartY});
		}
		if (Input.WasPressed(EKey::O))
		{
			// 床へ0.2めり込ませ、次の固定更新の重なりの解消を見る。
			Player->GetCharacter().Teleport({StartX, 0.3f});
		}
	}

private:
	TObjectHandle<DPlayer2D> m_Player;
};
// 3Dの操作。[V]で2画面を切り替える。
class DDirector3D final : public DGameObject
{
public:
	DDirector3D(TObjectHandle<DPlayer3D> Player, DCharacterSample3DScene& Scene) : m_Player(Player), m_pScene(&Scene)
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
			Require_Internal(Context.Scenes->RequestChange<DCharacterSample2DScene>());
			return;
		}
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::P) && Context.Scenes->GetCurrent() != nullptr)
		{
			FSceneClock& Clock = Context.Scenes->GetCurrent()->GetClock();
			Clock.SetPaused(!Clock.IsPaused());
		}
		if (Input.WasPressed(EKey::V))
		{
			m_pScene->ToggleSplit();
		}
		DPlayer3D* Player = m_Player.Get();
		if (Player == nullptr)
		{
			return;
		}
		if (Input.WasPressed(EKey::R))
		{
			Player->GetCharacter().Teleport({StartX, StartY, 0});
		}
		if (Input.WasPressed(EKey::O))
		{
			Player->GetCharacter().Teleport({StartX, 0.3f, 0});
		}
	}

private:
	TObjectHandle<DPlayer3D> m_Player;
	// 所有するシーン（所有しない。シーンはオブジェクトより長く生存する）。
	DCharacterSample3DScene* m_pScene;
};
// 箱の四隅（XY平面）。
void Corners_Internal(const FLevelBox& Box, Toolbox::FVector2 (&Out)[4]) noexcept
{
	const Toolbox::f32 Cosine = static_cast<Toolbox::f32>(Toolbox::Cos(Toolbox::f64(Box.Angle)));
	const Toolbox::f32 Sine = static_cast<Toolbox::f32>(Toolbox::Sin(Toolbox::f64(Box.Angle)));
	const Toolbox::FVector2 AxisX{Cosine * Box.Half.X, Sine * Box.Half.X};
	const Toolbox::FVector2 AxisY{-Sine * Box.Half.Y, Cosine * Box.Half.Y};
	Out[0] = Box.Center - AxisX - AxisY;
	Out[1] = Box.Center + AxisX - AxisY;
	Out[2] = Box.Center + AxisX + AxisY;
	Out[3] = Box.Center - AxisX + AxisY;
}
} // namespace

const Toolbox::TArray<FLevelBox, LevelBoxCount>& GetLevelBoxes() noexcept
{
	static const Toolbox::TArray<FLevelBox, LevelBoxCount> Boxes{
	    FLevelBox{{8, -1}, {22, 1}, 0, 0, 10, false},                     // 床（x∈[-14,30]、上面0）
	    FLevelBox{{3, 0.1f}, {1, 0.1f}, 0, 0, 4, false},                  // 低い段差（高さ0.2、上れる）
	    FLevelBox{{-6, 0.3f}, {1, 0.3f}, 0, 0, 4, false},                 // 高い段差（高さ0.6、上れない）
	    FLevelBox{{-2.5f, 1.6f}, {1.5f, 0.5f}, 0, 0, 4, false},           // 低い天井（下面1.1）
	    FLevelBox{{8.848f, 1.067f}, {3, 0.5f}, 0.5235988f, 0, 4, false},  // 30度の坂（x=6から上面3まで）
	    FLevelBox{{13.2f, 1.5f}, {2, 1.5f}, 0, 0, 4, false},              // 坂の上の台（上面3）
	    FLevelBox{{18.433f, 1.482f}, {2, 0.5f}, 1.0471976f, 0, 4, false}, // 60度の急坂（x=17から）
	    FLevelBox{{26, 3}, {1, 3}, 0, 0, 6, false},                       // 右端の壁（x∈[25,27]）
	    FLevelBox{{19, 3}, {7, 3}, 0, 5, 0.5f, true}}; // 3Dの奥の壁（z∈[4.5,5.5]、右端の壁と角を作る）
	return Boxes;
}

DCharacterMovement2DComponent& DPlayer2D::GetCharacter() const
{
	DCharacterMovement2DComponent* Character = m_Character.Get();
	if (Character == nullptr)
	{
		throw Toolbox::FException("2D sample player is not initialized");
	}
	return *Character;
}
TResult<void> DPlayer2D::OnInitialize(const FInitContext&)
{
	FCharacterMovementDescription2D Description;
	Description.Center = {StartX, StartY};
	auto Added = AddComponent<DCharacterMovement2DComponent>(Description);
	if (!Added)
	{
		return TResult<void>::Failure(Added.Error());
	}
	m_Character = Added.Value();
	return {};
}
void DPlayer2D::OnTick(const FTickContext& Context)
{
	DCharacterMovement2DComponent& Character = GetCharacter();
	Character.SetMoveInput({Axis_Internal(Context.Input, EKey::A, EKey::D), 0});
	if (Context.Input.WasPressed(EKey::Space))
	{
		Character.RequestJump();
	}
}

DCharacterMovement3DComponent& DPlayer3D::GetCharacter() const
{
	DCharacterMovement3DComponent* Character = m_Character.Get();
	if (Character == nullptr)
	{
		throw Toolbox::FException("3D sample player is not initialized");
	}
	return *Character;
}
TResult<void> DPlayer3D::OnInitialize(const FInitContext&)
{
	FCharacterMovementDescription3D Description;
	Description.Center = {StartX, StartY, 0};
	auto Added = AddComponent<DCharacterMovement3DComponent>(Description);
	if (!Added)
	{
		return TResult<void>::Failure(Added.Error());
	}
	m_Character = Added.Value();
	return {};
}
void DPlayer3D::OnTick(const FTickContext& Context)
{
	DCharacterMovement3DComponent& Character = GetCharacter();
	Character.SetMoveInput(
	    {Axis_Internal(Context.Input, EKey::A, EKey::D), 0, Axis_Internal(Context.Input, EKey::S, EKey::W)});
	if (Context.Input.WasPressed(EKey::Space))
	{
		Character.RequestJump();
	}
}

FVector2 DCharacterSample2DScene::ToScreen(Toolbox::f32 X, Toolbox::f32 Y) noexcept
{
	return {24 + (X + 14) * PixelsPerMeter, 620 - Y * PixelsPerMeter};
}
TResult<void> DCharacterSample2DScene::OnInitialize(const FInitContext& Context)
{
	auto Font = Context.Assets.LoadFont();
	if (!Font)
	{
		return TResult<void>::Failure(Font.Error());
	}
	m_Font = Font.Value();
	auto Level = Spawn<DLevel2D>();
	if (!Level)
	{
		return TResult<void>::Failure(Level.Error());
	}
	auto Player = Spawn<DPlayer2D>();
	if (!Player)
	{
		return TResult<void>::Failure(Player.Error());
	}
	m_Player = Player.Value();
	auto Director = Spawn<DDirector2D>(m_Player);
	if (!Director)
	{
		return TResult<void>::Failure(Director.Error());
	}
	return {};
}
void DCharacterSample2DScene::OnDraw(FRenderContext& Render) const
{
	auto& Draw = Render.Get2D();
	FDrawStyle Background;
	Background.Color = {18, 20, 26, 255};
	Background.Layer = -10;
	Require_Internal(Draw.FillRectangle({0, 0, 1280, 720}, Background));
	const auto& Boxes = GetLevelBoxes();
	for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
	{
		if (Boxes[Index].bOnly3D)
		{
			continue;
		}
		Toolbox::FVector2 Corners[4];
		Corners_Internal(Boxes[Index], Corners);
		FVector2 Screen[4];
		for (Toolbox::int32 Corner = 0; Corner < 4; ++Corner)
		{
			Screen[Corner] = ToScreen(Corners[Corner].X, Corners[Corner].Y);
		}
		FDrawStyle Style;
		Style.Color = BoxColor_Internal(Index);
		Require_Internal(Draw.FillTriangle(Screen[0], Screen[1], Screen[2], Style));
		Require_Internal(Draw.FillTriangle(Screen[0], Screen[2], Screen[3], Style));
	}
	const DPlayer2D* Player = m_Player.Get();
	if (Player == nullptr)
	{
		return;
	}
	const DCharacterMovement2DComponent& Character = Player->GetCharacter();
	const Toolbox::FVector2 Center = Character.GetRenderCenter();
	FDrawStyle PlayerStyle;
	PlayerStyle.Color = PlayerColor;
	PlayerStyle.Layer = 10;
	Require_Internal(
	    Draw.FillCircle(ToScreen(Center.X, Center.Y), Character.GetSettings().Radius * PixelsPerMeter, PlayerStyle));
	FDrawStyle Text;
	Text.Layer = 100;
	Require_Internal(
	    Draw.DrawText(m_Font,
	                  "Character 2D  [A/D] move  [Space] jump  [R] reset  [O] overlap test  [P] pause  [Tab] 3D  "
	                  "[Esc] quit",
	                  {16, 12}, Text));
	const FCharacterStepResult2D& Step = Character.GetLastStep();
	char Events[64];
	EventText_Internal(Step, Events);
	char Line[256];
	snprintf(Line, sizeof(Line), "pos (%.2f, %.2f)  vel (%.2f, %.2f)  ground %s  steps %lld  %s", Center.X, Center.Y,
	         Character.GetVelocity().X, Character.GetVelocity().Y, GroundName_Internal(Character.GetGround().State),
	         static_cast<long long>(Character.GetStepCount()), GetClock().IsPaused() ? "[PAUSED]" : "");
	Require_Internal(Draw.DrawText(m_Font, Line, {16, 40}, Text));
	snprintf(Line, sizeof(Line), "horizontal %s  vertical %s  overlap %s  queries %d  %s",
	         MoveStopName_Internal(Step.Horizontal.Stop), MoveStopName_Internal(Step.Vertical.Stop),
	         RecoveryName_Internal(Step.Recovery.Status), Step.Queries, Events);
	Require_Internal(Draw.DrawText(m_Font, Line, {16, 68}, Text));
}

FRenderView3D DCharacterSample3DScene::GetView(Toolbox::int32 Side) const
{
	Toolbox::FVector3 Center{StartX, StartY, 0};
	if (const DPlayer3D* Player = m_Player.Get())
	{
		Center = Player->GetCharacter().GetRenderCenter();
	}
	FRenderView3D View;
	View.Id = static_cast<Toolbox::uint64>(Side + 1);
	View.Eye = Center + Toolbox::FVector3{0, 4, -9};
	View.Target = Center + Toolbox::FVector3{0, 0.5f, 0};
	View.NearPlane = 0.1f;
	View.FarPlane = 200;
	if (m_bSplit)
	{
		View.bViewport = true;
		View.Viewport = {Side * 640, 0, (Side + 1) * 640, 720};
		if (Side == 1)
		{
			View.Eye = Center + Toolbox::FVector3{6, 10, -3};
		}
	}
	return View;
}
TResult<void> DCharacterSample3DScene::OnInitialize(const FInitContext& Context)
{
	auto Font = Context.Assets.LoadFont();
	if (!Font)
	{
		return TResult<void>::Failure(Font.Error());
	}
	m_Font = Font.Value();
	auto Level = Spawn<DLevel3D>();
	if (!Level)
	{
		return TResult<void>::Failure(Level.Error());
	}
	auto Player = Spawn<DPlayer3D>();
	if (!Player)
	{
		return TResult<void>::Failure(Player.Error());
	}
	m_Player = Player.Value();
	auto Director = Spawn<DDirector3D>(m_Player, *this);
	if (!Director)
	{
		return TResult<void>::Failure(Director.Error());
	}
	return {};
}
void DCharacterSample3DScene::OnDraw(FRenderContext& Render) const
{
	auto& Draw3D = Render.Get3D();
	const DPlayer3D* Player = m_Player.Get();
	const auto& Boxes = GetLevelBoxes();
	// 描画の数は、更新・物理Step・移動の回数に影響しない（更新は固定更新とOnTickだけで行う）。
	for (Toolbox::int32 Side = 0; Side < (m_bSplit ? 2 : 1); ++Side)
	{
		Require_Internal(Draw3D.SetView(GetView(Side)));
		for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
		{
			const FLevelBox& Box = Boxes[Index];
			Toolbox::FOBB Shape{{Box.Center.X, Box.Center.Y, Box.CenterZ}, {Box.Half.X, Box.Half.Y, Box.HalfZ}};
			const Toolbox::f32 Cosine = static_cast<Toolbox::f32>(Toolbox::Cos(Toolbox::f64(Box.Angle)));
			const Toolbox::f32 Sine = static_cast<Toolbox::f32>(Toolbox::Sin(Toolbox::f64(Box.Angle)));
			Shape.Axes = {Toolbox::FVector3{Cosine, Sine, 0}, Toolbox::FVector3{-Sine, Cosine, 0},
			              Toolbox::FVector3{0, 0, 1}};
			FDrawStyle3D Style;
			Style.Color = BoxColor_Internal(Index);
			Require_Internal(Draw3D.DrawBox(Shape, Style));
		}
		if (Player != nullptr)
		{
			const DCharacterMovement3DComponent& Character = Player->GetCharacter();
			FDrawStyle3D Style;
			Style.Color = PlayerColor;
			Require_Internal(
			    Draw3D.DrawSphere({Character.GetRenderCenter(), Character.GetSettings().Radius}, Style, 24));
		}
	}
	FDrawStyle Text;
	Text.Layer = 100;
	auto& Draw = Render.Get2D();
	Require_Internal(Draw.DrawText(m_Font,
	                               "Character 3D  [A/D/W/S] move  [Space] jump  [R] reset  [O] overlap  [V] split  [P] "
	                               "pause  [Tab] 2D  [Esc] quit",
	                               {16, 12}, Text));
	if (Player == nullptr)
	{
		return;
	}
	const DCharacterMovement3DComponent& Character = Player->GetCharacter();
	const Toolbox::FVector3 Center = Character.GetRenderCenter();
	const FCharacterStepResult3D& Step = Character.GetLastStep();
	char Events[64];
	EventText_Internal(Step, Events);
	char Line[256];
	snprintf(Line, sizeof(Line), "pos (%.2f, %.2f, %.2f)  vel (%.2f, %.2f, %.2f)  ground %s  steps %lld  %s", Center.X,
	         Center.Y, Center.Z, Character.GetVelocity().X, Character.GetVelocity().Y, Character.GetVelocity().Z,
	         GroundName_Internal(Character.GetGround().State), static_cast<long long>(Character.GetStepCount()),
	         GetClock().IsPaused() ? "[PAUSED]" : "");
	Require_Internal(Draw.DrawText(m_Font, Line, {16, 40}, Text));
	snprintf(Line, sizeof(Line), "horizontal %s  vertical %s  overlap %s  queries %d  %s",
	         MoveStopName_Internal(Step.Horizontal.Stop), MoveStopName_Internal(Step.Vertical.Stop),
	         RecoveryName_Internal(Step.Recovery.Status), Step.Queries, Events);
	Require_Internal(Draw.DrawText(m_Font, Line, {16, 68}, Text));
}
} // namespace Dxf::GameplaySample
