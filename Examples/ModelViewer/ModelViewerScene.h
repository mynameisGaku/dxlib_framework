// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MODEL_VIEWER_SCENE_H
#define DXF_MODEL_VIEWER_SCENE_H
#include "Dxf/GameScene.h"
#include "Toolbox/CollisionShapes.h"
#include "Dxf/Font.h"
#include "Dxf/Model.h"
#include "Dxf/RenderView3D.h"
namespace Dxf::ModelViewer
{
/**
 * .fbxを事前変換なしで読み、同じモデルデータの2体を独立に再生する確認用Scene。
 * P: 球・箱の選択例、V: 分割、O: 正射影、左クリック: 選択。
 * 1: 左の一時停止／再開、2: 右の速度切替、3: 右のクリップ切替、R: 再読込、Enter: 新しいSceneへ切替、Esc: 終了。
 */
class AModelViewerScene final : public DGameScene
{
public:
	/**
	 * @param Generation 何番目に作られたSceneか（切替の確認用）。
	 */
	explicit AModelViewerScene(Toolbox::uint32 Generation = 1) : m_Generation(Generation)
	{
	}

protected:
	TResult<void> OnInitialize(const FInitContext& Context) override;
	void OnTick(const FTickContext& Context) override;
	void OnDraw(FRenderContext& Render) const override;
	void OnDeinitialize() noexcept override;

private:
	/**
	 * モデルを読み込み、インスタンスを配置する。
	 */
	TResult<void> Load_Internal();
	/**
	 * 読み込みに使うAssetService。Sceneより長く生存する。
	 */
	FAssetService* m_pAssets = nullptr;
	FModel m_Column;
	FModel m_Box;
	FModelInstance m_Left;
	FModelInstance m_Right;
	FModelInstance m_BoxInstance;
	FFont m_Font;
	FRenderView3D m_View;
	/**
	 * 入力と描画が共有する、現在フレームのカメラ値。
	 */
	Toolbox::TArray<FRenderView3D, 2> m_DrawViews;
	/**
	 * 選択例で表示する球そのもの。
	 */
	Toolbox::FSphere m_PickSphere{{-100, 100, 0}, 45};
	/**
	 * 選択例で表示する箱そのもの。
	 */
	Toolbox::FOBB m_PickBox{{100, 100, 0}, {45, 45, 45}};
	/**
	 * 選択形状の番号。-1は非選択。モデルやWorldへの参照を持たない。
	 */
	Toolbox::int32 m_Selected = -1;
	/**
	 * 球・箱の選択例を表示するか。
	 */
	bool m_bPicking = false;
	/**
	 * 現在の表示設定からカメラ値を一度だけ準備する。
	 */
	void PrepareViews_Internal();
	Toolbox::uint32 m_Generation;
	Toolbox::uint32 m_Reloads = 0;
	Toolbox::f64 m_BoxAngle = 0.0;
	/**
	 * Vキーで左右のカメラ表示を切り替える。
	 */
	bool m_bSplit = false;
};
}
#endif
