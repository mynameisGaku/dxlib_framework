// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiPopup.h"
#include "Dxf/UiRoot.h"
namespace Dxf
{
DUiPopup::DUiPopup(bool bModal) : m_bModal(bModal)
{
	SetStack(EUiStackMode::Overlay);
	SetStyleId("Popup.Backdrop");
	SetWidth(FUiLength::Fill());
	SetHeight(FUiLength::Fill());
	SetHitTest(EUiHitTest::Self);
	SetPadding(FUiThickness::All(0));
}

void DUiPopup::SetContent(const TUiRef<DUiElement>& Content)
{
	if (Content.GetId() == m_Content.GetId())
	{
		return;
	}
	if (GetRoot() == nullptr || Content.Get() == nullptr)
	{
		throw Toolbox::FException("Popup content is invalid");
	}
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
}

TResult<void> DUiPopup::Open()
{
	if (GetRoot() == nullptr)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Popup has no root");
	}
	if (IsAttached())
	{
		return {};
	}

	return GetRoot()->AddToLayer(EUiLayer::Popup, GetRef());
}

void DUiPopup::OnAttach()
{
	if (m_bModal)
	{
		GetRoot()->PushModal_Internal(GetRef());
	}
}

void DUiPopup::Close()
{
	if (GetRoot() == nullptr || !IsAttached())
	{
		return;
	}
	const auto Self = GetRef<DUiPopup>();
	auto Result = GetRoot()->Remove(GetRef());
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
	if (DUiPopup* Popup = Self.Get())
	{
		Popup->m_Closed.Emit();
	}
}

void DUiPopup::OnPointerEvent(FUiPointerEvent& Event)
{
	if (Event.Type == EUiPointerEventType::Down || Event.Type == EUiPointerEventType::Wheel)
	{
		Event.bHandled = m_bModal;
		if (Event.Type == EUiPointerEventType::Down && m_bCloseOnBackdrop &&
		    (m_Content.Get() == nullptr || !m_Content.Get()->GetRect().Contains(Event.Position)))
		{
			Event.bHandled = true;
			Close();
		}
	}
}

void DUiPopup::OnNavigationEvent(FUiNavigationEvent& Event)
{
	if (Event.Command == EUiNavigationCommand::Cancel)
	{
		Event.bHandled = true;
		Close();
	}
}
} // namespace Dxf
// namespace Dxf
