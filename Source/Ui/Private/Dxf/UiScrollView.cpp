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

void DUiScrollView::SetScrollAxes(EUiScrollAxes Axes)
{
	if (Axes != EUiScrollAxes::Vertical && Axes != EUiScrollAxes::Horizontal && Axes != EUiScrollAxes::Both)
	{
		throw Toolbox::FException("Invalid scroll axes");
	}
	if (Axes == m_Axes)
	{
		return;
	}
	m_Axes = Axes;
	// 無効にした軸は先頭へ戻し、残りの移動も捨てる。
	for (Toolbox::size_t Axis = 0; Axis < 2; ++Axis)
	{
		if (!HasAxis_Internal(Axis))
		{
			m_Offset[Axis] = 0;
			Stop_Internal(Axis);
		}
	}
	RefreshUpdate_Internal();
	InvalidateMeasure();
}

void DUiScrollView::SetInertia(const FUiScrollInertia& Inertia)
{
	if (!Toolbox::IsFinite(Inertia.DecayPerSecond) || !(Inertia.DecayPerSecond > 0) ||
	    !Toolbox::IsFinite(Inertia.SnapDistance) || !(Inertia.SnapDistance > 0))
	{
		throw Toolbox::FException("Invalid scroll inertia");
	}
	m_Inertia = Inertia;
	if (!m_Inertia.bEnabled)
	{
		// 無効にしたら残りの移動をすぐに終える（最終位置は慣性の有無で変わらない）。
		for (Toolbox::size_t Axis = 0; Axis < 2; ++Axis)
		{
			if (m_bMoving[Axis])
			{
				SetOffset_Internal(Axis, m_Target[Axis]);
			}
			Stop_Internal(Axis);
		}
	}
	RefreshUpdate_Internal();
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
	SetOffset_Internal(1, Offset);
	Stop_Internal(1);
	RefreshUpdate_Internal();
}

void DUiScrollView::SetScrollOffsetX(Toolbox::f64 Offset)
{
	if (!Toolbox::IsFinite(Offset))
	{
		throw Toolbox::FException("Invalid scroll offset");
	}
	SetOffset_Internal(0, Offset);
	Stop_Internal(0);
	RefreshUpdate_Internal();
}

void DUiScrollView::SetOffset_Internal(Toolbox::size_t Axis, Toolbox::f64 Offset)
{
	Toolbox::f64 Next = 0;
	if (HasAxis_Internal(Axis))
	{
		Next =
		    m_ViewSize[Axis] > 0 ? Toolbox::Clamp(Offset, 0.0, GetMaximum_Internal(Axis)) : Toolbox::Max(0.0, Offset);
	}
	if (Next != m_Offset[Axis])
	{
		m_Offset[Axis] = Next;
		InvalidateArrange();
	}
}

Toolbox::f64 DUiScrollView::GetStep_Internal(Toolbox::size_t Axis) const noexcept
{
	return m_WheelStep * (m_bRelativeWheel ? m_ViewSize[Axis] : 1);
}

Toolbox::f64 DUiScrollView::ScrollBy_Internal(Toolbox::size_t Axis, Toolbox::f64 Notches)
{
	const Toolbox::f64 Step = GetStep_Internal(Axis);
	if (!HasAxis_Internal(Axis) || !(Step > 0))
	{
		return 0;
	}
	// 慣性の途中なら、到着する位置から続けて進める。
	const Toolbox::f64 Target = m_bMoving[Axis] ? m_Target[Axis] : m_Offset[Axis];
	const Toolbox::f64 Wanted = Target + Notches * Step;
	const Toolbox::f64 Reached =
	    m_ViewSize[Axis] > 0 ? Toolbox::Clamp(Wanted, 0.0, GetMaximum_Internal(Axis)) : Toolbox::Max(0.0, Wanted);
	const Toolbox::f64 Moved = Reached - Target;
	if (m_Inertia.bEnabled)
	{
		m_Target[Axis] = Reached;
		m_bMoving[Axis] = Reached != m_Offset[Axis];
		RefreshUpdate_Internal();
	}
	else
	{
		SetOffset_Internal(Axis, Reached);
	}
	return Moved / Step;
}

void DUiScrollView::OnUpdate(const FUiUpdateContext& Context)
{
	// 残りの距離は e^(-λt) で減る。フレームの刻みに依らず、同じ時刻の残りは同じになる。
	const Toolbox::f64 Keep = Toolbox::Exp(-m_Inertia.DecayPerSecond * Context.DeltaSeconds);
	for (Toolbox::size_t Axis = 0; Axis < 2; ++Axis)
	{
		if (!m_bMoving[Axis])
		{
			continue;
		}
		// 到着点からの残りだけを減らす（到着時は到着点へ正確に合わせる）。
		const Toolbox::f64 Remaining = (m_Target[Axis] - m_Offset[Axis]) * Keep;
		const bool bArrived = Toolbox::Abs(Remaining) < m_Inertia.SnapDistance;
		SetOffset_Internal(Axis, bArrived ? m_Target[Axis] : m_Target[Axis] - Remaining);
		if (bArrived)
		{
			Stop_Internal(Axis);
		}
	}
	RefreshUpdate_Internal();
}

