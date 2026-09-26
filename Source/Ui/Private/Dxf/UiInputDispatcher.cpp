// SPDX-License-Identifier: NOASSERTION
#include "UiInputDispatcher.h"
#include "UiRootState.h"
namespace Dxf::Detail
{
namespace
{
// 要素とその子孫を当たり判定する（子が前面）。
DUiElement* HitElement_Internal(DUiElement& Element, FVector2 Position)
{
	if (!FUiRootState::IsLive(&Element) || Element.GetVisibility() != EUiVisibility::Visible ||
	    Element.GetHitTest() == EUiHitTest::None || Element.HasLayoutError())
	{
		return nullptr;
	}
	// クリップの外は自身も子も受けない（クリップ外の要素は入力にも反応しない）。
	if (!Element.GetClipRect().Contains(Position))
	{
		return nullptr;
	}
	for (Toolbox::size_t Index = Element.GetChildCount(); Index > 0; --Index)
	{
		DUiElement* Child = Element.GetChild(Index - 1);
		if (Child == nullptr)
		{
			continue;
		}
		if (DUiElement* Hit = HitElement_Internal(*Child, Position))
		{
			return Hit;
		}
	}
	if (Element.GetHitTest() == EUiHitTest::Self && Element.GetRect().Contains(Position))
	{
		return &Element;
	}
	return nullptr;
}
// 祖先（自身を含む）か。
bool IsSelfOrAncestor_Internal(const DUiElement* Ancestor, const DUiElement* Element) noexcept
{
	for (const DUiElement* Current = Element; Current != nullptr; Current = Current->GetParent())
	{
		if (Current == Ancestor)
		{
			return true;
		}
	}
	return false;
}
} // namespace

// 最前面の要素。
DUiElement* FUiInputDispatcher::HitTest(FUiRootState& State, FVector2 Position)
{
	if (State.bShuttingDown || !State.Surface.IsDisplayable() || !Toolbox::IsFinite(Position.X) ||
	    !Toolbox::IsFinite(Position.Y))
	{
		return nullptr;
	}
	DUiElement* Hit = nullptr;
	for (Toolbox::size_t Layer = State.Layers.Size(); Layer > 0 && Hit == nullptr; --Layer)
	{
		Hit = HitElement_Internal(*State.Layers[Layer - 1], Position);
	}
	// Modalの外は、Modalが遮る（背後の要素へ抜けない）。
	DUiElement* Modal = State.GetTopModal();
	if (Modal != nullptr && !IsSelfOrAncestor_Internal(Modal, Hit))
	{
		// ツールチップの領域はModalより前面だが入力を受けない。
		return Modal;
	}
	return Hit;
}
// 一つの要素へ届ける。
void FUiInputDispatcher::Deliver_Internal(FUiRootState& State, const TUiRef<DUiElement>& Target, FUiPointerEvent& Event)
{
	// 直前に参照を確かめる（前の要素の処理で破棄・切断されていれば届けない）。
	DUiElement* Element = Target.Get();
	if (State.bShuttingDown || !FUiRootState::IsLive(Element) || !Element->IsEnabledInTree())
	{
		return;
	}
	FUiRootState::FDispatchScope Scope(State);
	Element->Pointer_Internal(Event);
}
// 対象から親へ届ける。
void FUiInputDispatcher::Bubble_Internal(FUiRootState& State, DUiElement* Target, FUiPointerEvent& Event)
{
	// 経路を参照で先に集める（配布中の変更に備える）。
	Toolbox::TVector<TUiRef<DUiElement>> Path;
	for (DUiElement* Current = Target; Current != nullptr; Current = Current->GetParent())
	{
		Path.PushBack(Current->GetRef());
	}
	for (const auto& Handle : Path)
	{
		Deliver_Internal(State, Handle, Event);
		if (Event.bHandled)
		{
			return;
		}
	}
}
// ホバーの経路を更新する。
void FUiInputDispatcher::UpdateHover_Internal(FUiRootState& State, DUiElement* Hit)
{
	Toolbox::TVector<TUiRef<DUiElement>> NewPath;
	for (DUiElement* Current = Hit; Current != nullptr; Current = Current->GetParent())
	{
		NewPath.PushBack(Current->GetRef());
	}
	// 同じ経路なら何もしない。
	bool bSame = NewPath.Size() == m_HoverPath.Size();
	for (Toolbox::size_t Index = 0; bSame && Index < NewPath.Size(); ++Index)
	{
		bSame = NewPath[Index].GetId() == m_HoverPath[Index].GetId();
	}
	if (bSame)
	{
		return;
	}
	auto Contains = [](const Toolbox::TVector<TUiRef<DUiElement>>& Path, const TUiRef<DUiElement>& Handle)
	{
		for (const auto& Item : Path)
		{
			if (Item.GetId() == Handle.GetId())
			{
				return true;
			}
		}
		return false;
	};
	auto OldPath = Toolbox::Move(m_HoverPath);
	m_HoverPath = NewPath;
	m_Hovered = Hit != nullptr ? Hit->GetRef() : TUiRef<DUiElement>{};
	for (const auto& Handle : OldPath)
	{
		if (Contains(NewPath, Handle))
		{
			continue;
		}
		if (DUiElement* Element = Handle.Get())
		{
			FUiRootState::Hovered(*Element) = false;
			FUiRootState::NotifyStateChanged(*Element);
			FUiPointerEvent Leave;
			Leave.Type = EUiPointerEventType::Leave;
			Leave.Position = m_LastPosition;
			Deliver_Internal(State, Handle, Leave);
		}
	}
	for (Toolbox::size_t Index = NewPath.Size(); Index > 0; --Index)
	{
		const auto& Handle = NewPath[Index - 1];
		if (Contains(OldPath, Handle))
		{
			continue;
		}
		if (DUiElement* Element = Handle.Get())
		{
			FUiRootState::Hovered(*Element) = true;
			FUiRootState::NotifyStateChanged(*Element);
			FUiPointerEvent Enter;
			Enter.Type = EUiPointerEventType::Enter;
			Enter.Position = m_LastPosition;
			Deliver_Internal(State, Handle, Enter);
		}
	}
}
// キャプチャを失わせる。
void FUiInputDispatcher::LoseCapture_Internal(FUiRootState& State) noexcept
{
	const TUiRef<DUiElement> Captured = Toolbox::Move(m_Captured);
	m_Captured = {};
	DUiElement* Element = Captured.Get();
	if (Element == nullptr)
	{
		return;
	}
	try
	{
		FUiPointerEvent Lost;
		Lost.Type = EUiPointerEventType::CaptureLost;
		Lost.Position = m_LastPosition;
		Lost.Button = m_CaptureButton;
		FUiRootState::FDispatchScope Scope(State);
		Element->Pointer_Internal(Lost);
	}
	catch (...)
	{
	}
	Element = Captured.Get();
	if (Element != nullptr && FUiRootState::Pressed(*Element))
	{
		FUiRootState::Pressed(*Element) = false;
		try
		{
			FUiRootState::NotifyStateChanged(*Element);
		}
		catch (...)
		{
		}
	}
}
// キャプチャする。
bool FUiInputDispatcher::Capture(FUiRootState& State, DUiElement& Element)
{
	(void)State;
	if (!m_bDeliveringDown || !FUiRootState::IsLive(&Element) || !Element.IsEnabledInTree())
	{
		return false;
	}
	if (DUiElement* Current = m_Captured.Get())
	{
		return Current == &Element;
	}
	m_Captured = Element.GetRef();
	m_CaptureButton = m_DownButton;
	return true;
}
// キャプチャを解く。
void FUiInputDispatcher::Release(FUiRootState& State, DUiElement& Element) noexcept
{
	(void)State;
	if (m_Captured.GetId() == Element.GetRef().GetId())
	{
		m_Captured = {};
	}
}
// 切断した要素のホバー・キャプチャを外す。
void FUiInputDispatcher::OnElementDetached(FUiRootState& State, DUiElement& Element) noexcept
{
	const FObjectId Id = Element.GetRef().GetId();
	if (m_Captured.GetId() == Id)
	{
		// 切断した要素へはCaptureLostを届けない（すでにOnDetach済み）。押下の状態だけ戻す。
		m_Captured = {};
		(void)State;
	}
	for (Toolbox::size_t Index = 0; Index < m_HoverPath.Size(); ++Index)
	{
		if (m_HoverPath[Index].GetId() == Id)
		{
			// 経路の途中が切れたので、次のフレームで作り直す。
			m_HoverPath.Clear();
			m_Hovered = {};
			break;
		}
	}
}
// キャプチャの有効性を確かめる。
void FUiInputDispatcher::ValidateCapture(FUiRootState& State) noexcept
{
	DUiElement* Captured = m_Captured.Get();
	if (Captured == nullptr)
	{
		m_Captured = {};
		return;
	}
	if (!FUiRootState::IsLive(Captured) || !Captured->IsVisibleInTree() || !Captured->IsEnabledInTree())
	{
		LoseCapture_Internal(State);
	}
}
// ホバーとキャプチャを外す。
void FUiInputDispatcher::Reset(FUiRootState& State) noexcept
{
	LoseCapture_Internal(State);
	try
	{
		UpdateHover_Internal(State, nullptr);
	}
	catch (...)
	{
		m_HoverPath.Clear();
		m_Hovered = {};
	}
}
// 一フレームのポインターを処理する。
void FUiInputDispatcher::Process(FUiRootState& State, const FUiPointerFrame& Frame, FUiInputResult& Result)
{
	ValidateCapture(State);
	const bool bMoved = Frame.bPresent != m_bLastPresent || Frame.Position.X != m_LastPosition.X ||
	                    Frame.Position.Y != m_LastPosition.Y;
	m_LastPosition = Frame.Position;
	m_bLastPresent = Frame.bPresent;
	DUiElement* Hit = Frame.bPresent ? HitTest(State, Frame.Position) : nullptr;
	UpdateHover_Internal(State, Hit);
	// ホバーの出来事で木が変わった場合に備えて引き直す。
	Hit = Frame.bPresent ? HitTest(State, Frame.Position) : nullptr;
	if (bMoved)
	{
		FUiPointerEvent Move;
		Move.Type = EUiPointerEventType::Move;
		Move.Position = Frame.Position;
		if (DUiElement* Captured = m_Captured.Get())
		{
			Move.bOverTarget = IsSelfOrAncestor_Internal(Captured, Hit);
			Deliver_Internal(State, Captured->GetRef(), Move);
		}
		else if (Hit != nullptr)
		{
			Move.bOverTarget = true;
			Bubble_Internal(State, Hit, Move);
		}
	}
	for (Toolbox::size_t Button = 0; Button < Frame.Pressed.Size(); ++Button)
	{
		if (!Frame.Pressed[Button])
		{
			continue;
		}
		if (m_Captured.Get() != nullptr)
		{
			// 別のボタンの操作中は、新しい押下をUIの操作として受ける（ゲームへ流さない）。
			Result.ButtonsClaimed[Button] = true;
			continue;
		}
		Hit = Frame.bPresent ? HitTest(State, Frame.Position) : nullptr;
		if (Hit == nullptr)
		{
			continue;
		}
		Result.ButtonsClaimed[Button] = true;
		FUiPointerEvent Down;
		Down.Type = EUiPointerEventType::Down;
		Down.Position = Frame.Position;
		Down.Button = static_cast<EUiPointerButton>(Button);
		Down.bOverTarget = true;
		m_bDeliveringDown = true;
		m_DownButton = Down.Button;
		try
		{
			Bubble_Internal(State, Hit, Down);
		}
		catch (...)
		{
			m_bDeliveringDown = false;
			throw;
		}
		m_bDeliveringDown = false;
		// 押した要素（またはその祖先）でフォーカスを受けるものへフォーカスを移す。
		for (DUiElement* Current = Hit; Current != nullptr; Current = Current->GetParent())
		{
			if (Current->IsFocusable() && FUiRootState::IsLive(Current) && Current->IsEnabledInTree())
			{
				State.Focus.SetFocus(State, Current);
				break;
			}
		}
		State.Tooltip.Hide(State);
	}
	for (Toolbox::size_t Button = 0; Button < Frame.Released.Size(); ++Button)
	{
		if (!Frame.Released[Button])
		{
			continue;
		}
		Hit = Frame.bPresent ? HitTest(State, Frame.Position) : nullptr;
		FUiPointerEvent Up;
		Up.Type = EUiPointerEventType::Up;
		Up.Position = Frame.Position;
		Up.Button = static_cast<EUiPointerButton>(Button);
		DUiElement* Captured = m_Captured.Get();
		if (Captured != nullptr && static_cast<Toolbox::size_t>(m_CaptureButton) == Button)
		{
			// 押し始めた要素の上で離したときだけ決定になる（bOverTarget）。
			Up.bOverTarget = Frame.bPresent && IsSelfOrAncestor_Internal(Captured, Hit);
			const TUiRef<DUiElement> Handle = Captured->GetRef();
			Deliver_Internal(State, Handle, Up);
			if (m_Captured.GetId() == Handle.GetId())
			{
				m_Captured = {};
			}
			if (DUiElement* Element = Handle.Get(); Element != nullptr && FUiRootState::Pressed(*Element))
			{
				FUiRootState::Pressed(*Element) = false;
				FUiRootState::NotifyStateChanged(*Element);
			}
		}
		else if (Hit != nullptr && Captured == nullptr)
		{
			Up.bOverTarget = true;
			Bubble_Internal(State, Hit, Up);
		}
	}
	if (Frame.WheelNotches != 0 && Frame.bPresent)
	{
		Hit = HitTest(State, Frame.Position);
		if (Hit != nullptr)
		{
			FUiPointerEvent Wheel;
			Wheel.Type = EUiPointerEventType::Wheel;
			Wheel.Position = Frame.Position;
			Wheel.WheelDelta = Frame.WheelNotches;
			Wheel.bOverTarget = true;
			Bubble_Internal(State, Hit, Wheel);
			// UIの上のホイールはゲームへ流さない（スクロールしなかった分も含む）。
			Result.bWheelConsumed = true;
		}
	}
	Result.bPointerOverUi = Result.bPointerOverUi || (Frame.bPresent && HitTest(State, Frame.Position) != nullptr) ||
	                        m_Captured.Get() != nullptr;
}
} // namespace Dxf::Detail
