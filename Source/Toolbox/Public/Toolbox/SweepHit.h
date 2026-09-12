// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SWEEP_HIT_H
#define TOOLBOX_SWEEP_HIT_H
#include "Toolbox/Vector2.h"
#include "Toolbox/Vector3.h"
namespace Toolbox
{
/**
 * 線形移動区間で最初に接触する時刻。衝突応答や貫通解消は行わない。
 */
template <typename TVectorType> struct TSweepHit
{
	/**
	 * 移動区間[0, 1]内で接触したか。Timeが1だけでは終端接触と非交差を区別できない。
	 */
	bool bHit = false;
	/**
	 * 区間の初期状態が、Toleranceを含めた接触状態だったか。
	 */
	bool bInitialContact = false;
	/**
	 * 変位全体を1とした最初の接触時刻。秒数ではない。
	 */
	f64 Time = 1;
	/**
	 * 二つ目の形状から一つ目へ向く単位法線。
	 * 初期貫通・同一点・半径ゼロでは法線は一意でないため決定的な代表方向を返す。
	 */
	TVectorType Normal;
};
/**
 * 三次元の接触結果。
 */
using FSweepHit3D = TSweepHit<FVector3>;
/**
 * 二次元の接触結果。
 */
using FSweepHit2D = TSweepHit<FVector2>;
} // namespace Toolbox
#endif
