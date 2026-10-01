// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_FIXEDJOINTCOMPONENTDESCRIPTION3D_H
#define DXF_GAMEPLAY_FIXEDJOINTCOMPONENTDESCRIPTION3D_H
#include "Dxf/PhysicsBodyReference3D.h"
#include "Dxf/FixedJointDescription3D.h"
namespace Dxf
{
/**
 * 両端の非所有参照と、重心基準の取付Frame・種類別設定。
 */
struct FFixedJointComponentDescription3D
{
	/**
	 * A側の型付き接続先。
	 */
	FPhysicsBodyReference3D BodyA;
	/**
	 * B側の型付き接続先。
	 */
	FPhysicsBodyReference3D BodyB;
	/**
	 * 同じWorld APIへ渡す接続・駆動設定。
	 */
	FFixedJointDescription3D Joint;
};
} // namespace Dxf
#endif
