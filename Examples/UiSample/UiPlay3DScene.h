// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_PLAY_3D_SCENE_H
#define DXF_UI_PLAY_3D_SCENE_H
#include "UiSampleShell.h"
#include "../GameplaySample/SampleCharacters.h"
#include "Dxf/PhysicsScene3D.h"
namespace Dxf::UiSample
{
/**
 * 3Dゲーム側からUIを接続する例。物理更新は既存PhysicsSceneだけが行う。
 */
class DUiPlay3DScene final : public DPhysicsScene3D
{
public:
	explicit DUiPlay3DScene(Toolbox::TSharedPtr<FUiSampleState> State);
	FUiSampleShell& GetUi() noexcept
	{
		return m_Ui;
	}

	GameplaySample::DPlayer3D& GetPlayer() const
	{
		return *m_Player.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext& Context) override;
	void OnDraw(FRenderContext& Render) const override;

private:
	TObjectHandle<GameplaySample::DPlayer3D> m_Player;
	mutable FUiSampleShell m_Ui;
};
} // namespace Dxf::UiSample
#endif
