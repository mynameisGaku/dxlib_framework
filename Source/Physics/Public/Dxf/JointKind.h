// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_JOINTKIND_H
#define DXF_PHYSICS_JOINTKIND_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 共通Joint IDが所有する拘束の種類。
 */
enum class EJointKind : Toolbox::uint8
{
	/**
	 * Anchor間距離を保持する。
	 */
	Distance,
	/**
	 * 一軸回転を残す。
	 */
	Revolute,
	/**
	 * 相対位置と姿勢を保持する。
	 */
	Fixed,
	/**
	 * 一軸移動を残す。
	 */
	Prismatic
};
} // namespace Dxf
#endif
