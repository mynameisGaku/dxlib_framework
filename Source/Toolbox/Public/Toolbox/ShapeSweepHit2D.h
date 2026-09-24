// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHAPE_SWEEP_HIT_2D_H
#define TOOLBOX_SHAPE_SWEEP_HIT_2D_H
#include "Toolbox/Optional.h"
#include "Toolbox/ShapeSweepHit.h"
#include "Toolbox/Vector2.h"
namespace Toolbox
{
/**
 * 円を終点の中心まで直線移動させたときの最初の接触と、取得できた場合の接触法線。
 * 割合・初期接触の意味は共通部分のFShapeSweepHitと同じ。
 */
struct FShapeSweepHit2D : FShapeSweepHit
{
	/**
	 * 接触対象から移動する円の中心へ向く、ワールド座標の無次元の単位方向。移動量・距離・押し戻し量ではない。
	 * 初期接触、移動する円の半径0、有効な方向を丸め誤差から区別できない場合は空（ヒット自体は成立している）。
	 */
	TOptional<FVector2> Normal;
};
} // namespace Toolbox
#endif
