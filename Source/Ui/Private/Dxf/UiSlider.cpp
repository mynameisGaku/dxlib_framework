// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiSlider.h"
#include "Dxf/UiDrawContext.h"
namespace Dxf
{
DUiSlider::DUiSlider()
{
	SetStyleId("Slider");
	SetFocusable(true);
	SetHitTest(EUiHitTest::Self);
	SetWidth(FUiLength::Fixed(200));
	SetHeight(FUiLength::Fixed(28));
}

void DUiSlider::SetRange(Toolbox::f64 Minimum, Toolbox::f64 Maximum, Toolbox::f64 Step)
{
	m_Range.Configure(Minimum, Maximum, Step);
}

void DUiSlider::SetValue(Toolbox::f64 Value)
{
	m_Range.Set(Value);
}

FUiRect DUiSlider::GetThumbRect() const noexcept
{
	const FUiRect Rect = GetContentRect();
	const Toolbox::f32 Width = Toolbox::Min(12.0f, Rect.Width);
	return {Rect.X + static_cast<Toolbox::f32>(m_Range.Fraction()) * (Rect.Width - Width), Rect.Y, Width, Rect.Height};
}

void DUiSlider::SetFromUser_Internal(Toolbox::f64 Value)
{
	const Toolbox::f64 Before = m_Range.Value;
	m_Range.Set(Value);
	if (m_Range.Value != Before)
	{
		m_Changed.Emit(m_Range.Value);
	}
}

void DUiSlider::SetFromPointer_Internal(Toolbox::f32 X)
{
	const FUiRect Rect = GetContentRect();
	const Toolbox::f32 Thumb = Toolbox::Min(12.0f, Rect.Width);
	const Toolbox::f64 Span = static_cast<Toolbox::f64>(Rect.Width) - Thumb;
	if (Span > 0)
	{
		const Toolbox::f64 Fraction =
		    Toolbox::Clamp((static_cast<Toolbox::f64>(X) - Rect.X - Thumb * 0.5) / Span, 0.0, 1.0);
		SetFromUser_Internal(m_Range.Minimum + Fraction * (m_Range.Maximum - m_Range.Minimum));
	}
}

void DUiSlider::OnPointerEvent(FUiPointerEvent& Event)
{
	if (Event.Type == EUiPointerEventType::Down && Event.Button == EUiPointerButton::Primary)
	{
		Event.bHandled = true;
		if (CapturePointer())
		{
			SetPressed_Internal(true);
			SetFromPointer_Internal(Event.Position.X);
		}
	}
	else if (Event.Type == EUiPointerEventType::Move && HasPointerCapture())
	{
		Event.bHandled = true;
		SetFromPointer_Internal(Event.Position.X);
	}
	else if (Event.Type == EUiPointerEventType::Up && Event.Button == EUiPointerButton::Primary && HasPointerCapture())
	{
		Event.bHandled = true;
		ReleasePointer();
		SetPressed_Internal(false);
		SetFromPointer_Internal(Event.Position.X);
	}
	else if (Event.Type == EUiPointerEventType::CaptureLost)
	{
		SetPressed_Internal(false);
	}
}

void DUiSlider::OnNavigationEvent(FUiNavigationEvent& Event)
{
	if (Event.Command == EUiNavigationCommand::Left || Event.Command == EUiNavigationCommand::Right)
	{
		Event.bHandled = true;
		const Toolbox::f64 Step = m_Range.Step > 0 ? m_Range.Step : (m_Range.Maximum - m_Range.Minimum) / 100;
		SetFromUser_Internal(m_Range.Value + (Event.Command == EUiNavigationCommand::Left ? -Step : Step));
	}
}

void DUiSlider::OnDraw(FUiDrawContext& Context) const
{
	DUiElement::OnDraw(Context);
	const FUiRect Area = GetContentRect();
	Context.FillRect({Area.X, Area.Y + Area.Height * 0.5f - 2, Area.Width, 4}, GetStyle().BorderColor);
	Context.FillRect(GetThumbRect(), GetStyle().Foreground);
}
} // namespace Dxf
// namespace Dxf
