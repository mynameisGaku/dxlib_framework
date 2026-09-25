// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiButton.h"
#include "Dxf/UiRoot.h"
namespace Dxf
{
// 文字を持つボタン。
DUiButton::DUiButton(Toolbox::FString Text) : m_Text(Toolbox::Move(Text))
{
	SetStyleId("Button");
}
// 表示する文字を変える。
void DUiButton::SetText(Toolbox::FString Text)
{
	m_Text = Toolbox::Move(Text);
	if (DUiLabel* Label = m_Label.Get())
	{
		Label->SetText(m_Text);
	}
}
// 文字の要素を一度だけ作る。
void DUiButton::OnFirstAttach()
{
	m_Label = CreateChild<DUiLabel>(m_Text);
	DUiLabel* Label = m_Label.Get();
	Label->SetName(GetName().IsEmpty() ? Toolbox::FString("Button.Label") : GetName() + ".Label");
	Label->SetAlign(EUiAlign::Center, EUiAlign::Center);
	Label->SetHitTest(EUiHitTest::None);
	Label->SetTextLayout(EUiTextWrap::NoWrap, EUiTextOverflow::Ellipsis);
	OnStyleResolved();
}
// 状態の見た目を文字の要素へ伝える。
void DUiButton::OnStyleResolved()
{
	DUiLabel* Label = m_Label.Get();
	if (Label != nullptr && !(IsSameUiColor(m_AppliedForeground, GetStyle().Foreground) && m_AppliedFontSize == GetStyle().FontSize &&
	                          m_AppliedFontFamily == GetStyle().FontFamily))
	{
		// 変わったときだけ伝える（見た目の変化のたびに文字を測り直さない）。
		m_AppliedForeground = GetStyle().Foreground;
		m_AppliedFontSize = GetStyle().FontSize;
		m_AppliedFontFamily = GetStyle().FontFamily;
		FUiStylePatch Patch;
		Patch.Foreground = GetStyle().Foreground;
		Patch.FontSize = GetStyle().FontSize;
		Patch.FontFamily = GetStyle().FontFamily;
		Label->SetStyleOverride(Patch);
	}
}
} // namespace Dxf
