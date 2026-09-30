// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_DISTANCEJOINTCOMPONENTDESCRIPTION3D_H
#define DXF_GAMEPLAY_DISTANCEJOINTCOMPONENTDESCRIPTION3D_H
#include "Dxf/PhysicsBodyReference3D.h"
#include "Dxf/DistanceJoint3D.h"
namespace Dxf
{
/**
 * 両端の非所有参照と、既存World APIへ渡す距離拘束設定。
 */
struct FDistanceJointComponentDescription3D
{
	/**
	 * A側の接続先。
	 */
	FPhysicsBodyReference3D BodyA;
	/**
	 * B側の接続先。
	 */
	FPhysicsBodyReference3D BodyB;
	/**
	 * 維持する距離と重心からのAnchor。
	 */
	FDistanceJointDescription3D Joint;
};
} // namespace Dxf
#endif
