// SPDX-License-Identifier: NOASSERTION
#include "UiPlay3DScene.h"
#include "UiSampleViews.h"
#include "../GameplaySample/SampleLevel.h"
namespace Dxf::UiSample
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
} // namespace
DUiPlay3DScene::DUiPlay3DScene(Toolbox::TSharedPtr<FUiSampleState> State) : m_Ui(Toolbox::Move(State))
{
}

TResult<void> DUiPlay3DScene::OnInitialize(const FInitContext& Context)
{
	auto Level = Spawn<GameplaySample::DLevel3D>();
	if (!Level)
	{
		return TResult<void>::Failure(Level.Error());
	}
	auto Player = Spawn<GameplaySample::DPlayer3D>();
	if (!Player)
	{
		return TResult<void>::Failure(Player.Error());
	}
	m_Player = Player.Value();
	return m_Ui.Initialize(*this, Context.Assets, false, true,
	                       [this]()
	                       {
		                       const auto& Character = m_Player.Get()->GetCharacter();
		                       return Toolbox::FString("3D Step ") + Toolbox::ToString(Character.GetStepCount()) +
		                              " x=" + Toolbox::ToString(Character.GetCenter().X) +
		                              (Character.IsGrounded() ? " Grounded" : " Airborne");
	                       });
}

void DUiPlay3DScene::OnDraw(FRenderContext& Render) const
{
	const auto Center = m_Player.Get()->GetCharacter().GetRenderCenter();
	const bool Split = m_Ui.GetState()->Split.Get();
	m_Ui.SetWorldMarker(Center);
	Require_Internal(m_Ui.PreparePanels(Render));
	for (const auto& View : MakeSampleViews(Split))
	{
		Require_Internal(Render.Get3D().SetView(View));
		FDrawStyle3D Style;
		for (Toolbox::size_t I = 0; I < GameplaySample::LevelBoxCount; ++I)
		{
			Style.Color = GameplaySample::LevelBoxColor(I);
			Require_Internal(
			    Render.Get3D().DrawBox(GameplaySample::LevelBoxShape3D(GameplaySample::GetLevelBoxes()[I]), Style));
		}
		Style.Color = GameplaySample::PlayerColor;
		Require_Internal(Render.Get3D().DrawSphere({Center, 0.5f}, Style));
		Require_Internal(m_Ui.DrawPanels(Render, View));
	}
	Require_Internal(m_Ui.Draw(Render));
}
} // namespace Dxf::UiSample
