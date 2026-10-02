// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_RECIPES_H
#define DXF_SAMPLE_SAVE_COURSE_RECIPES_H
#include "Dxf/CheckpointRestore.h"
#include "Dxf/SceneContentSource.h"
namespace Dxf::GameplaySample
{
/**
 * 保存用コースの許可構成と純粋な再開変換。ゲーム規則を保存コアへ持ち込まない。
 */
class FSaveCourseRecipes
{
public:
	/**
	 * @param Source 起動時に確定したProjectRootのCPU読込窓口。
	 * 読込済み定義だけをレシピへ捕捉し、再開変換中にディスクを再読込しない。
	 */
	static FCheckpointCatalog2D Load2D(const FSceneContentSource& Source);
	/**
	 * @param Source 同じRootと意味で使う3DのCPU読込窓口。
	 */
	static FCheckpointCatalog3D Load3D(const FSceneContentSource& Source);
	/**
	 * @param Catalog 信頼する2D初期構成。開扉の既定値は初期進行の解錠・開閉にも反映する。
	 */
	static FCheckpointDocument2D NewGame(const FCheckpointCatalog2D& Catalog);
	/**
	 * @param Catalog 同じ意味の3D初期構成。取得済み・得点・生成番号は新しく始める。
	 */
	static FCheckpointDocument3D NewGame(const FCheckpointCatalog3D& Catalog);
	/**
	 * @param Document codecとカタログ検証を通す2D論理状態。
	 * 取得物の除去、安全な再開位置、扉の初期開閉を独立した候補へ反映する。
	 */
	static FCheckpointDocument2D Restore(const FCheckpointDocument2D& Document);
	/**
	 * @param Document 同じゲーム進行規則を持つ3D論理状態。
	 */
	static FCheckpointDocument3D Restore(const FCheckpointDocument3D& Document);
};
} // namespace Dxf::GameplaySample
#endif
