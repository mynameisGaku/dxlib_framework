// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_TYPES_H
#define DXF_UI_TYPES_H
#include "Dxf/MathTypes.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * UIの論理座標の矩形（左上と大きさ）。単位は論理単位で、画素への変換は表示面（FUiSurface）が行う。
 */
struct FUiRect
{
	/**
	 * 左端。
	 */
	Toolbox::f32 X = 0;
	/**
	 * 上端。
	 */
	Toolbox::f32 Y = 0;
	/**
	 * 幅（0以上）。
	 */
	Toolbox::f32 Width = 0;
	/**
	 * 高さ（0以上）。
	 */
	Toolbox::f32 Height = 0;
	/**
	 * 右端（半開区間の終端）。
	 */
	FORCEINLINE Toolbox::f32 Right() const noexcept
	{
		return X + Width;
	}
	/**
	 * 下端（半開区間の終端）。
	 */
	FORCEINLINE Toolbox::f32 Bottom() const noexcept
	{
		return Y + Height;
	}
	/**
	 * 面積がないか（幅または高さが0以下）。
	 */
	FORCEINLINE bool IsEmpty() const noexcept
	{
		return !(Width > 0) || !(Height > 0);
	}
	/**
	 * 点が矩形の内側か。左・上は含み、右・下は含まない（半開区間）。
	 * @param Point 論理座標の点。
	 */
	FORCEINLINE bool Contains(FVector2 Point) const noexcept
	{
		return Point.X >= X && Point.Y >= Y && Point.X < Right() && Point.Y < Bottom();
	}
	/**
	 * 二つの矩形の共通部分。重ならなければ大きさ0の矩形。
	 * @param Other もう一方の矩形。
	 */
	FUiRect Intersect(const FUiRect& Other) const noexcept
	{
		// 共通部分の左上と右下。
		const Toolbox::f32 Left = Toolbox::Max(X, Other.X);
		const Toolbox::f32 Top = Toolbox::Max(Y, Other.Y);
		const Toolbox::f32 RightEdge = Toolbox::Min(Right(), Other.Right());
		const Toolbox::f32 BottomEdge = Toolbox::Min(Bottom(), Other.Bottom());
		if (!(RightEdge > Left) || !(BottomEdge > Top))
		{
			return {Left, Top, 0, 0};
		}
		return {Left, Top, RightEdge - Left, BottomEdge - Top};
	}
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiRect&) const = default;
};

/**
 * UIの論理単位の大きさ。
 */
struct FUiSize
{
	/**
	 * 幅。
	 */
	Toolbox::f32 Width = 0;
	/**
	 * 高さ。
	 */
	Toolbox::f32 Height = 0;
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiSize&) const = default;
};

/**
 * 四辺の余白（外側余白・内側余白・枠の幅）。論理単位。
 */
struct FUiThickness
{
	/**
	 * 左。
	 */
	Toolbox::f32 Left = 0;
	/**
	 * 上。
	 */
	Toolbox::f32 Top = 0;
	/**
	 * 右。
	 */
	Toolbox::f32 Right = 0;
	/**
	 * 下。
	 */
	Toolbox::f32 Bottom = 0;
	/**
	 * 四辺に同じ値を持つ余白。
	 * @param Value 四辺の値。
	 */
	static constexpr FUiThickness All(Toolbox::f32 Value) noexcept
	{
		return {Value, Value, Value, Value};
	}
	/**
	 * 左右と上下を指定した余白。
	 * @param Horizontal 左右の値。
	 * @param Vertical 上下の値。
	 */
	static constexpr FUiThickness Symmetric(Toolbox::f32 Horizontal, Toolbox::f32 Vertical) noexcept
	{
		return {Horizontal, Vertical, Horizontal, Vertical};
	}
	/**
	 * 左右の合計。
	 */
	FORCEINLINE Toolbox::f32 Horizontal() const noexcept
	{
		return Left + Right;
	}
	/**
	 * 上下の合計。
	 */
	FORCEINLINE Toolbox::f32 Vertical() const noexcept
	{
		return Top + Bottom;
	}
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiThickness&) const = default;
};

