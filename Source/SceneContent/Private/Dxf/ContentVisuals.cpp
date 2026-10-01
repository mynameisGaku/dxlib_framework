// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ContentVisuals.h"
#include "Dxf/ViewCoordinates.h"
#include "Toolbox/Utility.h"
namespace Dxf::ContentPrivate
{
// 受付失敗も既存Sceneの描画エラー経路へ伝える。
void RequireDraw(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
void FContentVisuals::Reserve(Toolbox::size_t Count)
{
	m_Entries.Reserve(Count);
}
void FContentVisuals::Add(const FContentVisualDefinition& Visual, const FContentResources& Resources, FAssetService& Assets)
{
	// Native個体も親の初期化失敗時に回収できるよう、この領域へだけ追加する。
	FEntry Entry;
	if (Visual.Font >= 0)
	{
		Entry.Label = FSharedText(Visual.Label);
	}
	if (Visual.Kind == EContentVisualKind::Model)
	{
		auto Created = Assets.CreateModelInstance(Resources.GetModel(static_cast<Toolbox::uint32>(Visual.Asset)));
		if (!Created)
		{
			throw Toolbox::FException(Created.Error().Message);
		}
		Entry.Model = Toolbox::Move(Created.Value());
		RequireDraw(Entry.Model.SetMaterial(Visual.Material));
		if (Visual.Animation >= 0)
		{
			RequireDraw(Entry.Model.Play(static_cast<Toolbox::size_t>(Visual.Animation)));
		}
	}
	m_Entries.PushBack(Toolbox::Move(Entry));
}
void FContentVisuals::Advance(Toolbox::f64 Seconds)
{
	for (auto& Entry : m_Entries)
	{
		if (Entry.Model.IsValid())
		{
			RequireDraw(Entry.Model.Advance(Seconds));
		}
	}
}
void FContentVisuals::Draw2D(Toolbox::size_t Index, const FContentVisualDefinition& Visual, const FContentResources& Resources, Toolbox::FVector2 Position, Toolbox::f32 Angle, const FContentView2D& View, FRenderContext& Render) const
{
	// 物理のX右・Y上を画素のX右・Y下へ変換する。
	const auto Screen = [&View](Toolbox::FVector2 P)
	{
		return FVector2{View.Origin.X + P.X * View.PixelsPerMeter, View.Origin.Y - P.Y * View.PixelsPerMeter};
	};
	FDrawStyle Style;
	Style.Color = Visual.Color;
	Style.Layer = Visual.Layer;
	Style.bClip = View.bClip;
	Style.ClipRect = View.ClipRect;
	const auto Center = Screen(Position);
	if (Visual.Kind == EContentVisualKind::Box)
	{
		const auto C = static_cast<Toolbox::f32>(Toolbox::Cos(Angle));
		const auto S = static_cast<Toolbox::f32>(Toolbox::Sin(Angle));
		const auto Corner = [C, S, Position, &Screen](Toolbox::f32 X, Toolbox::f32 Y)
		{
			return Screen(Position + Toolbox::FVector2{C * X - S * Y, S * X + C * Y});
		};
		const auto A = Corner(-Visual.HalfExtents.X, -Visual.HalfExtents.Y);
		const auto B = Corner(Visual.HalfExtents.X, -Visual.HalfExtents.Y);
		const auto C1 = Corner(Visual.HalfExtents.X, Visual.HalfExtents.Y);
		const auto D = Corner(-Visual.HalfExtents.X, Visual.HalfExtents.Y);
		RequireDraw(Render.Get2D().FillTriangle(A, B, C1, Style));
		RequireDraw(Render.Get2D().FillTriangle(A, C1, D, Style));
	}
	else if (Visual.Kind == EContentVisualKind::Sphere)
	{
		RequireDraw(Render.Get2D().FillCircle(Center, Visual.Radius * View.PixelsPerMeter, Style));
	}
	else if (Visual.Kind == EContentVisualKind::Texture)
	{
		const auto& Texture = Resources.GetTexture(static_cast<Toolbox::uint32>(Visual.Asset));
		FSpriteDrawOptions Options;
		static_cast<FDrawStyle&>(Options) = Style;
		Options.Scale = {2 * Visual.HalfExtents.X * View.PixelsPerMeter / Texture.GetWidth(),
		                 2 * Visual.HalfExtents.Y * View.PixelsPerMeter / Texture.GetHeight()};
		Options.Pivot = {static_cast<Toolbox::f32>(Texture.GetWidth()) / 2,
		                 static_cast<Toolbox::f32>(Texture.GetHeight()) / 2};
		Options.RotationRadians = -Angle;
		Options.Blend = Texture.IsPremultipliedAlpha() ? EBlendMode2D::PremultipliedAlpha : EBlendMode2D::Alpha;
		RequireDraw(Render.Get2D().DrawSprite(Texture, Center, Options));
	}
	if (Visual.Font >= 0)
	{
		Style.Blend = Resources.GetFont(static_cast<Toolbox::uint32>(Visual.Font))
		                      .GetResource_Internal()
		                      ->GetMetadata()
		                      .bPremultipliedAlpha
		                  ? EBlendMode2D::PremultipliedAlpha
		                  : EBlendMode2D::Alpha;
		RequireDraw(Render.Get2D().DrawText(Resources.GetFont(static_cast<Toolbox::uint32>(Visual.Font)), m_Entries[Index].Label, Center, Style));
	}
}
void FContentVisuals::Draw3D(Toolbox::size_t Index, const FContentVisualDefinition& Visual, const FContentResources& Resources, Toolbox::FVector3 Position, Toolbox::FQuaternion Rotation, FRenderContext& Render) const
{
	FDrawStyle3D Style;
	Style.Color = Visual.Color;
	if (Visual.Kind == EContentVisualKind::Box)
	{
		Toolbox::FOBB Box;
		Box.Center = Position;
		Box.HalfExtents = Visual.HalfExtents;
		Box.Axes[0] = Rotation.Rotate({1, 0, 0});
		Box.Axes[1] = Rotation.Rotate({0, 1, 0});
		Box.Axes[2] = Rotation.Rotate({0, 0, 1});
		RequireDraw(Render.Get3D().DrawBox(Box, Style));
	}
	else if (Visual.Kind == EContentVisualKind::Sphere)
	{
		RequireDraw(Render.Get3D().DrawSphere({Position, Visual.Radius}, Style));
	}
	else if (Visual.Kind == EContentVisualKind::Model)
	{
		auto& Model = m_Entries[Index].Model;
		const auto World = Toolbox::FMatrix4::Translation(Position) * Rotation.ToMatrix() *
		                   Toolbox::FMatrix4::Scale(Visual.ModelScale);
		RequireDraw(Model.SetTransform(World));
		RequireDraw(Render.Get3D().DrawModel(Model));
	}
	if (Visual.Font >= 0)
	{
		const auto Projected =
		    ProjectWorldToScreen(Render.Get3D().GetView(), Render.GetTargetWidth(), Render.GetTargetHeight(), Position);
		if (!Projected)
		{
			throw Toolbox::FException(Projected.Error().Message);
		}
		if (Projected.Value().bInsideView)
		{
			FDrawStyle TextStyle;
			TextStyle.Color = Visual.Color;
			TextStyle.Layer = Visual.Layer;
			TextStyle.Blend = Resources.GetFont(static_cast<Toolbox::uint32>(Visual.Font))
			                          .GetResource_Internal()
			                          ->GetMetadata()
			                          .bPremultipliedAlpha
			                      ? EBlendMode2D::PremultipliedAlpha
			                      : EBlendMode2D::Alpha;
			const auto& View = Render.Get3D().GetView();
			TextStyle.bClip = View.bViewport;
			TextStyle.ClipRect = View.Viewport;
			RequireDraw(Render.Get2D().DrawText(Resources.GetFont(static_cast<Toolbox::uint32>(Visual.Font)), m_Entries[Index].Label, Projected.Value().Screen, TextStyle));
		}
	}
}
void FContentVisuals::Clear() noexcept
{
	m_Entries.Clear();
}
} // namespace Dxf::ContentPrivate
