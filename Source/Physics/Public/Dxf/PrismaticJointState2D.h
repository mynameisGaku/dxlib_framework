// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_PRISMATICJOINTSTATE2D_H
#define DXF_PHYSICS_PRISMATICJOINTSTATE2D_H
#include "Toolbox/Utility.h"
#include "Dxf/LinearJointDrive.h"
#include "Dxf/JointLimitState.h"
namespace Dxf
{
/**
 * 現在の物理Poseと速度から計算する読み取り値。Stepと起床を行わない。
 */
struct FPrismaticJointState2D
{
	/**
	 * Anchorの一致誤差の長さ（m）。直動では横ずれ。
	 */
	Toolbox::f64 AnchorError = 0;
	/**
	 * 相対姿勢の最短回転誤差（rad）。
	 */
	Toolbox::f64 OrientationError = 0;
	/**
	 * Frame AのX軸への相対変位（m）。
	 */
	Toolbox::f64 Translation = 0;
	/**
	 * 回転する軸の微分を含む移動速度（m/s）。
	 */
	Toolbox::f64 TranslationRate = 0;
	/**
	 * 現在の座標とLimit設定から得る状態。
	 */
	EJointLimitState LimitState = EJointLimitState::Disabled;
	/**
	 * 受理済みのDrive。求解Impulseではない。
	 */
	FLinearJointDrive Drive;
};
} // namespace Dxf
#endif
