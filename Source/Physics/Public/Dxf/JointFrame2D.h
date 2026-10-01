// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_JOINTFRAME2D_H
#define DXF_PHYSICS_JOINTFRAME2D_H
#include "Toolbox/Vector2.h"

namespace Dxf
{
/**
 * Body重心基準の取付位置と方向。3DのZは回転軸、Xは移動軸。
 */
struct FJointFrame2D
{
	/**
	 * 重心から取付位置へのLocal変位。
	 */
	Toolbox::FVector2 LocalAnchor;
	/**
	 * Body姿勢へ後から合成するLocal方向。
	 */
	Toolbox::f64 LocalAngle = 0;
};
} // namespace Dxf
#endif
