// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_FIXEDJOINTDESCRIPTION2D_H
#define DXF_PHYSICS_FIXEDJOINTDESCRIPTION2D_H
#include "Dxf/JointFrame2D.h"
namespace Dxf
{
/**
 * Fixedの生成条件。BodyはWorld APIへ別に渡す。
 */
struct FFixedJointDescription2D
{
	/**
	 * Body A側の取付Frame。
	 */
	FJointFrame2D FrameA;
	/**
	 * Body B側の取付Frame。
	 */
	FJointFrame2D FrameB;
};
} // namespace Dxf
#endif
