// SPDX-License-Identifier: NOASSERTION
#include "CharacterSample2DScene.h"
#include "CharacterSample3DScene.h"
#include "SampleHud.h"
#include "SampleLevel.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderContext.h"
#include "Dxf/SceneNavigator.h"
namespace Dxf::GameplaySample
{
namespace
{
// 1画面の縮尺（ピクセル毎メートル）。
constexpr Toolbox::f32 PixelsPerMeter = 28;
// 2画面の各画面の縮尺（地形全体を画面の半分へ収める）。
constexpr Toolbox::f32 SplitPixelsPerMeter = 14;

// 2Dの操作（切替・リセット・重なりの確認・一時停止・2画面・歩行キャラクターの追加と破棄・終了）。一時停止中も受け付ける。
class DDirector2D final : public DGameObject
{
public:
	explicit DDirector2D(DCharacterSample2DScene& Scene) : m_pScene(&Scene)
	{
		SetTickWhenPaused(true);
	}

protected:
	void OnTick(const FTickContext& Context) override
	{
		const FInputSnapshot& Input = Context.Input;
		if (!Context.Time.bPaused)
		{
			m_pScene->AdvanceAnimation(Context.Time.DeltaSeconds);
		}
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::Escape))
		{
			Context.Scenes->RequestQuit();
			return;
		}
		if (Context.Scenes != nullptr && Input.WasPressed(EKey::Tab))
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
		if (Input.WasPressed(EKey::N))
		{
			(void)m_pScene->SpawnWalker();
		}
		if (Input.WasPressed(EKey::M))
		{
			(void)m_pScene->DestroyWalker();
		}
		DPlayer2D* Player = m_pScene->GetPlayer().Get();
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
	// 所有するシーン（所有しない。シーンはオブジェクトより長く生存する）。
	DCharacterSample2DScene* m_pScene;
};
} // namespace

