// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_LINEARJOINTDRIVE_H
#define DXF_PHYSICS_LINEARJOINTDRIVE_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * m/sとN単位の速度目標と有限な最大努力。速度を直接代入しない。
 */
struct FLinearJointDrive
{
	/**
	 * 有効か。目標速度0は有限のブレーキ。
	 */
	bool bEnabled = false;
	/**
	 * 有限な目標速度。
	 */
	Toolbox::f64 TargetSpeed = 0;
	/**
	 * 0以上の有限な最大努力。0は駆動しない。
	 */
	Toolbox::f64 MaxForce = 0;
};
} // namespace Dxf
#endif
