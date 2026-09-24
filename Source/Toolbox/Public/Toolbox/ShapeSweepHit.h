// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHAPE_SWEEP_HIT_H
#define TOOLBOX_SHAPE_SWEEP_HIT_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 円／球を終点の中心まで直線移動させたときの最初の接触。法線・接触点は持たない。
 */
struct FShapeSweepHit
{
	/**
	 * 移動区間全体を1とした最初の接触割合（0～1）。秒数・距離ではない。
	 */
	f64 Time = 0;
	/**
	 * 開始状態で接触または重なりがあったか。trueならTimeは0。
	 */
	bool bInitialContact = false;
};
} // namespace Toolbox
#endif
