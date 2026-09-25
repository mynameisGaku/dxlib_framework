// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_LAYOUT_PARAMS_H
#define DXF_UI_LAYOUT_PARAMS_H
#include "Dxf/UiTypes.h"
namespace Dxf
{
/**
 * 上限なしを表す長さ（f32の最大の有限値）。
 */
inline constexpr Toolbox::f32 UiUnbounded = FLT_MAX;
/**
 * 要素一つのレイアウトの指定。値はすべて論理単位。
 */
struct FUiLayoutParams
{
	/**
	 * 幅の決め方。
	 */
	FUiLength Width = FUiLength::Content();
	/**
	 * 高さの決め方。
	 */
	FUiLength Height = FUiLength::Content();
	/**
	 * 最小の大きさ。
	 */
	FUiSize MinSize{0, 0};
	/**
	 * 最大の大きさ（UiUnbounded以上は上限なし）。
	 */
	FUiSize MaxSize{UiUnbounded, UiUnbounded};
	/**
	 * 外側の余白。
	 */
	FUiThickness Margin;
	/**
	 * 内側の余白（子と内容の配置範囲を狭める）。
	 */
	FUiThickness Padding;
	/**
	 * 親から割り当てた範囲の中での横の配置。
	 */
	EUiAlign HorizontalAlign = EUiAlign::Stretch;
	/**
	 * 親から割り当てた範囲の中での縦の配置。
	 */
	EUiAlign VerticalAlign = EUiAlign::Stretch;
	/**
	 * 親の内容範囲の左上からの位置で置くか（重ね合わせの親だけが使う。ツールチップ・吹き出し等）。
	 */
	bool bAbsolute = false;
	/**
	 * bAbsoluteのときの、親の内容範囲の左上からの位置。
	 */
	FVector2 Position;
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiLayoutParams& Other) const noexcept
	{
		return Width == Other.Width && Height == Other.Height && MinSize == Other.MinSize && MaxSize == Other.MaxSize &&
		       Margin == Other.Margin && Padding == Other.Padding && HorizontalAlign == Other.HorizontalAlign &&
		       VerticalAlign == Other.VerticalAlign && bAbsolute == Other.bAbsolute && Position.X == Other.Position.X &&
		       Position.Y == Other.Position.Y;
	}
};

/**
 * 入れ物の子の並べ方。
 */
enum class EUiStackMode : Toolbox::uint8
{
	/**
	 * 重ね合わせ（全員が同じ内容範囲）。
	 */
	Overlay,
	/**
	 * 縦に並べる。
	 */
	Vertical,
	/**
	 * 横に並べる。
	 */
	Horizontal
};
} // namespace Dxf
#endif
