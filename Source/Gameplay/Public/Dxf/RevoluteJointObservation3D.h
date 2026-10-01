// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_REVOLUTEJOINTOBSERVATION3D_H
#define DXF_GAMEPLAY_REVOLUTEJOINTOBSERVATION3D_H
#include "Dxf/JointId3D.h"
#include "Dxf/RevoluteJointState3D.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 成功したPostPhysicsの値コピー。描画補間値やWorld全体の番号とは区別する。
 */
struct FRevoluteJointObservation3D
{
	/**
	 * 採取時点の世代付きJoint ID。
	 */
	FJointId3D Joint;
	/**
	 * このComponentで成功PostPhysicsへ到達した通し番号。
	 */
	Toolbox::uint64 SuccessfulStep = 0;
	/**
	 * その成功時点の物理Poseから計算した状態。
	 */
	FRevoluteJointState3D State;
	/**
	 * A側の物理Anchor。
	 */
	Toolbox::FVector3 AnchorA;
	/**
	 * B側の物理Anchor。
	 */
	Toolbox::FVector3 AnchorB;
};
} // namespace Dxf
#endif
