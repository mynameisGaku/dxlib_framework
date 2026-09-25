// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_LABEL_H
#define DXF_UI_LABEL_H
#include "Dxf/UiElement.h"
#include "Dxf/UiTextLayout.h"
namespace Dxf
{
/**
 * 文字の表示。寸法の計算と描画は同じフォント・同じ折返し・省略の規則を使う（実フォントの計測）。
 */
class DUiLabel : public DUiElement
{
public:
	/**
	 * @param Text 表示する文字（UTF-8）。
	 */
	explicit DUiLabel(Toolbox::FString Text = {});
	/**
	 * 表示する文字。
	 */
	FORCEINLINE const Toolbox::FString& GetText() const noexcept
	{
		return m_Text;
	}
	/**
	 * 表示する文字を変える（同じなら何もしない）。
	 * @param Text 文字。
	 */
	void SetText(Toolbox::FString Text);
	/**
	 * 折返しと省略を設定する。
	 * @param Wrap 折返し。
	 * @param Overflow 収まらない文字の扱い。
	 * @param MaxLines 行数の上限（0は上限なし）。
	 */
	void SetTextLayout(EUiTextWrap Wrap, EUiTextOverflow Overflow, Toolbox::uint32 MaxLines = 0);
	/**
	 * 横の文字の揃え。
	 * @param Align 揃え（Stretchは先頭）。
	 */
	void SetTextAlign(EUiAlign Align);
	/**
	 * 最後の配置の結果（画素単位）。
	 */
	FORCEINLINE const FUiTextLayoutResult& GetTextLayout() const noexcept
	{
		return m_Layout;
	}
	/**
	 * 最後の配置で使った最大幅（画素、負は上限なし）。
	 */
	FORCEINLINE Toolbox::int32 GetLayoutMaxWidth() const noexcept
	{
		return m_LayoutMaxWidth;
	}
	/**
	 * 描画に使うフォント（最後の配置で解決したもの）。
	 */
	FORCEINLINE const FFont& GetFont() const noexcept
	{
		return m_Font;
	}
	/**
	 * 最後の配置で使った画素の倍率。
	 */
	FORCEINLINE Toolbox::f32 GetLayoutScale() const noexcept
	{
		return m_LayoutScale;
	}

protected:
	/**
	 * 文字の大きさを測る（折返しは使える幅で行う）。
	 */
	FUiSize OnMeasure(FUiLayoutContext& Context, FUiSize Available) override;
	/**
	 * 割り当てた幅が測ったときより狭ければ、その幅で配置し直す。
	 */
	void OnArrange(FUiLayoutContext& Context, const FUiRect& Content) override;
	/**
	 * 背景・枠と、各行の文字を描く。
	 */
	void OnDraw(FUiDrawContext& Context) const override;
	/**
	 * 文字の大きさ・字体が変わったら測り直す。
	 */
	void OnStyleResolved() override;

private:
	/**
	 * 指定の最大幅で文字を配置する。
	 * @param Context 窓口。
	 * @param MaxWidth 最大幅（画素、負は上限なし）。
	 */
	void LayoutText_Internal(FUiLayoutContext& Context, Toolbox::int32 MaxWidth);
	/**
	 * 表示する文字。
	 */
	Toolbox::FString m_Text;
	/**
	 * 折返し。
	 */
	EUiTextWrap m_Wrap = EUiTextWrap::NoWrap;
	/**
	 * 収まらない文字の扱い。
	 */
	EUiTextOverflow m_Overflow = EUiTextOverflow::Visible;
	/**
	 * 行数の上限。
	 */
	Toolbox::uint32 m_MaxLines = 0;
	/**
	 * 横の揃え。
	 */
	EUiAlign m_TextAlign = EUiAlign::Start;
	/**
	 * 配置の結果。
	 */
	FUiTextLayoutResult m_Layout;
	/**
	 * 配置の最大幅。
	 */
	Toolbox::int32 m_LayoutMaxWidth = -2;
	/**
	 * 配置の倍率。
	 */
	Toolbox::f32 m_LayoutScale = 0;
	/**
	 * 配置に使ったフォント。
	 */
	FFont m_Font;
	/**
	 * 配置に使った字体名。
	 */
	Toolbox::FString m_FontFamily;
	/**
	 * 配置に使った文字の大きさ。
	 */
	Toolbox::f32 m_FontSize = 0;
	/**
	 * 文字・設定が変わって配置し直すか。
	 */
	bool m_bTextDirty = true;
};
} // namespace Dxf
#endif
