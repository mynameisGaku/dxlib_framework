// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_PLAY_2D_SCENE_H
#define DXF_UI_PLAY_2D_SCENE_H
#include "UiSampleShell.h"
#include "../GameplaySample/SampleCharacters.h"
#include "Dxf/PhysicsScene2D.h"
namespace Dxf::UiSample
{
/**
 * 2Dゲーム側からUIを接続する例。物理更新は既存PhysicsSceneだけが行う。
 */
class DUiPlay2DScene final : public DPhysicsScene2D
{
public:
	explicit DUiPlay2DScene(Toolbox::TSharedPtr<FUiSampleState> State);
	FUiSampleShell& GetUi() noexcept
	{
		return m_Ui;
	}

	GameplaySample::DPlayer2D& GetPlayer() const
	{
		return *m_Player.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext& Context) override;
	void OnDraw(FRenderContext& Render) const override;

private:
	TObjectHandle<GameplaySample::DPlayer2D> m_Player;
	mutable FUiSampleShell m_Ui;
};
} // namespace Dxf::UiSample
#endif
