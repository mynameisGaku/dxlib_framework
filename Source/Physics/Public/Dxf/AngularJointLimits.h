// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_ANGULARJOINTLIMITS_H
#define DXF_PHYSICS_ANGULARJOINTLIMITS_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * rad単位の有限な上下限。角度は折返しを跨がない主値区間。
 */
struct FAngularJointLimits
{
	/**
	 * 制限を使用する。
	 */
	bool bEnabled = false;
	/**
	 * 下限。等値は一座標ロック。
	 */
	Toolbox::f64 LowerAngle = -1;
	/**
	 * 上限。下限以上の有限値。
	 */
	Toolbox::f64 UpperAngle = 1;
};
} // namespace Dxf
#endif
