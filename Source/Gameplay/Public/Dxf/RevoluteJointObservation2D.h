// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_REVOLUTEJOINTOBSERVATION2D_H
#define DXF_GAMEPLAY_REVOLUTEJOINTOBSERVATION2D_H
#include "Dxf/JointId2D.h"
#include "Dxf/RevoluteJointState2D.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * 成功したPostPhysicsの値コピー。描画補間値やWorld全体の番号とは区別する。
 */
struct FRevoluteJointObservation2D
{
	/**
	 * 採取時点の世代付きJoint ID。
	 */
	FJointId2D Joint;
	/**
	 * このComponentで成功PostPhysicsへ到達した通し番号。
	 */
	Toolbox::uint64 SuccessfulStep = 0;
	/**
	 * その成功時点の物理Poseから計算した状態。
	 */
	FRevoluteJointState2D State;
	/**
	 * A側の物理Anchor。
	 */
	Toolbox::FVector2 AnchorA;
	/**
	 * B側の物理Anchor。
	 */
	Toolbox::FVector2 AnchorB;
};
} // namespace Dxf
#endif
