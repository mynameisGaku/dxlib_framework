// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_PREFAB_RECIPE_2D_H
#define DXF_CHECKPOINT_PREFAB_RECIPE_2D_H
#include "Dxf/PrefabDefinition2D.h"
#include "Toolbox/Function.h"
namespace Dxf
{
/**
 * ゲームが許可するPrefabとCPU再構築処理。保存ファイルに生成関数やパスを渡させない。
 */
struct FCheckpointPrefabRecipe2D
{
	/**
	 * 保存で使う許可キー。実ファイルのパスとは別。
	 */
	Toolbox::FString Key;
	/**
	 * 準備時の不変定義。ContentFingerprintは実読込バイトから得た値。
	 */
	FPrefabDefinition2D Definition;
	/**
	 * 公開値が異なるときだけ呼ぶCPU処理。World・Native・保存先を変更しない。
	 * 返す定義の指紋・公開値を再検証する。空なら準備済みの公開値だけを許可する。
	 */
	Toolbox::TFunction<FPrefabDefinition2D(const Toolbox::TVector<FContentParameterValue>&)> Resolve;
};
} // namespace Dxf
#endif
