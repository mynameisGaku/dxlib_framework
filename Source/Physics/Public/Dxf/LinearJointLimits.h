// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_LINEARJOINTLIMITS_H
#define DXF_PHYSICS_LINEARJOINTLIMITS_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * m単位の有限な上下限。角度は折返しを跨がない主値区間。
 */
struct FLinearJointLimits
{
	/**
	 * 制限を使用する。
	 */
	bool bEnabled = false;
	/**
	 * 下限。等値は一座標ロック。
	 */
	Toolbox::f64 LowerTranslation = -1;
	/**
	 * 上限。下限以上の有限値。
	 */
	Toolbox::f64 UpperTranslation = 1;
};
} // namespace Dxf
#endif
