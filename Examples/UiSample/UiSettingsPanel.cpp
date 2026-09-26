// SPDX-License-Identifier: NOASSERTION
#include "UiSettingsPanel.h"
#include "UiSampleWidgets.h"
#include "Dxf/UiBindProperty.h"
namespace Dxf::UiSample
{
DUiSettingsPanel::DUiSettingsPanel(Toolbox::TSharedPtr<FUiSampleState> State)
    : DUiPanel(EUiStackMode::Vertical, 12), m_pState(Toolbox::Move(State))
{
	SetWidth(FUiLength::Fixed(460));
	SetHeight(FUiLength::Content());
	SetPadding(FUiThickness::All(20));
	SetAlign(EUiAlign::Center, EUiAlign::Center);
}

void DUiSettingsPanel::OnFirstAttach()
{
	auto Title = CreateChild<DUiLabel>("設定 / Settings");
	SetupSampleLabel(*Title.Get(), "Heading", 40);
	auto Volume = CreateChild<DUiLabel>("音量（試聴ボタンへ反映）");
	SetupSampleLabel(*Volume.Get(), "VolumeLabel");
	m_Volume = CreateChild<DUiSlider>();
	m_Volume.Get()->SetRange(0, 1, 0.05);
	m_Volume.Get()->SetName("Volume");
	auto Scale = CreateChild<DUiLabel>("UI倍率 0.5〜1.25");
	SetupSampleLabel(*Scale.Get(), "ScaleLabel");
	m_Scale = CreateChild<DUiSlider>();
	m_Scale.Get()->SetRange(0.5, 1.25, 0.05);
	m_Scale.Get()->SetName("Scale");
	m_Split = CreateChild<DUiToggle>("2画面表示");
	SetupSampleButton(*m_Split.Get(), "Split");
	m_Sound = CreateChild<DUiButton>("音を再生");
	SetupSampleButton(*m_Sound.Get(), "Sound");
	m_Reload = CreateChild<DUiButton>("スタイルを再読込");
	SetupSampleButton(*m_Reload.Get(), "ReloadStyle");
	m_Close = CreateChild<DUiButton>("閉じる");
	SetupSampleButton(*m_Close.Get(), "CloseSettings");
}

void DUiSettingsPanel::OnAttach()
{
	const auto State = m_pState;
	BindUiProperty(m_Volume, State->Volume,
	               [](DUiSlider& View, const Toolbox::f64& Value)
	               {
		               View.SetValue(Value);
	               });
	BindUiProperty(m_Scale, State->Scale,
	               [](DUiSlider& View, const Toolbox::f64& Value)
	               {
		               View.SetValue(Value);
	               });
	BindUiProperty(m_Split, State->Split,
	               [](DUiToggle& View, const bool& Value)
	               {
		               View.SetValue(Value);
	               });
	GetAttachScope().Add(m_Volume.Get()->OnValueChanged().Subscribe(
	    [State](Toolbox::f64 Value)
	    {
		    State->Volume.Set(Value);
	    }));
	GetAttachScope().Add(m_Scale.Get()->OnValueChanged().Subscribe(
	    [State](Toolbox::f64 Value)
	    {
		    State->Scale.Set(Value);
	    }));
	GetAttachScope().Add(m_Split.Get()->OnValueChanged().Subscribe(
	    [State](bool Value)
	    {
		    State->Split.Set(Value);
	    }));
	GetAttachScope().Add(m_Sound.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::PlaySound;
	    }));
	GetAttachScope().Add(m_Reload.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::ReloadStyle;
	    }));
	GetAttachScope().Add(m_Close.Get()->OnClicked().Subscribe(
	    [State]()
	    {
		    State->Action = EUiSampleAction::CloseSettings;
	    }));
}
} // namespace Dxf::UiSample