void DUiScrollView::RefreshUpdate_Internal()
{
	const bool bMoving = m_bMoving[0] || m_bMoving[1];
	if (bMoving != WantsUpdate())
	{
		SetWantsUpdate(bMoving);
	}
}

FUiSize DUiScrollView::MeasureScrollable(FUiLayoutContext& Context, FUiSize Available)
{
	return m_Content.Get() != nullptr ? Context.MeasureChild(*m_Content.Get(), Available) : FUiSize{};
}

FUiSize DUiScrollView::OnMeasure(FUiLayoutContext& Context, FUiSize Available)
{
	const Toolbox::f32 BarX = HasAxis_Internal(1) ? m_BarWidth : 0.0f;
	const Toolbox::f32 BarY = HasAxis_Internal(0) ? m_BarWidth : 0.0f;
	const FUiSize Inner{Toolbox::Max(0.0f, Available.Width - BarX), Toolbox::Max(0.0f, Available.Height - BarY)};
	// スクロールする軸は上限なしで測る。
	const FUiSize Wanted = MeasureScrollable(
	    Context, {HasAxis_Internal(0) ? UiUnbounded : Inner.Width, HasAxis_Internal(1) ? UiUnbounded : Inner.Height});
	m_ContentSize[0] = Wanted.Width;
	m_ContentSize[1] = Wanted.Height;
	return {Toolbox::Min(Available.Width, Wanted.Width + BarX), Toolbox::Min(Available.Height, Wanted.Height + BarY)};
}

void DUiScrollView::ArrangeScrollable(FUiLayoutContext& Context, const FUiRect& View)
{
	if (DUiElement* Content = m_Content.Get())
	{
		const Toolbox::f32 Width =
		    HasAxis_Internal(0) ? static_cast<Toolbox::f32>(Toolbox::Max(m_ContentSize[0], m_ViewSize[0])) : View.Width;
		const Toolbox::f32 Height = HasAxis_Internal(1)
		                                ? static_cast<Toolbox::f32>(Toolbox::Max(m_ContentSize[1], m_ViewSize[1]))
		                                : View.Height;
		Context.ArrangeChild(*Content, {View.X - static_cast<Toolbox::f32>(m_Offset[0]),
		                                View.Y - static_cast<Toolbox::f32>(m_Offset[1]), Width, Height});
	}
}

FUiRect DUiScrollView::GetViewRect_Internal() const noexcept
{
	const FUiRect Content = GetContentRect();
	return {Content.X, Content.Y, Toolbox::Max(0.0f, Content.Width - (HasAxis_Internal(1) ? m_BarWidth : 0.0f)),
	        Toolbox::Max(0.0f, Content.Height - (HasAxis_Internal(0) ? m_BarWidth : 0.0f))};
}

void DUiScrollView::OnArrange(FUiLayoutContext& Context, const FUiRect& Content)
{
	const FUiRect View{Content.X, Content.Y,
	                   Toolbox::Max(0.0f, Content.Width - (HasAxis_Internal(1) ? m_BarWidth : 0.0f)),
	                   Toolbox::Max(0.0f, Content.Height - (HasAxis_Internal(0) ? m_BarWidth : 0.0f))};
	m_ViewSize[0] = View.Width;
	m_ViewSize[1] = View.Height;
	for (Toolbox::size_t Axis = 0; Axis < 2; ++Axis)
	{
		m_Offset[Axis] = Toolbox::Clamp(m_Offset[Axis], 0.0, GetMaximum_Internal(Axis));
		// 慣性の到着点も新しい範囲へ収める。
		m_Target[Axis] = Toolbox::Clamp(m_Target[Axis], 0.0, GetMaximum_Internal(Axis));
		if (!m_bMoving[Axis] || m_Target[Axis] == m_Offset[Axis])
		{
			Stop_Internal(Axis);
		}
	}
	RefreshUpdate_Internal();
	ArrangeScrollable(Context, View);
}

FUiRect DUiScrollView::GetScrollThumbRect() const noexcept
{
	const FUiRect View = GetViewRect_Internal();
	if (!(GetScrollMaximum() > 0) || !(View.Height > 0))
	{
		return {};
	}
	const Toolbox::f32 Height = Toolbox::Min(
	    View.Height, Toolbox::Max(16.0f, static_cast<Toolbox::f32>(View.Height * View.Height / m_ContentSize[1])));
	return {View.Right(), View.Y + static_cast<Toolbox::f32>(m_Offset[1] / GetScrollMaximum()) * (View.Height - Height),
	        m_BarWidth, Height};
}

FUiRect DUiScrollView::GetScrollThumbRectX() const noexcept
{
	const FUiRect View = GetViewRect_Internal();
	if (!(GetScrollMaximumX() > 0) || !(View.Width > 0))
	{
		return {};
	}
	const Toolbox::f32 Width = Toolbox::Min(
	    View.Width, Toolbox::Max(16.0f, static_cast<Toolbox::f32>(View.Width * View.Width / m_ContentSize[0])));
	return {View.X + static_cast<Toolbox::f32>(m_Offset[0] / GetScrollMaximumX()) * (View.Width - Width), View.Bottom(),
	        Width, m_BarWidth};
}

