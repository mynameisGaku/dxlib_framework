// SPDX-License-Identifier: NOASSERTION
#include "CharacterSample3DScene.h"
#include "CharacterSample2DScene.h"
#include "SampleHud.h"
#include "SampleLevel.h"
#include "Dxf/AssetService.h"
#include "Dxf/RenderContext.h"
#include "Dxf/SceneNavigator.h"
namespace Dxf::GameplaySample
{
namespace
{
// 3Dの操作。[V]で2画面を切り替える。その他は2Dと同じ。一時停止中も受け付ける。
class DDirector3D final : public DGameObject
{
public:
	explicit DDirector3D(DCharacterSample3DScene& Scene) : m_pScene(&Scene)
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
		if (Input.WasPressed(EKey::N))
		{
			(void)m_pScene->SpawnWalker();
		}
		if (Input.WasPressed(EKey::M))
		{
			(void)m_pScene->DestroyWalker();
		}
		DPlayer3D* Player = m_pScene->GetPlayer().Get();
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
	// 所有するシーン（所有しない。シーンはオブジェクトより長く生存する）。
	DCharacterSample3DScene* m_pScene;
};
} // namespace

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
bool DCharacterSample3DScene::SpawnWalker()
{
	if (m_WalkerCount == MaxWalkers)
	{
		return false;
	}
	// 高い段差より左（x∈[-13,-8]）を往復する。Zをずらして互いに重ならないようにする。
	const Toolbox::f32 Z = -3.0f + static_cast<Toolbox::f32>(m_WalkersSpawned % 6) * 1.2f;
	auto Walker = Spawn<DWalker3D>(Toolbox::FVector3{-12.5f, StartY, Z}, -13.0f, -8.0f);
	if (!Walker)
	{
		RequireSample(TResult<void>::Failure(Walker.Error()));
	}
	m_Walkers[static_cast<Toolbox::size_t>(m_WalkerCount)] = Walker.Value();
	++m_WalkerCount;
	++m_WalkersSpawned;
	return true;
}
bool DCharacterSample3DScene::DestroyWalker() noexcept
{
	if (m_WalkerCount == 0)
	{
		return false;
	}
	--m_WalkerCount;
	if (DWalker3D* Walker = m_Walkers[static_cast<Toolbox::size_t>(m_WalkerCount)].Get())
	{
		Walker->Destroy();
	}
	m_Walkers[static_cast<Toolbox::size_t>(m_WalkerCount)] = {};
	return true;
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
	auto Director = Spawn<DDirector3D>(*this);
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
		RequireSample(Draw3D.SetView(GetView(Side)));
		for (Toolbox::size_t Index = 0; Index < Boxes.Size(); ++Index)
		{
			FDrawStyle3D Style;
			Style.Color = LevelBoxColor(Index);
			RequireSample(Draw3D.DrawBox(LevelBoxShape3D(Boxes[Index]), Style));
		}
		for (Toolbox::int32 Index = 0; Index < m_WalkerCount; ++Index)
		{
			if (const DWalker3D* Walker = GetWalker(Index).Get(); Walker != nullptr && Walker->IsInitialized())
			{
				FDrawStyle3D Style;
				Style.Color = WalkerColor;
				RequireSample(Draw3D.DrawSphere(
				    {Walker->GetCharacter().GetRenderCenter(), Walker->GetCharacter().GetSettings().Radius}, Style,
				    16));
			}
		}
		if (Player != nullptr)
		{
			const DCharacterMovement3DComponent& Character = Player->GetCharacter();
			FDrawStyle3D Style;
			Style.Color = PlayerColor;
			RequireSample(Draw3D.DrawSphere({Character.GetRenderCenter(), Character.GetSettings().Radius}, Style, 24));
		}
	}
	FDrawStyle Text;
	Text.Layer = 100;
	auto& Draw = Render.Get2D();
	RequireSample(Draw.DrawText(m_Font,
	                            "Character 3D  [A/D/W/S] move  [Space] jump  [R] reset  [O] overlap  [N/M] add/remove "
	                            "walker  [V] split  [P] pause  [Tab] 2D  [Esc] quit",
	                            {16, 12}, Text));
	if (Player == nullptr)
	{
		return;
	}
	const DCharacterMovement3DComponent& Character = Player->GetCharacter();
	const Toolbox::FVector3 Center = Character.GetRenderCenter();
	const FCharacterStepResult3D& Step = Character.GetLastStep();
	char Events[64];
	StepEventText(Step, Events);
	char Line[256];
	snprintf(Line, sizeof(Line),
	         "pos (%.2f, %.2f, %.2f)  vel (%.2f, %.2f, %.2f)  ground %s  steps %lld  walkers %d  %s", Center.X,
	         Center.Y, Center.Z, Character.GetVelocity().X, Character.GetVelocity().Y, Character.GetVelocity().Z,
	         GroundName(Character.GetGround().State), static_cast<long long>(Character.GetStepCount()), m_WalkerCount,
	         GetClock().IsPaused() ? "[PAUSED]" : "");
	RequireSample(Draw.DrawText(m_Font, Line, {16, 40}, Text));
	snprintf(Line, sizeof(Line), "horizontal %s  vertical %s  overlap %s  queries %d  %s",
	         MoveStopName(Step.Horizontal.Stop), MoveStopName(Step.Vertical.Stop), RecoveryName(Step.Recovery.Status),
	         Step.Queries, Events);
	RequireSample(Draw.DrawText(m_Font, Line, {16, 68}, Text));
}
} // namespace Dxf::GameplaySample
