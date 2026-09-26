// SPDX-License-Identifier: NOASSERTION
#include "UiPlay2DScene.h"
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
DUiPlay2DScene::DUiPlay2DScene(Toolbox::TSharedPtr<FUiSampleState> State) : m_Ui(Toolbox::Move(State))
{
}

TResult<void> DUiPlay2DScene::OnInitialize(const FInitContext& Context)
{
	auto Level = Spawn<GameplaySample::DLevel2D>();
	if (!Level)
	{
		return TResult<void>::Failure(Level.Error());
	}
	auto Player = Spawn<GameplaySample::DPlayer2D>();
	if (!Player)
	{
		return TResult<void>::Failure(Player.Error());
	}
	m_Player = Player.Value();
	return m_Ui.Initialize(*this, Context.Assets, false, false,
	                       [this]()
	                       {
		                       const auto& Character = m_Player.Get()->GetCharacter();
		                       return Toolbox::FString("2D Step ") + Toolbox::ToString(Character.GetStepCount()) +
		                              " x=" + Toolbox::ToString(Character.GetCenter().X) +
		                              (Character.IsGrounded() ? " Grounded" : " Airborne");
	                       });
}

void DUiPlay2DScene::OnDraw(FRenderContext& Render) const
{
	const auto Center = m_Player.Get()->GetCharacter().GetRenderCenter();
	const bool Split = m_Ui.GetState()->Split.Get();
	m_Ui.SetWorldMarker({Center.X, Center.Y, 0});
	for (Toolbox::int32 View = 0; View < (Split ? 2 : 1); ++View)
	{
		const auto Transform = MakeSampleTransform2D(Split, View);
		FDrawStyle Style;
		Style.bClip = Split;
		Style.ClipRect = {View * 640, 0, (View + 1) * 640, 720};
		Style.Layer = -100;
		for (Toolbox::size_t I = 0; I < GameplaySample::LevelBoxCount; ++I)
		{
			const auto& Box = GameplaySample::GetLevelBoxes()[I];
			if (Box.bOnly3D)
			{
				continue;
			}
			Toolbox::FVector2 Points[4];
			GameplaySample::LevelBoxCorners(Box, Points);
			Style.Color = GameplaySample::LevelBoxColor(I);
			Require_Internal(Render.Get2D().FillTriangle(Transform.ToScreen({Points[0].X, Points[0].Y}),
			                                             Transform.ToScreen({Points[1].X, Points[1].Y}),
			                                             Transform.ToScreen({Points[2].X, Points[2].Y}), Style));
			Require_Internal(Render.Get2D().FillTriangle(Transform.ToScreen({Points[0].X, Points[0].Y}),
			                                             Transform.ToScreen({Points[2].X, Points[2].Y}),
			                                             Transform.ToScreen({Points[3].X, Points[3].Y}), Style));
		}
		Style.Color = GameplaySample::PlayerColor;
		Require_Internal(
		    Render.Get2D().FillCircle(Transform.ToScreen({Center.X, Center.Y}), Transform.PixelsPerUnit * 0.5f, Style));
	}
	Require_Internal(m_Ui.Draw(Render));
}
} // namespace Dxf::UiSample