void DUiScrollView::OnDrawOverlay(FUiDrawContext& Context) const
{
	DUiElement::OnDrawOverlay(Context);
	const FUiRect View = GetViewRect_Internal();
	const FUiRect ThumbY = GetScrollThumbRect();
	if (!ThumbY.IsEmpty())
	{
		Context.FillRect({View.Right(), View.Y, m_BarWidth, View.Height}, GetStyle().Background);
		Context.FillRect(ThumbY, GetStyle().Foreground);
	}
	const FUiRect ThumbX = GetScrollThumbRectX();
	if (!ThumbX.IsEmpty())
	{
		Context.FillRect({View.X, View.Bottom(), View.Width, m_BarWidth}, GetStyle().Background);
		Context.FillRect(ThumbX, GetStyle().Foreground);
	}
}

void DUiScrollView::DragTo_Internal(FVector2 Position)
{
	const Toolbox::size_t Axis = m_DragAxis;
	const FUiRect View = GetViewRect_Internal();
	const FUiRect Thumb = Axis == 0 ? GetScrollThumbRectX() : GetScrollThumbRect();
	const Toolbox::f64 Span = Axis == 0 ? static_cast<Toolbox::f64>(View.Width) - Thumb.Width
	                                    : static_cast<Toolbox::f64>(View.Height) - Thumb.Height;
	if (Span > 0)
	{
		const Toolbox::f64 Along = Axis == 0 ? Position.X - View.X : Position.Y - View.Y;
		SetOffset_Internal(Axis, (Along - m_GrabOffset) / Span * GetMaximum_Internal(Axis));
		Stop_Internal(Axis);
		RefreshUpdate_Internal();
	}
}

void DUiScrollView::OnPointerEvent(FUiPointerEvent& Event)
{
	if (Event.Type == EUiPointerEventType::Wheel)
	{
		// 軸ごとに使った分だけ減らし、残りは外側のスクロールへ渡す。
		if (Event.WheelDelta != 0)
		{
			Event.WheelDelta -= static_cast<Toolbox::f32>(ScrollBy_Internal(1, Event.WheelDelta));
		}
		if (Event.WheelDeltaX != 0)
		{
			Event.WheelDeltaX -= static_cast<Toolbox::f32>(ScrollBy_Internal(0, Event.WheelDeltaX));
		}
		Event.bHandled = Toolbox::Abs(Event.WheelDelta) < 1.0e-6f && Toolbox::Abs(Event.WheelDeltaX) < 1.0e-6f;
	}
	else if (Event.Type == EUiPointerEventType::Down && Event.Button == EUiPointerButton::Primary)
	{
		const FUiRect View = GetViewRect_Internal();
		const FUiRect TrackY{View.Right(), View.Y, m_BarWidth, View.Height};
		const FUiRect TrackX{View.X, View.Bottom(), View.Width, m_BarWidth};
		const bool bY = HasAxis_Internal(1) && TrackY.Contains(Event.Position) && GetScrollMaximum() > 0;
		const bool bX = !bY && HasAxis_Internal(0) && TrackX.Contains(Event.Position) && GetScrollMaximumX() > 0;
		if ((bX || bY) && CapturePointer())
		{
			m_DragAxis = bX ? 0 : 1;
			const FUiRect Thumb = bX ? GetScrollThumbRectX() : GetScrollThumbRect();
			if (bX)
			{
				m_GrabOffset = Thumb.Contains(Event.Position) ? Event.Position.X - Thumb.X : Thumb.Width * 0.5f;
			}
			else
			{
				m_GrabOffset = Thumb.Contains(Event.Position) ? Event.Position.Y - Thumb.Y : Thumb.Height * 0.5f;
			}
			Event.bHandled = true;
			DragTo_Internal(Event.Position);
		}
	}
	else if (Event.Type == EUiPointerEventType::Move && HasPointerCapture())
	{
		Event.bHandled = true;
		DragTo_Internal(Event.Position);
	}
	else if (Event.Type == EUiPointerEventType::Up && HasPointerCapture() && Event.Button == EUiPointerButton::Primary)
	{
		Event.bHandled = true;
		ReleasePointer();
	}
}

void DUiScrollView::OnNavigationEvent(FUiNavigationEvent& Event)
{
	const bool bVertical = Event.Command == EUiNavigationCommand::Down || Event.Command == EUiNavigationCommand::Up;
	const bool bHorizontal =
	    Event.Command == EUiNavigationCommand::Right || Event.Command == EUiNavigationCommand::Left;
	// スクロールしない軸の方向操作はフォーカスの移動へ残す。
	if ((bVertical && HasAxis_Internal(1)) || (bHorizontal && HasAxis_Internal(0)))
	{
		const bool bForward =
		    Event.Command == EUiNavigationCommand::Down || Event.Command == EUiNavigationCommand::Right;
		(void)ScrollBy_Internal(bVertical ? 1 : 0, bForward ? 1.0 : -1.0);
		Event.bHandled = true;
	}
}
} // namespace Dxf
