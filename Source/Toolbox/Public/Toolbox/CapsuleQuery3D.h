// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CAPSULE_QUERY_3D_H
#define TOOLBOX_CAPSULE_QUERY_3D_H
#include "Toolbox/Capsule.h"
#include "Toolbox/Optional.h"
#include "Toolbox/ShapeSweepHit3D.h"
namespace Toolbox
{
/**
 * 有限線分とカプセルの最初の交点の割合（0〜1）。始点が内部なら0。交わらなければ空。不正な値は例外。
 * 胴体（円柱面）と両端の球の最初の交点の最小値。
 * @param Start 線分の始点。
 * @param End 線分の終点。
 * @param Capsule 対象のカプセル。
 */
TOptional<f64> IntersectSegment(FVector3 Start, FVector3 End, const FCapsule& Capsule);
/**
 * 球をEndCenterまで直線移動したときに、カプセルへ最初に接触する時刻（0〜1）。開始時の接触・重なりはTime=0・bInitialContact。
 * 法線は対象から移動する球の中心へ向く（区別できない場合は空）。
 * @param Moving 開始時の球。
 * @param EndCenter 終点の中心。
 * @param Target 対象のカプセル。
 */
TOptional<FShapeSweepHit3D> SweepToCenter(const FSphere& Moving, FVector3 EndCenter, const FCapsule& Target);
/**
 * カプセルを中心線の中点がEndCenterになるまで平行移動したときに、対象へ最初に接触する時刻。
 * 距離の下限を使う保守的な前進（最大64回）で求め、接触の前で止まる（貫通させない）。開始時の接触はTime=0・bInitialContact。
 * @param Moving 開始時のカプセル。
 * @param EndCenter 終点の中心線の中点。
 * @param Target 対象の球。
 */
TOptional<FShapeSweepHit3D> SweepToCenter(const FCapsule& Moving, FVector3 EndCenter, const FSphere& Target);
/**
 * カプセルの平行移動と箱の最初の接触（規則は球の対象と同じ）。
 * @param Moving 開始時のカプセル。
 * @param EndCenter 終点の中心線の中点。
 * @param Target 対象の箱。
 */
TOptional<FShapeSweepHit3D> SweepToCenter(const FCapsule& Moving, FVector3 EndCenter, const FOBB& Target);
/**
 * カプセルの平行移動とカプセルの最初の接触（規則は球の対象と同じ）。
 * @param Moving 開始時のカプセル。
 * @param EndCenter 終点の中心線の中点。
 * @param Target 対象のカプセル。
 */
TOptional<FShapeSweepHit3D> SweepToCenter(const FCapsule& Moving, FVector3 EndCenter, const FCapsule& Target);
} // namespace Toolbox
#endif
