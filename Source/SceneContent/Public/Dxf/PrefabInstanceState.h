// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PREFAB_INSTANCE_STATE_H
#define DXF_PREFAB_INSTANCE_STATE_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 生成受付と実際の利用可能を区別する状態。実行時の例外を握りつぶさない。
 */
enum class EPrefabInstanceState : Toolbox::uint8
{
	/**
	 * 親とComponentの初期化を待つ。
	 */
	PendingInitialization,
	/**
	 * 固定更新による登録と成功観察を待つ。
	 */
	PendingPhysics,
	/**
	 * 必須構成と成功PostPhysics観察がそろった。
	 */
	Ready,
	/**
	 * 必須Bodyなどの外部破棄を検出した。
	 */
	EndpointLost,
	/**
	 * 自身の破棄要求によりhandleが失効する。
	 */
	Destroyed
};
} // namespace Dxf
#endif
