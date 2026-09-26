// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_UI_WINDOW_SCENE_H
#define DXF_TEST_UI_WINDOW_SCENE_H
#include "Dxf/Scene.h"
#include "Dxf/UiAssetTextService.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiSceneHost.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf::UiSmoke
{
/**
 * ウィンドウの拡縮・最小化を確かめるScene。全面のボタン（描画先の寸法に追従）、実画像、文字を画面へ出す。
 */
class DWindowScene final : public DScene
{
public:
	/**
	 * 全面のボタンの色。
	 */
	static constexpr FColor ButtonColor{30, 160, 90, 255};
	/**
	 * 実画像を描く位置（64x64、等倍）。
	 */
	static constexpr FUiPixelRect ImageRect{10, 10, 74, 74};
	/**
	 * 更新・描画の回数、決定の回数、最後に受け取ったウィンドウの状態。
	 */
	Toolbox::int32 Ticks = 0;
	mutable Toolbox::int32 Draws = 0;
	Toolbox::int32 Clicks = 0;
	FWindowState Window;

protected:
	/**
	 * ルートと表示先を作る。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * 更新の回数とウィンドウの状態を記録する。
	 */
	void OnTick(const FTickContext& Context) override;
	/**
	 * UIを描く。
	 */
	void OnDraw(FRenderContext& Render) const override;

private:
	/**
	 * 文字の窓口とルート。
	 */
	Toolbox::TUniquePtr<FUiAssetTextService> m_pText;
	Toolbox::TUniquePtr<FUiRoot> m_pRoot;
	/**
	 * 仲介（ルートより先に破棄する）。
	 */
	mutable FUiSceneHost m_Host;
	/**
	 * 決定の購読。
	 */
	FUiSubscription m_Click;
};
} // namespace Dxf::UiSmoke
#endif