/**
 * 一つの軸の大きさの決め方。
 */
enum class EUiSizeMode : Toolbox::uint8
{
	/**
	 * 内容（文字・子要素）の大きさ。
	 */
	Content,
	/**
	 * 固定の論理単位。
	 */
	Fixed,
	/**
	 * 親の余り領域を重みで分け合う。親がその軸を内容の大きさで決める場合は内容の大きさとして扱う。
	 */
	Fill
};

/**
 * 一つの軸の大きさの指定。
 */
struct FUiLength
{
	/**
	 * 決め方。
	 */
	EUiSizeMode Mode = EUiSizeMode::Content;
	/**
	 * Fixedでは論理単位の長さ、Fillでは重み（正の値）。Contentでは使わない。
	 */
	Toolbox::f32 Value = 0;
	/**
	 * 内容の大きさ。
	 */
	static constexpr FUiLength Content() noexcept
	{
		return {EUiSizeMode::Content, 0};
	}
	/**
	 * 固定の長さ。
	 * @param Value 論理単位の長さ。
	 */
	static constexpr FUiLength Fixed(Toolbox::f32 Value) noexcept
	{
		return {EUiSizeMode::Fixed, Value};
	}
	/**
	 * 余り領域の分配。
	 * @param Weight 正の重み。
	 */
	static constexpr FUiLength Fill(Toolbox::f32 Weight = 1) noexcept
	{
		return {EUiSizeMode::Fill, Weight};
	}
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiLength&) const = default;
};

/**
 * 親から割り当てた範囲の中での配置。
 */
enum class EUiAlign : Toolbox::uint8
{
	/**
	 * 先頭（左・上）。
	 */
	Start,
	/**
	 * 中央。
	 */
	Center,
	/**
	 * 末尾（右・下）。
	 */
	End,
	/**
	 * 範囲いっぱいに伸ばす。
	 */
	Stretch
};

/**
 * 表示とレイアウトへの参加の状態。
 */
enum class EUiVisibility : Toolbox::uint8
{
	/**
	 * 表示し、レイアウトに参加する。
	 */
	Visible,
	/**
	 * 描かず入力も受けないが、レイアウトの領域は確保する。
	 */
	Hidden,
	/**
	 * 描かず入力も受けず、レイアウトからも外す（大きさ0）。
	 */
	Collapsed
};

/**
 * ポインターの当たり判定の方針。
 */
enum class EUiHitTest : Toolbox::uint8
{
	/**
	 * 自身の矩形で受け、子も判定する（子が前面）。
	 */
	Self,
	/**
	 * 自身は受けず、子だけを判定する（透明な入れ物）。
	 */
	ChildrenOnly,
	/**
	 * 自身も子も受けず、背後へ通す（装飾）。
	 */
	None
};

/**
 * ルートの中の重なり領域。値の小さい順に描き、大きい方を前面として選ぶ。
 */
enum class EUiLayer : Toolbox::uint8
{
	/**
	 * 通常の画面。
	 */
	Normal,
	/**
	 * 通常の画面の上に出す追加パネル（スライドパネル等）。
	 */
	Panel,
	/**
	 * ポップアップ・Modal。
	 */
	Popup,
	/**
	 * ツールチップ（最前面に一つ）。
	 */
	Tooltip,
	/**
	 * 領域の数。
	 */
	Count
};

/**
 * 論理座標の点を作る。
 * @param X 横。
 * @param Y 縦。
 */
FORCEINLINE constexpr FVector2 MakeUiPoint(Toolbox::f32 X, Toolbox::f32 Y) noexcept
{
	return {X, Y};
}
} // namespace Dxf
#endif
