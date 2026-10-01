// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_SCENEDEFINITION2D_H
#define DXF_CONTENT_SCENEDEFINITION2D_H
#include "Dxf/PrefabDefinition2D.h"
#include "Dxf/PrefabSpawnOptions2D.h"
#include "Dxf/ContentView2D.h"
#include "Toolbox/Array.h"
#include "Toolbox/FixedStepScheduler.h"
#include "Dxf/ContentSceneConnection2D.h"
namespace Dxf
{
/**
 * 参照展開済みSceneの初期構成。実行状態のセーブデータではない。
 */
struct FSceneDefinition2D
{
	/**
	 * 診断用のSceneパス。
	 */
	Toolbox::FString Path;
	/**
	 * CPU読込で確定したProjectRoot。CWDへ切り替えない。
	 */
	Toolbox::FString ProjectRoot;
	/**
	 * 既存Schedulerへ渡す固定時間と追いつき処理の有限上限。
	 */
	Toolbox::FFixedStepSettings FixedUpdate;
	/**
	 * 別Instanceの公開先を通した接続。Prefabファイルの循環参照とは別。
	 */
	Toolbox::TVector<FContentSceneConnection2D> Connections;
	/**
	 * Scene全体から型付きで参照できる共通資源。Native所有ではない。
	 */
	Toolbox::TVector<FContentAssetDefinition> Assets;
	/**
	 * 表示する一つか二つのView。更新回数を変えない。
	 */
	Toolbox::TArray<FContentView2D, 2> Views;
	/**
	 * 有効なViewの数。
	 */
	Toolbox::uint32 ViewCount = 1;

	/**
	 * World座標の重力。Prefab配置で回転しない。
	 */
	Toolbox::FVector2 Gravity{};
	/**
	 * Prefabインスタンスの配列順。
	 */
	Toolbox::TVector<FPrefabDefinition2D> Prefabs;
	/**
	 * 各Prefabの配置変換。
	 */
	Toolbox::TVector<FPrefabSpawnOptions2D> Placements;
	/**
	 * 各Prefabの論理ID。
	 */
	Toolbox::TVector<Toolbox::FString> Instances;
};
} // namespace Dxf
#endif
