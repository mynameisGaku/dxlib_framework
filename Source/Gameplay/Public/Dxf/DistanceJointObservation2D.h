// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_DISTANCEJOINTOBSERVATION2D_H
#define DXF_GAMEPLAY_DISTANCEJOINTOBSERVATION2D_H
#include "Dxf/JointId2D.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * このComponentで成功した物理Stepの拘束状態。補間座標ではない。
 */
struct FDistanceJointObservation2D
{
	/**
	 * 採取時の完全な世代付き拘束ID。
	 */
	FJointId2D Joint;
	/**
	 * Componentの開始から成功PostPhysicsが到達した通し番号。
	 */
	Toolbox::uint64 SuccessfulStep = 0;
	/**
	 * 指定した接続長。
	 */
	Toolbox::f64 TargetLength = 0;
	/**
	 * 成功したStepの実測長。
	 */
	Toolbox::f64 CurrentLength = 0;
	/**
	 * 実測長から指定長を引いた差。
	 */
	Toolbox::f64 Error = 0;
	/**
	 * A側の物理姿勢から計算したAnchor。
	 */
	Toolbox::FVector2 AnchorA;
	/**
	 * B側の物理姿勢から計算したAnchor。
	 */
	Toolbox::FVector2 AnchorB;
};
} // namespace Dxf
#endif
