// SPDX-License-Identifier: NOASSERTION
#include "UiWorldControls.h"
#include "UiSampleWidgets.h"
#include "Dxf/UiBindProperty.h"
namespace Dxf::UiSample
{
DUiWorldControls::DUiWorldControls(Toolbox::TSharedPtr<FUiSampleState> State)
    : DUiPanel(EUiStackMode::Vertical, 18), m_pState(Toolbox::Move(State))
{
	SetPadding(FUiThickness::All(24));
}

void DUiWorldControls::OnFirstAttach()
{
	auto Title = CreateChild<DUiLabel>("WORLD PANEL");
	SetupSampleLabel(*Title.Get(), "WorldTitle", 50);
	m_Split = CreateChild<DUiToggle>("2画面表示");
	SetupSampleButton(*m_Split.Get(), "WorldSplit");
	m_Settings = CreateChild<DUiButton>("設定を開く");
	SetupSampleButton(*m_Settings.Get(), "WorldSettings");
}

void DUiWorldControls::OnAttach()
{
	const auto State = m_pState;
	BindUiProperty(m_Split, State->Split,
	               [](DUiToggle& View, const bool& Value)
	               {
		               View.SetValue(Value);
	               });
	GetAttachScope().Add(m_Split.Get()->OnValueChanged().Subscribe(
	    [State](bool Value)
	    {
		    State->Split.Set(Value);
	    }));
	GetAttachScope().Add(m_Settings.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::Settings;
	    }));
}
} // namespace Dxf::UiSample
