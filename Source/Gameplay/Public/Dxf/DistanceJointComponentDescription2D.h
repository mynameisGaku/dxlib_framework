// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_DISTANCEJOINTCOMPONENTDESCRIPTION2D_H
#define DXF_GAMEPLAY_DISTANCEJOINTCOMPONENTDESCRIPTION2D_H
#include "Dxf/PhysicsBodyReference2D.h"
#include "Dxf/DistanceJoint2D.h"
namespace Dxf
{
/**
 * 両端の非所有参照と、既存World APIへ渡す距離拘束設定。
 */
struct FDistanceJointComponentDescription2D
{
	/**
	 * A側の接続先。
	 */
	FPhysicsBodyReference2D BodyA;
	/**
	 * B側の接続先。
	 */
	FPhysicsBodyReference2D BodyB;
	/**
	 * 維持する距離と重心からのAnchor。
	 */
	FDistanceJointDescription2D Joint;
};
} // namespace Dxf
#endif
