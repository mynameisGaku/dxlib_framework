// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiScrollView.h"
#include "Dxf/UiRoot.h"
#include "Dxf/UiDrawContext.h"
namespace Dxf
{
DUiScrollView::DUiScrollView()
{
	SetStyleId("ScrollView");
	SetClipChildren(true);
	SetFocusable(true);
	SetHitTest(EUiHitTest::Self);
	SetWidth(FUiLength::Fixed(240));
	SetHeight(FUiLength::Fixed(200));
	SetPadding({0, 0, 0, 0});
}

void DUiScrollView::SetContent(const TUiRef<DUiElement>& Content)
{
	if (Content.GetId() == m_Content.GetId())
	{
		return;
	}
	if (Content.Get() == nullptr || GetRoot() == nullptr)
	{
		throw Toolbox::FException("Scroll content must belong to a UI root");
	}
	// 新しい内容の接続が失敗したら古い内容を保持する。
	auto Added = GetRoot()->AddChild(GetRef(), Content);
	if (!Added)
	{
		throw Toolbox::FException(Added.Error().Message);
	}
	if (m_Content.Get() != nullptr)
	{
		(void)GetRoot()->Remove(m_Content);
	}
	m_Content = Content;
	InvalidateMeasure();
}

void DUiScrollView::SetWheelStep(Toolbox::f64 Amount, bool bRelative)
{
	if (!Toolbox::IsFinite(Amount) || !(Amount > 0))
	{
		throw Toolbox::FException("Invalid scroll wheel distance");
	}
	m_WheelStep = Amount;
	m_bRelativeWheel = bRelative;
}

void DUiScrollView::SetScrollOffset(Toolbox::f64 Offset)
{
	if (!Toolbox::IsFinite(Offset))
	{
		throw Toolbox::FException("Invalid scroll offset");
	}
	const Toolbox::f64 Next =
	    m_ViewHeight > 0 ? Toolbox::Clamp(Offset, 0.0, GetScrollMaximum()) : Toolbox::Max(0.0, Offset);
	if (Next != m_Offset)
	{
		m_Offset = Next;
		InvalidateArrange();
	}
}

FUiSize DUiScrollView::MeasureScrollable(FUiLayoutContext& Context, FUiSize Available)
{
	return m_Content.Get() != nullptr ? Context.MeasureChild(*m_Content.Get(), {Available.Width, UiUnbounded})
	                                  : FUiSize{};
}

FUiSize DUiScrollView::OnMeasure(FUiLayoutContext& Context, FUiSize Available)
{
	const FUiSize Wanted =
	    MeasureScrollable(Context, {Toolbox::Max(0.0f, Available.Width - m_BarWidth), Available.Height});
	m_ContentHeight = Wanted.Height;
	return {Toolbox::Min(Available.Width, Wanted.Width + m_BarWidth), Toolbox::Min(Available.Height, Wanted.Height)};
}

void DUiScrollView::ArrangeScrollable(FUiLayoutContext& Context, const FUiRect& View)
{
	if (DUiElement* Content = m_Content.Get())
	{
		Context.ArrangeChild(*Content, {View.X, View.Y - static_cast<Toolbox::f32>(m_Offset), View.Width,
		                                static_cast<Toolbox::f32>(Toolbox::Max(m_ContentHeight, m_ViewHeight))});
	}
}

void DUiScrollView::OnArrange(FUiLayoutContext& Context, const FUiRect& Content)
{
	m_ViewHeight = Content.Height;
	m_Offset = Toolbox::Clamp(m_Offset, 0.0, GetScrollMaximum());
	ArrangeScrollable(Context, {Content.X, Content.Y, Toolbox::Max(0.0f, Content.Width - m_BarWidth), Content.Height});
}

FUiRect DUiScrollView::GetScrollThumbRect() const noexcept
{
	const FUiRect View = GetContentRect();
	if (!(GetScrollMaximum() > 0) || !(View.Height > 0))
	{
		return {};
	}
	const Toolbox::f32 Height = Toolbox::Min(
	    View.Height, Toolbox::Max(16.0f, static_cast<Toolbox::f32>(View.Height * View.Height / m_ContentHeight)));
	return {View.Right() - m_BarWidth,
	        View.Y + static_cast<Toolbox::f32>(m_Offset / GetScrollMaximum()) * (View.Height - Height), m_BarWidth,
	        Height};
}

void DUiScrollView::OnDrawOverlay(FUiDrawContext& Context) const
{
	DUiElement::OnDrawOverlay(Context);
	const FUiRect Thumb = GetScrollThumbRect();
	if (!Thumb.IsEmpty())
	{
		const FUiRect View = GetContentRect();
		Context.FillRect({View.Right() - m_BarWidth, View.Y, m_BarWidth, View.Height}, GetStyle().Background);
		Context.FillRect(Thumb, GetStyle().Foreground);
	}
}

void DUiScrollView::DragTo_Internal(Toolbox::f32 Y)
{
	const FUiRect Thumb = GetScrollThumbRect();
	const FUiRect View = GetContentRect();
	const Toolbox::f64 Span = static_cast<Toolbox::f64>(View.Height) - Thumb.Height;
	if (Span > 0)
	{
		SetScrollOffset((static_cast<Toolbox::f64>(Y) - m_GrabOffset - View.Y) / Span * GetScrollMaximum());
	}
}

void DUiScrollView::OnPointerEvent(FUiPointerEvent& Event)
{
	if (Event.Type == EUiPointerEventType::Wheel)
	{
		const Toolbox::f64 Step = m_WheelStep * (m_bRelativeWheel ? m_ViewHeight : 1);
		if (Step > 0)
		{
			const Toolbox::f64 Before = m_Offset;
			SetScrollOffset(m_Offset + Event.WheelDelta * Step);
			Event.WheelDelta -= static_cast<Toolbox::f32>((m_Offset - Before) / Step);
			Event.bHandled = Toolbox::Abs(Event.WheelDelta) < 1.0e-6f;
		}
	}
	else if (Event.Type == EUiPointerEventType::Down && Event.Button == EUiPointerButton::Primary)
	{
		const FUiRect View = GetContentRect();
		const FUiRect Track{View.Right() - m_BarWidth, View.Y, m_BarWidth, View.Height};
		if (Track.Contains(Event.Position) && GetScrollMaximum() > 0 && CapturePointer())
		{
			const auto Thumb = GetScrollThumbRect();
			m_GrabOffset = Thumb.Contains(Event.Position) ? Event.Position.Y - Thumb.Y : Thumb.Height * 0.5f;
			Event.bHandled = true;
			DragTo_Internal(Event.Position.Y);
		}
	}
	else if (Event.Type == EUiPointerEventType::Move && HasPointerCapture())
	{
		Event.bHandled = true;
		DragTo_Internal(Event.Position.Y);
	}
	else if (Event.Type == EUiPointerEventType::Up && HasPointerCapture() && Event.Button == EUiPointerButton::Primary)
	{
		Event.bHandled = true;
		ReleasePointer();
	}
}

void DUiScrollView::OnNavigationEvent(FUiNavigationEvent& Event)
{
	if (Event.Command == EUiNavigationCommand::Down || Event.Command == EUiNavigationCommand::Up)
	{
		const Toolbox::f64 Step = m_WheelStep * (m_bRelativeWheel ? m_ViewHeight : 1);
		SetScrollOffset(m_Offset + (Event.Command == EUiNavigationCommand::Down ? Step : -Step));
		Event.bHandled = true;
	}
}
} // namespace Dxf
// namespace Dxf
