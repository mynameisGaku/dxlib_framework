// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CAPSULE_QUERY_2D_H
#define TOOLBOX_CAPSULE_QUERY_2D_H
#include "Toolbox/Capsule2D.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/ShapeSweepHit2D.h"
namespace Toolbox
{
/**
 * 有限線分とカプセルの最初の交点の割合（0〜1）。始点が内部なら0。交わらなければ空。不正な値は例外。
 * 胴体（平行な二辺）と両端の円の最初の交点の最小値。
 * @param Start 線分の始点。
 * @param End 線分の終点。
 * @param Capsule 対象のカプセル。
 */
TOptional<f64> IntersectSegment(FVector2 Start, FVector2 End, const FCapsule2D& Capsule);
/**
 * 円をEndCenterまで直線移動したときに、カプセルへ最初に接触する時刻（0〜1）。開始時の接触・重なりはTime=0・bInitialContact。
 * 法線は対象から移動する円の中心へ向く（区別できない場合は空）。
 * @param Moving 開始時の円。
 * @param EndCenter 終点の中心。
 * @param Target 対象のカプセル。
 */
TOptional<FShapeSweepHit2D> SweepToCenter(const FCircle2D& Moving, FVector2 EndCenter, const FCapsule2D& Target);
/**
 * カプセルを中心線の中点がEndCenterになるまで平行移動したときに、対象へ最初に接触する時刻。
 * 距離の下限を使う保守的な前進（最大64回）で求め、接触の前で止まる（貫通させない）。開始時の接触はTime=0・bInitialContact。
 * @param Moving 開始時のカプセル。
 * @param EndCenter 終点の中心線の中点。
 * @param Target 対象の円。
 */
TOptional<FShapeSweepHit2D> SweepToCenter(const FCapsule2D& Moving, FVector2 EndCenter, const FCircle2D& Target);
/**
 * カプセルの平行移動と矩形の最初の接触（規則は円の対象と同じ）。
 * @param Moving 開始時のカプセル。
 * @param EndCenter 終点の中心線の中点。
 * @param Target 対象の矩形。
 */
TOptional<FShapeSweepHit2D> SweepToCenter(const FCapsule2D& Moving, FVector2 EndCenter, const FOrientedBox2D& Target);
/**
 * カプセルの平行移動とカプセルの最初の接触（規則は円の対象と同じ）。
 * @param Moving 開始時のカプセル。
 * @param EndCenter 終点の中心線の中点。
 * @param Target 対象のカプセル。
 */
TOptional<FShapeSweepHit2D> SweepToCenter(const FCapsule2D& Moving, FVector2 EndCenter, const FCapsule2D& Target);
} // namespace Toolbox
#endif
