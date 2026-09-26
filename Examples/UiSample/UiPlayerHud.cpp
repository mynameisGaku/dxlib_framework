// SPDX-License-Identifier: NOASSERTION
#include "UiPlayerHud.h"
#include "UiSampleWidgets.h"
#include "Dxf/UiBindProperty.h"
namespace Dxf::UiSample
{
DUiPlayerHud::DUiPlayerHud(Toolbox::TSharedPtr<FUiSampleState> State, bool bInteractive)
    : DUiPanel(EUiStackMode::Vertical, 6), m_pState(Toolbox::Move(State)), m_bInteractive(bInteractive)
{
	SetName("Hud");
	SetWidth(FUiLength::Fixed(440));
	SetHeight(FUiLength::Content());
	SetAlign(EUiAlign::Start, EUiAlign::Start);
	SetPadding(FUiThickness::All(10));
	if (!bInteractive)
	{
		SetHitTest(EUiHitTest::None);
	}
}

void DUiPlayerHud::OnFirstAttach()
{
	m_Status = CreateChild<DUiLabel>();
	SetupSampleLabel(*m_Status.Get(), "Status", 30);
	m_Notice = CreateChild<DUiLabel>();
	SetupSampleLabel(*m_Notice.Get(), "Notice", 30);
	if (m_bInteractive)
	{
		m_Pause = CreateChild<DUiButton>("一時停止 / ESC");
		SetupSampleButton(*m_Pause.Get(), "Pause");
		m_Settings = CreateChild<DUiButton>("設定");
		SetupSampleButton(*m_Settings.Get(), "Settings");
	}
}

void DUiPlayerHud::OnAttach()
{
	const auto State = m_pState;
	BindUiProperty(m_Status, State->Status,
	               [](DUiLabel& View, const Toolbox::FString& Text)
	               {
		               View.SetText(Text);
	               });
	BindUiProperty(m_Notice, State->Notice,
	               [](DUiLabel& View, const Toolbox::FString& Text)
	               {
		               View.SetText(Text);
	               });
	if (m_bInteractive)
	{
		GetAttachScope().Add(m_Pause.Get()->OnClicked().Subscribe(
		    [State]()
		    {
			    State->Action = EUiSampleAction::Pause;
		    }));
		GetAttachScope().Add(m_Settings.Get()->OnClicked().Subscribe(
		    [State]()
		    {
			    State->Action = EUiSampleAction::Settings;
		    }));
	}
}
} // namespace Dxf::UiSample
