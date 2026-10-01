// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_PRISMATICJOINTDESCRIPTION3D_H
#define DXF_PHYSICS_PRISMATICJOINTDESCRIPTION3D_H
#include "Dxf/JointFrame3D.h"
#include "Dxf/LinearJointLimits.h"
#include "Dxf/LinearJointDrive.h"
namespace Dxf
{
/**
 * Prismaticの生成条件。BodyはWorld APIへ別に渡す。
 */
struct FPrismaticJointDescription3D
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
	FLinearJointLimits Limits;
	/**
	 * 自由座標の速度Motor。
	 */
	FLinearJointDrive Drive;
};
} // namespace Dxf
#endif
