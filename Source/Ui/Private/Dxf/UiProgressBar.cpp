// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiProgressBar.h"
#include "Dxf/UiDrawContext.h"
namespace Dxf
{
DUiProgressBar::DUiProgressBar()
{
	SetStyleId("ProgressBar");
	SetHitTest(EUiHitTest::None);
	SetWidth(FUiLength::Fixed(200));
	SetHeight(FUiLength::Fixed(20));
}

void DUiProgressBar::SetRange(Toolbox::f64 Minimum, Toolbox::f64 Maximum)
{
	m_Range.Configure(Minimum, Maximum, 0);
}

void DUiProgressBar::SetValue(Toolbox::f64 Value)
{
	m_Range.Set(Value);
}

void DUiProgressBar::OnDraw(FUiDrawContext& Context) const
{
	DUiElement::OnDraw(Context);
	FUiRect Fill = GetContentRect();
	Fill.Width *= static_cast<Toolbox::f32>(m_Range.Fraction());
	Context.FillRect(Fill, GetStyle().Foreground);
}
} // namespace Dxf
// namespace Dxf
