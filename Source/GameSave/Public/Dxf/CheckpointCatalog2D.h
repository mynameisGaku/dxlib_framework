// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_CATALOG_2D_H
#define DXF_CHECKPOINT_CATALOG_2D_H
#include "Dxf/SaveDocument.h"
#include "Dxf/SceneDefinition2D.h"
#include "Dxf/CheckpointPrefabRecipe2D.h"
namespace Dxf
{
/**
 * ゲームが所有する許可カタログ。保存データが任意のSceneやPrefabを選べないようにする。
 */
struct FCheckpointCatalog2D
{
	/**
	 * 期待するゲーム・Content・Scene・次元の識別。Payloadは空Object。
	 */
	FSaveDocument Identity;
	/**
	 * 現在信頼する初期Scene。資源所有や実行時IDを持たない。
	 */
	FSceneDefinition2D Scene;
	/**
	 * 初期SceneのInstances順に対応する許可Prefabキー。保存側の配列位置を使わない。
	 */
	Toolbox::TVector<Toolbox::FString> InitialPrefabKeys;
	/**
	 * 動的生成も含め、明示的に許可したCPUレシピ。
	 */
	Toolbox::TVector<FCheckpointPrefabRecipe2D> Prefabs;
};
} // namespace Dxf
#endif
