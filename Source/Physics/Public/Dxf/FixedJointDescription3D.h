// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_FIXEDJOINTDESCRIPTION3D_H
#define DXF_PHYSICS_FIXEDJOINTDESCRIPTION3D_H
#include "Dxf/JointFrame3D.h"
namespace Dxf
{
/**
 * Fixedの生成条件。BodyはWorld APIへ別に渡す。
 */
struct FFixedJointDescription3D
{
	/**
	 * Body A側の取付Frame。
	 */
	FJointFrame3D FrameA;
	/**
	 * Body B側の取付Frame。
	 */
	FJointFrame3D FrameB;
};
} // namespace Dxf
#endif
