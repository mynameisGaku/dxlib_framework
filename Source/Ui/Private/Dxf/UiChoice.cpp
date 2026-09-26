// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiChoice.h"
namespace Dxf
{
void DUiChoice::SetOptions(Toolbox::TVector<Toolbox::FString> Options)
{
	m_Options = Toolbox::Move(Options);
	m_Selected = 0;
	SetText(m_Options.IsEmpty() ? Toolbox::FString() : m_Options[0]);
	SetEnabled(!m_Options.IsEmpty());
}

void DUiChoice::SetSelectedIndex(Toolbox::size_t Index)
{
	if (Index >= m_Options.Size())
	{
		throw Toolbox::FException("UI choice index is out of range");
	}
	m_Selected = Index;
	SetText(m_Options[Index]);
}

void DUiChoice::Change_Internal(bool bNext)
{
	if (m_Options.IsEmpty())
	{
		return;
	}
	SetSelectedIndex(bNext ? (m_Selected + 1) % m_Options.Size()
	                       : (m_Selected + m_Options.Size() - 1) % m_Options.Size());
	m_Changed.Emit(m_Selected);
}

void DUiChoice::OnActivated()
{
	Change_Internal(true);
}

void DUiChoice::OnNavigationEvent(FUiNavigationEvent& Event)
{
	if (Event.Command == EUiNavigationCommand::Left || Event.Command == EUiNavigationCommand::Right)
	{
		Event.bHandled = true;
		Change_Internal(Event.Command == EUiNavigationCommand::Right);
	}
	else
	{
		DUiButton::OnNavigationEvent(Event);
	}
}
} // namespace Dxf
// namespace Dxf
