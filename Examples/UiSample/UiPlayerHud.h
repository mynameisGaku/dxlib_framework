// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_PLAYER_HUD_H
#define DXF_UI_PLAYER_HUD_H
#include "UiSampleState.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiButton.h"
namespace Dxf::UiSample
{
/**
 * ゲームの採取済み状態を一方向に表示するHUD。
 */
class DUiPlayerHud final : public DUiPanel
{
public:
	explicit DUiPlayerHud(Toolbox::TSharedPtr<FUiSampleState> State, bool bInteractive = true);

protected:
	void OnFirstAttach() override;
	void OnAttach() override;

private:
	Toolbox::TSharedPtr<FUiSampleState> m_pState;
	bool m_bInteractive;
	TUiRef<DUiLabel> m_Status;
	TUiRef<DUiLabel> m_Notice;
	TUiRef<DUiButton> m_Pause;
	TUiRef<DUiButton> m_Settings;
};
} // namespace Dxf::UiSample
#endif
