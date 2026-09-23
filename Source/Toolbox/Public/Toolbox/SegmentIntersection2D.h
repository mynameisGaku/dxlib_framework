// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SEGMENT_INTERSECTION_2D_H
#define TOOLBOX_SEGMENT_INTERSECTION_2D_H
#include "Toolbox/Collision2D.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/Optional.h"
namespace Toolbox
{
/**
 * 円と平面上の有限線分の最初の交点を、始点0～終点1の割合で返す。内部・境界上の始点は0、非交差は空。
 * 半径0は点として扱う。非有限入力、不正形状、f32で表現できない変位はFException。既存の円Sweepを利用する。
 * 3D版（SegmentIntersection.h）とは別のヘッダーにし、3D版の利用側へ2Dの型を持ち込まない。
 * @param Start 線分の始点。 @param End 線分の終点。 @param Circle 対象の円。
 */
TOptional<f64> IntersectSegment(FVector2 Start, FVector2 End, const FCircle2D& Circle);
/**
 * 回転矩形と平面上の有限線分の最初の交点を、始点0～終点1の割合で返す。内部・境界上の始点は0、非交差は空。
 * 半幅0の軸は辺・点として接触を含む。不正形状・非有限入力はFException。外接矩形では代用しない。
 * @param Start 線分の始点。 @param End 線分の終点。 @param Box 対象の回転矩形（Angleは反時計回りのラジアン）。
 */
TOptional<f64> IntersectSegment(FVector2 Start, FVector2 End, const FOrientedBox2D& Box);
} // namespace Toolbox
#endif
