// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SEGMENT_INTERSECTION_H
#define TOOLBOX_SEGMENT_INTERSECTION_H
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/Optional.h"
namespace Toolbox
{
/**
 * 球と有限線分の最初の交点を、始点0～終点1の割合で返す。内部始点は0、非交差は空。
 * 非有限入力、不正形状、f32で表現できない変位はFException。既存の球Sweepを利用する。
 * @param Start 線分の始点。 @param End 線分の終点。 @param Sphere 選択する球。
 */
TOptional<f64> IntersectSegment(FVector3 Start, FVector3 End, const FSphere& Sphere);
/**
 * 任意向きの箱と有限線分の最初の交点を、始点0～終点1の割合で返す。内部始点は0、非交差は空。
 * 不正形状・非有限入力・局所座標への変換不能はFException。厚さ0も接触を含む。
 * @param Start 線分の始点。 @param End 線分の終点。 @param Box 選択する箱。
 */
TOptional<f64> IntersectSegment(FVector3 Start, FVector3 End, const FOBB& Box);
} // namespace Toolbox
#endif
