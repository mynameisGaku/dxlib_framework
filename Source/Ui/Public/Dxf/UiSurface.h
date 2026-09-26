// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SURFACE_H
#define DXF_UI_SURFACE_H
#include "Dxf/UiTypes.h"
namespace Dxf
{
/**
 * 論理座標から画素への倍率の決め方。
 */
enum class EUiScaleMode : Toolbox::uint8
{
	/**
	 * 表示領域の高さを基準の高さで割った倍率（高さ基準）。横幅は論理座標の幅が変わる。
	 */
	ReferenceHeight,
	/**
	 * 1論理単位を1画素（に利用者倍率を掛けた値）で表す（固定ピクセル）。
	 */
	FixedPixel
};

/**
 * 倍率の設定。
 */
struct FUiScaleSettings
{
	/**
	 * 決め方。
	 */
	EUiScaleMode Mode = EUiScaleMode::ReferenceHeight;
	/**
	 * 高さ基準の基準の高さ（論理単位）。例: 720、1080、2160。
	 */
	Toolbox::f32 ReferenceHeight = 720;
	/**
	 * 利用者の倍率（設定画面のUIスケール等）。
	 */
	Toolbox::f32 UserScale = 1;
	/**
	 * 倍率の下限。
	 */
	Toolbox::f32 MinScale = 0.25f;
	/**
	 * 倍率の上限。
	 */
	Toolbox::f32 MaxScale = 8;
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiScaleSettings&) const = default;
};

/**
 * 画素の矩形（左・上を含み、右・下を含まない）。
 */
struct FUiPixelRect
{
	/**
	 * 左。
	 */
	Toolbox::int32 Left = 0;
	/**
	 * 上。
	 */
	Toolbox::int32 Top = 0;
	/**
	 * 右（含まない）。
	 */
	Toolbox::int32 Right = 0;
	/**
	 * 下（含まない）。
	 */
	Toolbox::int32 Bottom = 0;
	/**
	 * 幅。
	 */
	FORCEINLINE Toolbox::int32 Width() const noexcept
	{
		return static_cast<Toolbox::int32>(Toolbox::Clamp<Toolbox::int64>(
		    static_cast<Toolbox::int64>(Right) - Left, 0, Toolbox::TNumericLimits<Toolbox::int32>::Max()));
	}
	/**
	 * 高さ。
	 */
	FORCEINLINE Toolbox::int32 Height() const noexcept
	{
		return static_cast<Toolbox::int32>(Toolbox::Clamp<Toolbox::int64>(
		    static_cast<Toolbox::int64>(Bottom) - Top, 0, Toolbox::TNumericLimits<Toolbox::int32>::Max()));
	}
	/**
	 * 面積がないか。
	 */
	FORCEINLINE bool IsEmpty() const noexcept
	{
		return Right <= Left || Bottom <= Top;
	}
	/**
	 * 共通部分。重ならなければ空。
	 * @param Other もう一方。
	 */
	FUiPixelRect Intersect(const FUiPixelRect& Other) const noexcept
	{
		FUiPixelRect Result{Toolbox::Max(Left, Other.Left), Toolbox::Max(Top, Other.Top),
		                    Toolbox::Min(Right, Other.Right), Toolbox::Min(Bottom, Other.Bottom)};
		if (Result.IsEmpty())
		{
			Result.Right = Result.Left;
			Result.Bottom = Result.Top;
		}
		return Result;
	}
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiPixelRect&) const = default;
};

/**
 * ルートを表示する面。表示領域の画素矩形と倍率から、論理座標と画素の変換を決める。
 * 描画・文字の画素の大きさ・入力・クリップはすべてこの変換を共有する。
 */
class FUiSurface
{
public:
	FUiSurface() = default;
	/**
	 * 表示領域と倍率の設定から作る。表示領域が空・倍率が不正なら表示できない面になる。
	 * @param PixelRect 表示領域（全画面・Viewport・描画先テクスチャ）の画素矩形。
	 * @param Settings 倍率の設定。
	 */
	FUiSurface(FUiPixelRect PixelRect, const FUiScaleSettings& Settings);
	/**
	 * 表示できるか（領域が空でなく倍率が有限の正の値）。
	 */
	FORCEINLINE bool IsDisplayable() const noexcept
	{
		return m_bDisplayable;
	}
	/**
	 * 論理単位あたりの画素数。
	 */
	FORCEINLINE Toolbox::f32 GetScale() const noexcept
	{
		return m_Scale;
	}
	/**
	 * 論理座標の大きさ。
	 */
	FORCEINLINE FUiSize GetLogicalSize() const noexcept
	{
		return m_LogicalSize;
	}
	/**
	 * 表示領域の画素矩形。
	 */
	FORCEINLINE const FUiPixelRect& GetPixelRect() const noexcept
	{
		return m_PixelRect;
	}
	/**
	 * 論理座標の点を画素座標へ変える。
	 * @param Point 論理座標。
	 */
	FVector2 ToPixel(FVector2 Point) const noexcept;
	/**
	 * 論理座標の矩形を画素矩形へ変える。辺ごとに切り捨てるため、隣接する矩形は隙間なく接する（半開区間）。
	 * @param Rect 論理座標の矩形。
	 */
	FUiPixelRect ToPixel(const FUiRect& Rect) const noexcept;
	/**
	 * 画素座標の点を論理座標へ変える。
	 * @param Point 画素座標。
	 */
	FVector2 ToLogical(FVector2 Point) const noexcept;
	/**
	 * 文字の大きさ（論理単位）に対応する画素の大きさ（1以上に丸める）。
	 * @param LogicalSize 論理単位の大きさ。
	 */
	Toolbox::int32 ToFontPixelSize(Toolbox::f32 LogicalSize) const noexcept;
	/**
	 * 乗算済みアルファで描く表示面か（透明な中間画像）。文字は乗算済みのフォント、画像は乗算済みの画像で描く。
	 */
	FORCEINLINE bool IsPremultipliedAlpha() const noexcept
	{
		return m_bPremultipliedAlpha;
	}
	/**
	 * 乗算済みアルファで描くかを設定する。
	 * @param bPremultiplied 乗算済みで描くか。
	 */
	FORCEINLINE void SetPremultipliedAlpha(bool bPremultiplied) noexcept
	{
		m_bPremultipliedAlpha = bPremultiplied;
	}
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiSurface&) const = default;

private:
	/**
	 * 表示領域。
	 */
	FUiPixelRect m_PixelRect;
	/**
	 * 論理座標の大きさ。
	 */
	FUiSize m_LogicalSize;
	/**
	 * 倍率。
	 */
	Toolbox::f32 m_Scale = 1;
	/**
	 * 表示できるか。
	 */
	bool m_bDisplayable = false;
	/**
	 * 乗算済みアルファで描くか。
	 */
	bool m_bPremultipliedAlpha = false;
};
} // namespace Dxf
#endif
