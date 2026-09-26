// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiToggle.h"
#include "Dxf/UiDrawContext.h"
namespace Dxf
{
// チェックの表示は文字と別に描く。
DUiToggle::DUiToggle(Toolbox::FString Text, bool bValue) : DUiButton(Toolbox::Move(Text)), m_bValue(bValue)
{
	SetStyleId("Toggle");
	SetPadding({28, 4, 4, 4});
}

void DUiToggle::SetValue(bool bValue) noexcept
{
	m_bValue = bValue;
}

void DUiToggle::OnActivated()
{
	m_bValue = !m_bValue;
	// 通知後にthisへ戻らない。購読者は要素やルートを破棄できる。
	m_Changed.Emit(m_bValue);
}

void DUiToggle::OnDrawOverlay(FUiDrawContext& Context) const
{
	DUiElement::OnDrawOverlay(Context);
	const FUiRect Box{GetRect().X + 4, GetRect().Y + (GetRect().Height - 16) * 0.5f, 16, 16};
	Context.DrawBorder(Box, GetStyle().Foreground, 1);
	if (m_bValue)
	{
		Context.FillRect({Box.X + 4, Box.Y + 4, 8, 8}, GetStyle().Foreground);
	}
}
} // namespace Dxf
// namespace Dxf
