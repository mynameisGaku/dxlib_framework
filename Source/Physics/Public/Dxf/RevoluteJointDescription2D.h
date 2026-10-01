// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_REVOLUTEJOINTDESCRIPTION2D_H
#define DXF_PHYSICS_REVOLUTEJOINTDESCRIPTION2D_H
#include "Dxf/JointFrame2D.h"
#include "Dxf/AngularJointLimits.h"
#include "Dxf/AngularJointDrive.h"
namespace Dxf
{
/**
 * Revoluteの生成条件。BodyはWorld APIへ別に渡す。
 */
struct FRevoluteJointDescription2D
{
	/**
	 * Body A側の取付Frame。
	 */
	FJointFrame2D FrameA;
	/**
	 * Body B側の取付Frame。
	 */
	FJointFrame2D FrameB;
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
