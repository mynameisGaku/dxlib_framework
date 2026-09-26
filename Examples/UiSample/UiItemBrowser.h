// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_ITEM_BROWSER_H
#define DXF_UI_ITEM_BROWSER_H
#include "UiSampleState.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiListView.h"
namespace Dxf::UiSample
{
/**
 * 仮想化一覧と詳細のサンプル。ゲームのインベントリ実装ではない。
 */
class DUiItemBrowser final : public DUiPanel
{
public:
	explicit DUiItemBrowser(Toolbox::TSharedPtr<FUiSampleState> State);
	Toolbox::size_t GetVisibleRowCount() const noexcept;

protected:
	void OnFirstAttach() override;
	void OnAttach() override;

private:
	Toolbox::TSharedPtr<FUiSampleState> m_pState;
	TUiRef<DUiListView> m_List;
	TUiRef<DUiLabel> m_Detail;
};
} // namespace Dxf::UiSample
#endif
