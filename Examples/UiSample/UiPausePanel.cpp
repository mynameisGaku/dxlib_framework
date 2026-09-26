// SPDX-License-Identifier: NOASSERTION
#include "UiPausePanel.h"
#include "UiSampleWidgets.h"
namespace Dxf::UiSample
{
DUiPausePanel::DUiPausePanel(Toolbox::TSharedPtr<FUiSampleState> State)
    : DUiPanel(EUiStackMode::Vertical, 12), m_pState(Toolbox::Move(State))
{
	SetWidth(FUiLength::Fixed(420));
	SetHeight(FUiLength::Content());
	SetAlign(EUiAlign::Center, EUiAlign::Center);
	SetPadding(FUiThickness::All(20));
}

void DUiPausePanel::OnFirstAttach()
{
	auto Title = CreateChild<DUiLabel>("PAUSED");
	SetupSampleLabel(*Title.Get(), "Heading", 40);
	m_Resume = CreateChild<DUiButton>("再開");
	SetupSampleButton(*m_Resume.Get(), "Resume");
	m_Settings = CreateChild<DUiButton>("設定");
	SetupSampleButton(*m_Settings.Get(), "Settings");
	m_Title = CreateChild<DUiButton>("タイトルへ戻る");
	SetupSampleButton(*m_Title.Get(), "Title");
}

void DUiPausePanel::OnAttach()
{
	const auto State = m_pState;
	GetAttachScope().Add(m_Resume.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::Pause;
	    }));
	GetAttachScope().Add(m_Settings.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::Settings;
	    }));
	GetAttachScope().Add(m_Title.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    // タイトルへ戻る前に確認する。
		    State->Action = EUiSampleAction::ConfirmTitle;
	    }));
}
} // namespace Dxf::UiSample
