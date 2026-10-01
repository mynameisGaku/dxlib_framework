// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_REVOLUTEJOINTSTATE3D_H
#define DXF_PHYSICS_REVOLUTEJOINTSTATE3D_H
#include "Toolbox/Utility.h"
#include "Dxf/AngularJointDrive.h"
#include "Dxf/JointLimitState.h"
namespace Dxf
{
/**
 * 現在の物理Poseと速度から計算する読み取り値。Stepと起床を行わない。
 */
struct FRevoluteJointState3D
{
	/**
	 * Anchorの一致誤差の長さ（m）。直動では横ずれ。
	 */
	Toolbox::f64 AnchorError = 0;
	/**
	 * 3Dでは二つの回転軸の傾き（rad）。2Dは0。
	 */
	Toolbox::f64 AxisAlignmentError = 0;
	/**
	 * Frame間の主値角(-pi,pi]。累積回転数ではない。
	 */
	Toolbox::f64 Angle = 0;
	/**
	 * 主値角の瞬間変化速度（rad/s）。
	 */
	Toolbox::f64 AngularSpeed = 0;
	/**
	 * 現在の座標とLimit設定から得る状態。
	 */
	EJointLimitState LimitState = EJointLimitState::Disabled;
	/**
	 * 受理済みのDrive。求解Impulseではない。
	 */
	FAngularJointDrive Drive;
};
} // namespace Dxf
#endif
