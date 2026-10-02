// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_LIMITS_H
#define DXF_CHECKPOINT_LIMITS_H
#include "Dxf/SaveLimits.h"
namespace Dxf
{
/**
 * 論理チェックポイントの確保前上限。Content展開時の上限も別途適用する。
 */
struct FCheckpointLimits
{
	/**
	 * 封筒を含むJSONの有限上限。
	 */
	FSaveLimits Document;
	/**
	 * 存在しない個体の記録も含む個体数。
	 */
	Toolbox::uint32 MaxInstances = 4096;
	/**
	 * 種類別の論理接続・操作状態の件数。
	 */
	Toolbox::uint32 MaxJoints = 16384;
	/**
	 * 一個体の公開パラメーター数。
	 */
	Toolbox::uint32 MaxParameters = 128;
	/**
	 * ゲームが許可するPrefabレシピの件数。
	 */
	Toolbox::uint32 MaxCatalogEntries = 128;
};
} // namespace Dxf
#endif
