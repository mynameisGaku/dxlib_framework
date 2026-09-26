// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiRenderer.h"
namespace Dxf
{
// 描画の列を2D命令へ変えて送る。
TResult<FUiRenderStats> SubmitUiDrawList(FRender2DContext& Render, const FUiDrawList& List, const FUiSurface& Surface,
                                         const FUiRenderOptions& Options)
{
	FUiRenderStats Stats;
	Stats.NextOrder = Options.OrderBase;
	if (!Surface.IsDisplayable())
	{
		return TResult<FUiRenderStats>::Success(Stats);
	}
	if (List.GetItems().Size() >
	    static_cast<Toolbox::size_t>(static_cast<Toolbox::int64>(Toolbox::TNumericLimits<Toolbox::int32>::Max()) -
	                                 Options.OrderBase))
	{
		return TResult<FUiRenderStats>::Failure(EErrorCode::InvalidArgument, "UI draw order overflow");
	}
	const FUiPixelRect Area = Surface.GetPixelRect();
	for (const FUiDrawItem& Item : List.GetItems())
	{
		if (Item.Kind > EUiDrawKind::Image || !Toolbox::IsFinite(Item.Opacity) || Item.Opacity < 0 ||
		    Item.Opacity > 1 || !Toolbox::IsFinite(Item.Rect.X) || !Toolbox::IsFinite(Item.Rect.Y) ||
		    !Toolbox::IsFinite(Item.Rect.Width) || !Toolbox::IsFinite(Item.Rect.Height) || Item.Rect.Width < 0 ||
		    Item.Rect.Height < 0 || !Toolbox::IsFinite(Item.Clip.X) || !Toolbox::IsFinite(Item.Clip.Y) ||
		    !Toolbox::IsFinite(Item.Clip.Width) || !Toolbox::IsFinite(Item.Clip.Height) || Item.Clip.Width < 0 ||
		    Item.Clip.Height < 0)
		{
			return TResult<FUiRenderStats>::Failure(EErrorCode::InvalidArgument, "Invalid UI draw item");
		}
		// 要素のクリップと表示面の共通部分（画素）。空なら送らない（全体への復帰と取り違えない）。
		const FUiPixelRect Clip = Surface.ToPixel(Item.Clip).Intersect(Area);
		if (Clip.IsEmpty())
		{
			++Stats.SkippedClipped;
			continue;
		}
		FDrawStyle Style;
		Style.Color = Item.Color;
		Style.Opacity = Item.Opacity;
		Style.Layer = Options.Layer;
		Style.Order = Stats.NextOrder;
		Style.ClipRect = {Clip.Left, Clip.Top, Clip.Right, Clip.Bottom};
		Style.bClip = true;
		TResult<void> Result;
		switch (Item.Kind)
		{
		case EUiDrawKind::Rectangle:
		{
			const FUiPixelRect Rect = Surface.ToPixel(Item.Rect);
			if (Rect.IsEmpty())
			{
				continue;
			}
			Result = Render.FillRectangle({Rect.Left, Rect.Top, Rect.Right, Rect.Bottom}, Style);
			break;
		}
		case EUiDrawKind::Text:
		{
			if (!Item.Font.IsValid())
			{
				return TResult<FUiRenderStats>::Failure(EErrorCode::InvalidState, "UI draw resource invalidated");
			}
			const FVector2 Pixel = Surface.ToPixel(FVector2{Item.Rect.X, Item.Rect.Y});
			Result = Render.DrawText(Item.Font, Item.Text,
			                         {static_cast<Toolbox::f32>(Toolbox::Floor(Pixel.X)), static_cast<Toolbox::f32>(Toolbox::Floor(Pixel.Y))},
			                         Style);
			break;
		}
		case EUiDrawKind::Image:
		{
			if (!Item.Texture.IsValid() || Item.Texture.GetWidth() <= 0 || Item.Texture.GetHeight() <= 0)
			{
				return TResult<FUiRenderStats>::Failure(EErrorCode::InvalidState, "UI draw resource invalidated");
			}
			const FUiPixelRect Rect = Surface.ToPixel(Item.Rect);
			if (Rect.IsEmpty())
			{
				continue;
			}
			FSpriteDrawOptions Sprite;
			static_cast<FDrawStyle&>(Sprite) = Style;
			Sprite.Scale = {static_cast<Toolbox::f32>(Rect.Width()) / static_cast<Toolbox::f32>(Item.Texture.GetWidth()),
			                static_cast<Toolbox::f32>(Rect.Height()) / static_cast<Toolbox::f32>(Item.Texture.GetHeight())};
			Result = Render.DrawSprite(Item.Texture, {static_cast<Toolbox::f32>(Rect.Left), static_cast<Toolbox::f32>(Rect.Top)}, Sprite);
			break;
		}
		}
		if (!Result)
		{
			return TResult<FUiRenderStats>::Failure(Result.Error());
		}
		++Stats.Commands;
		++Stats.NextOrder;
	}
	return TResult<FUiRenderStats>::Success(Stats);
}
} // namespace Dxf
