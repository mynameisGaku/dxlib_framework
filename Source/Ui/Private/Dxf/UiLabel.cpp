// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiLabel.h"
#include "Dxf/UiDrawContext.h"
#include "Dxf/UiLayoutContext.h"
namespace Dxf
{
// 文字で作る。
DUiLabel::DUiLabel(Toolbox::FString Text) : m_Text(Toolbox::Move(Text))
{
}
// 表示する文字を変える。
void DUiLabel::SetText(Toolbox::FString Text)
{
	if (Text == m_Text)
	{
		return;
	}
	m_Text = Toolbox::Move(Text);
	m_bTextDirty = true;
	InvalidateMeasure();
}
// 折返しと省略を設定する。
void DUiLabel::SetTextLayout(EUiTextWrap Wrap, EUiTextOverflow Overflow, Toolbox::uint32 MaxLines)
{
	if (Wrap == m_Wrap && Overflow == m_Overflow && MaxLines == m_MaxLines)
	{
		return;
	}
	m_Wrap = Wrap;
	m_Overflow = Overflow;
	m_MaxLines = MaxLines;
	m_bTextDirty = true;
	InvalidateMeasure();
}
// 横の揃えを設定する。
void DUiLabel::SetTextAlign(EUiAlign Align)
{
	m_TextAlign = Align;
	InvalidateArrange();
}
// 見た目の解決後。
void DUiLabel::OnStyleResolved()
{
	if (GetStyle().FontFamily != m_FontFamily || GetStyle().FontSize != m_FontSize)
	{
		m_bTextDirty = true;
		InvalidateMeasure();
	}
}
// 指定の最大幅で文字を配置する。
void DUiLabel::LayoutText_Internal(FUiLayoutContext& Context, Toolbox::int32 MaxWidth)
{
	const Toolbox::f32 Scale = Context.GetSurface().GetScale();
	const bool bPremultiplied = Context.GetSurface().IsPremultipliedAlpha();
	if (!m_bTextDirty && MaxWidth == m_LayoutMaxWidth && Scale == m_LayoutScale &&
	    bPremultiplied == m_bLayoutPremultiplied)
	{
		return;
	}
	m_Layout = {};
	m_LayoutMaxWidth = MaxWidth;
	m_LayoutScale = Scale;
	m_bLayoutPremultiplied = bPremultiplied;
	m_FontFamily = GetStyle().FontFamily;
	m_FontSize = GetStyle().FontSize;
	if (m_Text.IsEmpty() || !Context.HasTextService())
	{
		m_Font = {};
		m_bTextDirty = false;
		return;
	}
	auto Font = Context.ResolveFont(m_FontFamily, m_FontSize);
	if (!Font)
	{
		throw Toolbox::FException(Font.Error().Message.CStr());
	}
	m_Font = Font.Value();
	FUiTextLayoutRequest Request;
	Request.MaxWidth = MaxWidth;
	Request.Wrap = m_Wrap;
	Request.Overflow = m_Overflow;
	Request.MaxLines = m_MaxLines;
	auto Layout = Context.LayoutText(m_Font, m_Text, Request);
	if (!Layout)
	{
		throw Toolbox::FException(Layout.Error().Message.CStr());
	}
	m_Layout = Toolbox::Move(Layout).Value();
	m_bTextDirty = false;
}
// 文字の大きさを測る。
FUiSize DUiLabel::OnMeasure(FUiLayoutContext& Context, FUiSize Available)
{
	// 折返し・省略がなければ幅の上限は使わない（同じ配置を幅に依らず再利用する）。
	const bool bUsesWidth = m_Wrap == EUiTextWrap::Wrap || m_Overflow == EUiTextOverflow::Ellipsis;
	LayoutText_Internal(Context, bUsesWidth ? Context.ToPixels(Available.Width) : -1);
	return {Context.ToLogical(m_Layout.Width), Context.ToLogical(m_Layout.Height)};
}
// 狭い幅なら配置し直す。
void DUiLabel::OnArrange(FUiLayoutContext& Context, const FUiRect& Content)
{
	const bool bUsesWidth = m_Wrap == EUiTextWrap::Wrap || m_Overflow == EUiTextOverflow::Ellipsis;
	if (bUsesWidth)
	{
		const Toolbox::int32 Width = Context.ToPixels(Content.Width);
		if (m_LayoutMaxWidth < 0 || Width < m_LayoutMaxWidth)
		{
			LayoutText_Internal(Context, Width);
		}
	}
	DUiElement::OnArrange(Context, Content);
}
// 背景・枠と、各行の文字を描く。
void DUiLabel::OnDraw(FUiDrawContext& Context) const
{
	DUiElement::OnDraw(Context);
	// フォントの有効性は描画の実行側で確かめる（無効なフォントの文字は描かない）。
	if (m_Layout.Lines.IsEmpty())
	{
		return;
	}
	const FUiRect Content = GetContentRect();
	const Toolbox::f32 Scale = Context.GetSurface().GetScale();
	const Toolbox::f32 LineHeight = static_cast<Toolbox::f32>(m_Layout.LineHeight) / Scale;
	for (Toolbox::size_t Index = 0; Index < m_Layout.Lines.Size(); ++Index)
	{
		const FUiTextLine& Line = m_Layout.Lines[Index];
		const Toolbox::f32 Width = static_cast<Toolbox::f32>(Line.Width) / Scale;
		Toolbox::f32 X = Content.X;
		if (m_TextAlign == EUiAlign::Center)
		{
			X = Content.X + (Content.Width - Width) * 0.5f;
		}
		else if (m_TextAlign == EUiAlign::End)
		{
			X = Content.Right() - Width;
		}
		Context.DrawText(m_Font, Line.Text, {X, Content.Y + LineHeight * static_cast<Toolbox::f32>(Index)},
		                 {Width, LineHeight}, GetStyle().Foreground);
	}
}
} // namespace Dxf
