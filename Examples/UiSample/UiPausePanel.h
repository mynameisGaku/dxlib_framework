// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UIPAUSEPANEL_H
#define DXF_UIPAUSEPANEL_H
#include "UiSampleState.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiButton.h"
namespace Dxf::UiSample
{
/**
 * 一時停止画面の組合せ。所有と停止時間の判断はサンプルのShellに置く。
 */
class DUiPausePanel final : public DUiPanel
{
public:
	explicit DUiPausePanel(Toolbox::TSharedPtr<FUiSampleState> State);

protected:
	void OnFirstAttach() override;
	void OnAttach() override;

private:
	Toolbox::TSharedPtr<FUiSampleState> m_pState;
	TUiRef<DUiButton> m_Resume;
	TUiRef<DUiButton> m_Settings;
	TUiRef<DUiButton> m_Title;
};
} // namespace Dxf::UiSample
#endif
