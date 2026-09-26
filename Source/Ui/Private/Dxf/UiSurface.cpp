// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiSurface.h"
namespace Dxf
{
// 表示領域と倍率の設定から変換を決める。
FUiSurface::FUiSurface(FUiPixelRect PixelRect, const FUiScaleSettings& Settings) : m_PixelRect(PixelRect)
{
	const Toolbox::f64 Width = PixelRect.Width();
	const Toolbox::f64 Height = PixelRect.Height();
	if (static_cast<Toolbox::int64>(PixelRect.Right) - PixelRect.Left >
	        Toolbox::TNumericLimits<Toolbox::int32>::Max() ||
	    static_cast<Toolbox::int64>(PixelRect.Bottom) - PixelRect.Top >
	        Toolbox::TNumericLimits<Toolbox::int32>::Max() ||
	    Settings.Mode > EUiScaleMode::FixedPixel || PixelRect.IsEmpty() || !Toolbox::IsFinite(Settings.UserScale) ||
	    !(Settings.UserScale > 0) || !Toolbox::IsFinite(Settings.MinScale) || !Toolbox::IsFinite(Settings.MaxScale) ||
	    !(Settings.MinScale > 0) || Settings.MaxScale < Settings.MinScale)
	{
		return;
	}
	// 倍率の候補。
	Toolbox::f64 Scale = Settings.UserScale;
	if (Settings.Mode == EUiScaleMode::ReferenceHeight)
	{
		if (!Toolbox::IsFinite(Settings.ReferenceHeight) || !(Settings.ReferenceHeight > 0))
		{
			return;
		}
		Scale = Height / Settings.ReferenceHeight * Settings.UserScale;
	}
	Scale = Toolbox::Clamp<Toolbox::f64>(Scale, Settings.MinScale, Settings.MaxScale);
	if (!Toolbox::IsFinite(Scale) || !(Scale > 0))
	{
		return;
	}
	m_Scale = static_cast<Toolbox::f32>(Scale);
	m_LogicalSize = {static_cast<Toolbox::f32>(Width / Scale), static_cast<Toolbox::f32>(Height / Scale)};
	m_bDisplayable = true;
}
// 論理座標の点を画素座標へ変える。
FVector2 FUiSurface::ToPixel(FVector2 Point) const noexcept
{
	return {static_cast<Toolbox::f32>(m_PixelRect.Left + static_cast<Toolbox::f64>(Point.X) * m_Scale),
	        static_cast<Toolbox::f32>(m_PixelRect.Top + static_cast<Toolbox::f64>(Point.Y) * m_Scale)};
}
// 論理座標の矩形を画素矩形へ変える。
FUiPixelRect FUiSurface::ToPixel(const FUiRect& Rect) const noexcept
{
	// 辺ごとの画素座標（切り捨て）。範囲外は表示領域の外側の値に抑える。
	auto Edge = [&](Toolbox::f64 Logical, Toolbox::int32 Origin) -> Toolbox::int32
	{
		const Toolbox::f64 Pixel = Toolbox::Floor(Origin + Logical * m_Scale);
		if (!Toolbox::IsFinite(Pixel))
		{
			return Origin;
		}
		return static_cast<Toolbox::int32>(Toolbox::Clamp<Toolbox::f64>(Pixel, -1.0e9, 1.0e9));
	};
	FUiPixelRect Result{Edge(Rect.X, m_PixelRect.Left), Edge(Rect.Y, m_PixelRect.Top),
	                    Edge(static_cast<Toolbox::f64>(Rect.X) + Rect.Width, m_PixelRect.Left),
	                    Edge(static_cast<Toolbox::f64>(Rect.Y) + Rect.Height, m_PixelRect.Top)};
	if (Result.Right < Result.Left)
	{
		Result.Right = Result.Left;
	}
	if (Result.Bottom < Result.Top)
	{
		Result.Bottom = Result.Top;
	}
	return Result;
}
// 画素座標の点を論理座標へ変える。
FVector2 FUiSurface::ToLogical(FVector2 Point) const noexcept
{
	return {static_cast<Toolbox::f32>((static_cast<Toolbox::f64>(Point.X) - m_PixelRect.Left) / m_Scale),
	        static_cast<Toolbox::f32>((static_cast<Toolbox::f64>(Point.Y) - m_PixelRect.Top) / m_Scale)};
}
// 文字の画素の大きさ。
Toolbox::int32 FUiSurface::ToFontPixelSize(Toolbox::f32 LogicalSize) const noexcept
{
	const Toolbox::f64 Pixel = static_cast<Toolbox::f64>(LogicalSize) * m_Scale;
	if (!Toolbox::IsFinite(Pixel) || Pixel < 1)
	{
		return 1;
	}
	return static_cast<Toolbox::int32>(Toolbox::Min<Toolbox::int64>(Toolbox::RoundToLong(Pixel), 1024));
}
} // namespace Dxf
