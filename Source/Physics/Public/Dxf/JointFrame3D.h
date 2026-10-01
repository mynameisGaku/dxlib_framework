// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_JOINTFRAME3D_H
#define DXF_PHYSICS_JOINTFRAME3D_H
#include "Toolbox/Vector3.h"
#include "Toolbox/Quaternion.h"
namespace Dxf
{
/**
 * Body重心基準の取付位置と方向。3DのZは回転軸、Xは移動軸。
 */
struct FJointFrame3D
{
	/**
	 * 重心から取付位置へのLocal変位。
	 */
	Toolbox::FVector3 LocalAnchor;
	/**
	 * Body姿勢へ後から合成するLocal方向。
	 */
	Toolbox::FQuaternion LocalRotation;
};
} // namespace Dxf
#endif
