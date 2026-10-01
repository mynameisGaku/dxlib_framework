// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_PRISMATICJOINTCOMPONENTDESCRIPTION2D_H
#define DXF_GAMEPLAY_PRISMATICJOINTCOMPONENTDESCRIPTION2D_H
#include "Dxf/PhysicsBodyReference2D.h"
#include "Dxf/PrismaticJointDescription2D.h"
namespace Dxf
{
/**
 * 両端の非所有参照と、重心基準の取付Frame・種類別設定。
 */
struct FPrismaticJointComponentDescription2D
{
	/**
	 * A側の型付き接続先。
	 */
	FPhysicsBodyReference2D BodyA;
	/**
	 * B側の型付き接続先。
	 */
	FPhysicsBodyReference2D BodyB;
	/**
	 * 同じWorld APIへ渡す接続・駆動設定。
	 */
	FPrismaticJointDescription2D Joint;
};
} // namespace Dxf
#endif
