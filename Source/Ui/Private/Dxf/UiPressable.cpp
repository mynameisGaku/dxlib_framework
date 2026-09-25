// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiPressable.h"
namespace Dxf
{
// 押して決定する部品。
DUiPressable::DUiPressable()
{
	SetHitTest(EUiHitTest::Self);
	SetFocusable(true);
}
// ポインターの出来事。
void DUiPressable::OnPointerEvent(FUiPointerEvent& Event)
{
	switch (Event.Type)
	{
	case EUiPointerEventType::Down:
		if (Event.Button == EUiPointerButton::Primary && CapturePointer())
		{
			SetPressed_Internal(true);
			Event.bHandled = true;
		}
		break;
	case EUiPointerEventType::Move:
		if (HasPointerCapture())
		{
			// 押したまま外へ出たら押下の見た目を戻し、戻れば押下に戻す。
			SetPressed_Internal(Event.bOverTarget);
			Event.bHandled = true;
		}
		break;
	case EUiPointerEventType::Up:
		if (Event.Button == EUiPointerButton::Primary && HasPointerCapture())
		{
			ReleasePointer();
			SetPressed_Internal(false);
			Event.bHandled = true;
			if (Event.bOverTarget && IsEnabledInTree() && IsVisibleInTree())
			{
				OnActivated();
			}
		}
		break;
	case EUiPointerEventType::CaptureLost:
		SetPressed_Internal(false);
		break;
	default:
		break;
	}
}
// 決定の操作。
void DUiPressable::OnNavigationEvent(FUiNavigationEvent& Event)
{
	if (Event.Command == EUiNavigationCommand::Confirm && !Event.bRepeat)
	{
		Event.bHandled = true;
		OnActivated();
	}
}
// 決定したとき。
void DUiPressable::OnActivated()
{
	++m_Activations;
	m_Clicked.Emit();
}
} // namespace Dxf
