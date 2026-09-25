// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_STYLE_H
#define DXF_UI_STYLE_H
#include "Dxf/UiTypes.h"
#include "Toolbox/Optional.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 色が同じか。
 * @param A 一方。
 * @param B もう一方。
 */
FORCEINLINE constexpr bool IsSameUiColor(FColor A, FColor B) noexcept
{
	return A.R == B.R && A.G == B.G && A.B == B.B && A.A == B.A;
}
/**
 * 状態を反映した後の、要素一つの見た目の値。
 */
struct FUiStyle
{
	/**
	 * 背景色（A=0は描かない）。
	 */
	FColor Background{0, 0, 0, 0};
	/**
	 * 文字色。
	 */
	FColor Foreground{235, 235, 240, 255};
	/**
	 * 枠の色。
	 */
	FColor BorderColor{0, 0, 0, 0};
	/**
	 * 枠の幅（論理単位、0は描かない）。
	 */
	Toolbox::f32 BorderWidth = 0;
	/**
	 * 字体名。
	 */
	Toolbox::FString FontFamily = "Meiryo";
	/**
	 * 文字の大きさ（論理単位の行の高さの目安。画素の大きさは表示面の倍率を掛けて決める）。
	 */
	Toolbox::f32 FontSize = 20;
	/**
	 * 内側の余白（要素が明示していなければ使う）。
	 */
	FUiThickness Padding;
	/**
	 * 不透明度（0〜1）。
	 */
	Toolbox::f32 Opacity = 1;
	/**
	 * 値が同じか。
	 */
	bool operator==(const FUiStyle& Other) const noexcept
	{
		return IsSameUiColor(Background, Other.Background) && IsSameUiColor(Foreground, Other.Foreground) &&
		       IsSameUiColor(BorderColor, Other.BorderColor) && BorderWidth == Other.BorderWidth &&
		       FontFamily == Other.FontFamily && FontSize == Other.FontSize && Padding == Other.Padding &&
		       Opacity == Other.Opacity;
	}
};

/**
 * 見た目の一部の上書き。値のある項目だけを置き換える。
 */
struct FUiStylePatch
{
	/**
	 * 背景色。
	 */
	Toolbox::TOptional<FColor> Background;
	/**
	 * 文字色。
	 */
	Toolbox::TOptional<FColor> Foreground;
	/**
	 * 枠の色。
	 */
	Toolbox::TOptional<FColor> BorderColor;
	/**
	 * 枠の幅。
	 */
	Toolbox::TOptional<Toolbox::f32> BorderWidth;
	/**
	 * 字体名。
	 */
	Toolbox::TOptional<Toolbox::FString> FontFamily;
	/**
	 * 文字の大きさ。
	 */
	Toolbox::TOptional<Toolbox::f32> FontSize;
	/**
	 * 内側の余白。
	 */
	Toolbox::TOptional<FUiThickness> Padding;
	/**
	 * 不透明度。
	 */
	Toolbox::TOptional<Toolbox::f32> Opacity;
	/**
	 * 値のある項目を置き換える。
	 * @param Style 置き換える対象。
	 */
	void ApplyTo(FUiStyle& Style) const
	{
		if (Background)
		{
			Style.Background = *Background;
		}
		if (Foreground)
		{
			Style.Foreground = *Foreground;
		}
		if (BorderColor)
		{
			Style.BorderColor = *BorderColor;
		}
		if (BorderWidth)
		{
			Style.BorderWidth = *BorderWidth;
		}
		if (FontFamily)
		{
			Style.FontFamily = *FontFamily;
		}
		if (FontSize)
		{
			Style.FontSize = *FontSize;
		}
		if (Padding)
		{
			Style.Padding = *Padding;
		}
		if (Opacity)
		{
			Style.Opacity = *Opacity;
		}
	}
	/**
	 * 値のある項目がないか。
	 */
	bool IsEmpty() const noexcept
	{
		return !Background && !Foreground && !BorderColor && !BorderWidth && !FontFamily && !FontSize && !Padding &&
		       !Opacity;
	}
};

/**
 * 見た目を切り替える要素の状態（ビットの組合せ）。
 */
enum class EUiStyleState : Toolbox::uint8
{
	/**
	 * 通常。
	 */
	Normal = 0,
	/**
	 * ポインターが上にある。
	 */
	Hover = 1,
	/**
	 * 押されている。
	 */
	Pressed = 2,
	/**
	 * フォーカスがある。
	 */
	Focus = 4,
	/**
	 * 無効。
	 */
	Disabled = 8
};

/**
 * 状態の組合せ。
 * @param A 一方。
 * @param B もう一方。
 */
FORCEINLINE constexpr EUiStyleState operator|(EUiStyleState A, EUiStyleState B) noexcept
{
	return static_cast<EUiStyleState>(static_cast<Toolbox::uint8>(A) | static_cast<Toolbox::uint8>(B));
}
/**
 * 状態が含まれるか。
 * @param States 状態の組合せ。
 * @param State 調べる状態。
 */
FORCEINLINE constexpr bool HasUiState(EUiStyleState States, EUiStyleState State) noexcept
{
	return (static_cast<Toolbox::uint8>(States) & static_cast<Toolbox::uint8>(State)) != 0;
}

/**
 * 一つのスタイルIDの定義。基本値と、状態ごとの上書き。
 * 反映の順は 基本→Hover→Pressed→Focus→Disabled（後の状態が優先）。
 */
struct FUiStyleSet
{
	/**
	 * 基本値。
	 */
	FUiStyle Base;
	/**
	 * ポインターが上にあるとき。
	 */
	FUiStylePatch Hover;
	/**
	 * 押されているとき。
	 */
	FUiStylePatch Pressed;
	/**
	 * フォーカスがあるとき。
	 */
	FUiStylePatch Focus;
	/**
	 * 無効のとき。
	 */
	FUiStylePatch Disabled;
	/**
	 * 状態を反映した見た目を求める。
	 * @param States 状態の組合せ。
	 */
	FUiStyle Resolve(EUiStyleState States) const
	{
		FUiStyle Result = Base;
		if (HasUiState(States, EUiStyleState::Hover))
		{
			Hover.ApplyTo(Result);
		}
		if (HasUiState(States, EUiStyleState::Pressed))
		{
			Pressed.ApplyTo(Result);
		}
		if (HasUiState(States, EUiStyleState::Focus))
		{
			Focus.ApplyTo(Result);
		}
		if (HasUiState(States, EUiStyleState::Disabled))
		{
			Disabled.ApplyTo(Result);
		}
		return Result;
	}
};
} // namespace Dxf
#endif
