// SPDX-License-Identifier: NOASSERTION
#include "UiTitleScene.h"
namespace Dxf::UiSample
{
DUiTitleScene::DUiTitleScene(Toolbox::TSharedPtr<FUiSampleState> State) : m_Ui(Toolbox::Move(State))
{
}

TResult<void> DUiTitleScene::OnInitialize(const FInitContext& Context)
{
	return m_Ui.Initialize(*this, Context.Assets, true, false);
}

void DUiTitleScene::OnDraw(FRenderContext& Render) const
{
	auto Result = m_Ui.Draw(Render);
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
} // namespace Dxf::UiSample