FVector2 DCharacterSample2DScene::ToScreen(Toolbox::f32 X, Toolbox::f32 Y) noexcept
{
	return {24 + (X + 14) * PixelsPerMeter, 620 - Y * PixelsPerMeter};
}
bool DCharacterSample2DScene::SpawnWalker()
{
	if (m_WalkerCount == MaxWalkers)
	{
		return false;
	}
	// 高い段差より左（x∈[-13,-8]）を往復する。二体目以降は開始位置を少しずつずらす。
	const Toolbox::f32 X = -12.5f + static_cast<Toolbox::f32>(m_WalkersSpawned % 4) * 1.2f;
	auto Walker = Spawn<DWalker2D>(Toolbox::FVector2{X, StartY}, -13.0f, -8.0f);
	if (!Walker)
	{
		RequireSample(TResult<void>::Failure(Walker.Error()));
	}
	m_Walkers[static_cast<Toolbox::size_t>(m_WalkerCount)] = Walker.Value();
	++m_WalkerCount;
	++m_WalkersSpawned;
	return true;
}
bool DCharacterSample2DScene::DestroyWalker() noexcept
{
	if (m_WalkerCount == 0)
	{
		return false;
	}
	--m_WalkerCount;
	if (DWalker2D* Walker = m_Walkers[static_cast<Toolbox::size_t>(m_WalkerCount)].Get())
	{
		Walker->Destroy();
	}
	m_Walkers[static_cast<Toolbox::size_t>(m_WalkerCount)] = {};
	return true;
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
	auto Director = Spawn<DDirector2D>(*this);
	if (!Director)
	{
		return TResult<void>::Failure(Director.Error());
	}
	return {};
}
void DCharacterSample2DScene::DrawView_Internal(FRenderContext& Render, Toolbox::f32 OriginX, Toolbox::f32 Scale) const
{
	auto& Draw = Render.Get2D();
	// 1画面は従来の変換（ToScreen）、2画面は各半分へ地形全体を収める変換。
	const bool bSingle = Scale == PixelsPerMeter;
	auto Point = [&](Toolbox::f32 X, Toolbox::f32 Y) -> FVector2
	{
		if (bSingle)
		{
			return ToScreen(X, Y);
		}
		return {OriginX + 8 + (X + 14) * Scale, 500 - Y * Scale};
	};
	const auto& Boxes = GetLevelBoxes();
	for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
	{
		if (Boxes[Index].bOnly3D)
		{
			continue;
		}
		Toolbox::FVector2 Corners[4];
		LevelBoxCorners(Boxes[Index], Corners);
		FVector2 Screen[4];
		for (Toolbox::int32 Corner = 0; Corner < 4; ++Corner)
		{
			Screen[Corner] = Point(Corners[Corner].X, Corners[Corner].Y);
		}
		FDrawStyle Style;
		Style.Color = LevelBoxColor(Index);
		RequireSample(Draw.FillTriangle(Screen[0], Screen[1], Screen[2], Style));
		RequireSample(Draw.FillTriangle(Screen[0], Screen[2], Screen[3], Style));
	}
	for (Toolbox::int32 Index = 0; Index < m_WalkerCount; ++Index)
	{
		if (const DWalker2D* Walker = GetWalker(Index).Get(); Walker != nullptr && Walker->IsInitialized())
		{
			const Toolbox::FVector2 Center = Walker->GetCharacter().GetRenderCenter();
			FDrawStyle Style;
			Style.Color = WalkerColor;
			Style.Layer = 9;
			RequireSample(
			    Draw.FillCircle(Point(Center.X, Center.Y), Walker->GetCharacter().GetSettings().Radius * Scale, Style));
		}
	}
	if (const DPlayer2D* Player = m_Player.Get())
	{
		const DCharacterMovement2DComponent& Character = Player->GetCharacter();
		const Toolbox::FVector2 Center = Character.GetRenderCenter();
		FDrawStyle Style;
		Style.Color = PlayerColor;
		Style.Layer = 10;
		RequireSample(Draw.FillCircle(Point(Center.X, Center.Y), Character.GetSettings().Radius * Scale, Style));
	}
}
void DCharacterSample2DScene::OnDraw(FRenderContext& Render) const
{
	auto& Draw = Render.Get2D();
	FDrawStyle Background;
	Background.Color = {18, 20, 26, 255};
	Background.Layer = -10;
	RequireSample(Draw.FillRectangle({0, 0, 1280, 720}, Background));
	// 描画の数は、更新・物理Step・移動の回数に影響しない（更新は固定更新とOnTickだけで行う）。
	if (m_bSplit)
	{
		DrawView_Internal(Render, 0, SplitPixelsPerMeter);
		DrawView_Internal(Render, 640, SplitPixelsPerMeter);
	}
	else
	{
		DrawView_Internal(Render, 0, PixelsPerMeter);
	}
	FDrawStyle Text;
	Text.Layer = 100;
	RequireSample(
	    Draw.DrawText(m_Font,
	                  "Character 2D  [A/D] move  [Space] jump  [R] reset  [O] overlap  [N/M] add/remove walker  "
	                  "[V] split  [P] pause  [Tab] 3D  [Esc] quit",
	                  {16, 12}, Text));
	const DPlayer2D* Player = m_Player.Get();
	if (Player == nullptr)
	{
		return;
	}
	const DCharacterMovement2DComponent& Character = Player->GetCharacter();
	const Toolbox::FVector2 Center = Character.GetRenderCenter();
	const FCharacterStepResult2D& Step = Character.GetLastStep();
	char Events[64];
	StepEventText(Step, Events);
	char Line[256];
	snprintf(Line, sizeof(Line), "pos (%.2f, %.2f)  vel (%.2f, %.2f)  ground %s  steps %lld  walkers %d  %s", Center.X,
	         Center.Y, Character.GetVelocity().X, Character.GetVelocity().Y, GroundName(Character.GetGround().State),
	         static_cast<long long>(Character.GetStepCount()), m_WalkerCount, GetClock().IsPaused() ? "[PAUSED]" : "");
	RequireSample(Draw.DrawText(m_Font, Line, {16, 40}, Text));
	snprintf(Line, sizeof(Line), "horizontal %s  vertical %s  overlap %s  queries %d  %s",
	         MoveStopName(Step.Horizontal.Stop), MoveStopName(Step.Vertical.Stop), RecoveryName(Step.Recovery.Status),
	         Step.Queries, Events);
	RequireSample(Draw.DrawText(m_Font, Line, {16, 68}, Text));
}
} // namespace Dxf::GameplaySample
