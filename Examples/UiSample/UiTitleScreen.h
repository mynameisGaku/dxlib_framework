// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UITITLESCREEN_H
#define DXF_UITITLESCREEN_H
#include "UiSampleState.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiButton.h"
namespace Dxf::UiSample
{
/**
 * タイトルの組合せ。ゲーム開始・設定・終了を操作要求として返す。
 */
class DUiTitleScreen final : public DUiPanel
{
public:
	explicit DUiTitleScreen(Toolbox::TSharedPtr<FUiSampleState> State);

protected:
	void OnFirstAttach() override;
	void OnAttach() override;

private:
	Toolbox::TSharedPtr<FUiSampleState> m_pState;
	TUiRef<DUiButton> m_Start2D;
	TUiRef<DUiButton> m_Start3D;
	TUiRef<DUiButton> m_Settings;
	TUiRef<DUiButton> m_Quit;
};
} // namespace Dxf::UiSample
#endif
