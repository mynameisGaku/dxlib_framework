// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_WORLD_CONTROLS_H
#define DXF_UI_WORLD_CONTROLS_H
#include "UiSampleState.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiToggle.h"
namespace Dxf::UiSample
{
/**
 * 同じ部品を2D世界と3D平面へ置く操作例。
 */
class DUiWorldControls final : public DUiPanel
{
public:
	explicit DUiWorldControls(Toolbox::TSharedPtr<FUiSampleState> State);

protected:
	void OnFirstAttach() override;
	void OnAttach() override;

private:
	Toolbox::TSharedPtr<FUiSampleState> m_pState;
	TUiRef<DUiToggle> m_Split;
	TUiRef<DUiButton> m_Settings;
};
} // namespace Dxf::UiSample
#endif
