// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_REVOLUTEJOINTDESCRIPTION3D_H
#define DXF_PHYSICS_REVOLUTEJOINTDESCRIPTION3D_H
#include "Dxf/JointFrame3D.h"
#include "Dxf/AngularJointLimits.h"
#include "Dxf/AngularJointDrive.h"
namespace Dxf
{
/**
 * Revoluteの生成条件。BodyはWorld APIへ別に渡す。
 */
struct FRevoluteJointDescription3D
{
	/**
	 * Body A側の取付Frame。
	 */
	FJointFrame3D FrameA;
	/**
	 * Body B側の取付Frame。
	 */
	FJointFrame3D FrameB;
	/**
	 * 自由座標のLimit。
	 */
	FAngularJointLimits Limits;
	/**
	 * 自由座標の速度Motor。
	 */
	FAngularJointDrive Drive;
};
} // namespace Dxf
#endif
