// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_FIXEDJOINTSTATE2D_H
#define DXF_PHYSICS_FIXEDJOINTSTATE2D_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 現在の物理Poseと速度から計算する読み取り値。Stepと起床を行わない。
 */
struct FFixedJointState2D
{
	/**
	 * Anchorの一致誤差の長さ（m）。直動では横ずれ。
	 */
	Toolbox::f64 AnchorError = 0;
	/**
	 * 相対姿勢の最短回転誤差（rad）。
	 */
	Toolbox::f64 OrientationError = 0;
};
} // namespace Dxf
#endif
