// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SETTINGS_PANEL_H
#define DXF_UI_SETTINGS_PANEL_H
#include "UiSampleState.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiSlider.h"
#include "Dxf/UiToggle.h"
namespace Dxf::UiSample
{
/**
 * 音量・拡縮・分割表示の共通画面。表示反映と操作通知を分離する。
 */
class DUiSettingsPanel final : public DUiPanel
{
public:
	explicit DUiSettingsPanel(Toolbox::TSharedPtr<FUiSampleState> State);

protected:
	void OnFirstAttach() override;
	void OnAttach() override;

private:
	Toolbox::TSharedPtr<FUiSampleState> m_pState;
	TUiRef<DUiSlider> m_Volume;
	TUiRef<DUiSlider> m_Scale;
	TUiRef<DUiToggle> m_Split;
	TUiRef<DUiButton> m_Sound;
	TUiRef<DUiButton> m_Reload;
	TUiRef<DUiButton> m_Close;
};
} // namespace Dxf::UiSample
#endif
