// SPDX-License-Identifier: NOASSERTION
#include "UiTitleScreen.h"
#include "UiSampleWidgets.h"
namespace Dxf::UiSample
{
DUiTitleScreen::DUiTitleScreen(Toolbox::TSharedPtr<FUiSampleState> State)
    : DUiPanel(EUiStackMode::Vertical, 12), m_pState(Toolbox::Move(State))
{
	SetWidth(FUiLength::Fixed(420));
	SetHeight(FUiLength::Content());
	SetAlign(EUiAlign::Center, EUiAlign::Center);
	SetPadding(FUiThickness::All(20));
}

void DUiTitleScreen::OnFirstAttach()
{
	auto Title = CreateChild<DUiLabel>("共通UI / 2D・3Dゲーム");
	SetupSampleLabel(*Title.Get(), "Heading", 40);
	m_Start2D = CreateChild<DUiButton>("2Dゲームを開始");
	SetupSampleButton(*m_Start2D.Get(), "Start2D");
	m_Start3D = CreateChild<DUiButton>("3Dゲームを開始");
	SetupSampleButton(*m_Start3D.Get(), "Start3D");
	m_Settings = CreateChild<DUiButton>("設定");
	SetupSampleButton(*m_Settings.Get(), "Settings");
	m_Quit = CreateChild<DUiButton>("終了");
	SetupSampleButton(*m_Quit.Get(), "Quit");
}

void DUiTitleScreen::OnAttach()
{
	const auto State = m_pState;
	GetAttachScope().Add(m_Start2D.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::Start2D;
	    }));
	GetAttachScope().Add(m_Start3D.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::Start3D;
	    }));
	GetAttachScope().Add(m_Settings.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::Settings;
	    }));
	GetAttachScope().Add(m_Quit.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::Quit;
	    }));
}
} // namespace Dxf::UiSample
