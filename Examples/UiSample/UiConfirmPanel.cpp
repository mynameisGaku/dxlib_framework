// SPDX-License-Identifier: NOASSERTION
#include "UiConfirmPanel.h"
#include "UiSampleWidgets.h"
namespace Dxf::UiSample
{
DUiConfirmPanel::DUiConfirmPanel(Toolbox::TSharedPtr<FUiSampleState> State)
    : DUiPanel(EUiStackMode::Vertical, 12), m_pState(Toolbox::Move(State))
{
	SetWidth(FUiLength::Fixed(420));
	SetHeight(FUiLength::Content());
	SetAlign(EUiAlign::Center, EUiAlign::Center);
	SetPadding(FUiThickness::All(20));
}

void DUiConfirmPanel::OnFirstAttach()
{
	auto Message = CreateChild<DUiLabel>("タイトルへ戻りますか？");
	SetupSampleLabel(*Message.Get(), "ConfirmMessage", 40);
	m_Yes = CreateChild<DUiButton>("はい");
	SetupSampleButton(*m_Yes.Get(), "ConfirmYes");
	m_No = CreateChild<DUiButton>("いいえ");
	SetupSampleButton(*m_No.Get(), "ConfirmNo");
}

void DUiConfirmPanel::OnAttach()
{
	const auto State = m_pState;
	GetAttachScope().Add(m_Yes.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::Title;
	    }));
	GetAttachScope().Add(m_No.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::CancelConfirm;
	    }));
}
} // namespace Dxf::UiSample
