// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_TITLE_SCENE_H
#define DXF_UI_TITLE_SCENE_H
#include "UiSampleShell.h"
namespace Dxf::UiSample
{
/**
 * ゲームを開始するScene。UI部品の定義は別ファイル。
 */
class DUiTitleScene final : public DScene
{
public:
	explicit DUiTitleScene(Toolbox::TSharedPtr<FUiSampleState> State);
	FUiSampleShell& GetUi() noexcept
	{
		return m_Ui;
	}

protected:
	TResult<void> OnInitialize(const FInitContext& Context) override;
	void OnDraw(FRenderContext& Render) const override;

private:
	mutable FUiSampleShell m_Ui;
};
} // namespace Dxf::UiSample
#endif
