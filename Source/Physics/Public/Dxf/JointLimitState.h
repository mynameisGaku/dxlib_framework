// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_JOINTLIMITSTATE_H
#define DXF_PHYSICS_JOINTLIMITSTATE_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 現在座標と受理済みLimit設定から判別する状態。
 */
enum class EJointLimitState : Toolbox::uint8
{
	/**
	 * Limitを使わない。
	 */
	Disabled,
	/**
	 * 両端の内側。
	 */
	Inside,
	/**
	 * 下限側。
	 */
	Lower,
	/**
	 * 上限側。
	 */
	Upper,
	/**
	 * 等しい上下限で座標を固定する。
	 */
	Locked
};
} // namespace Dxf
#endif
