// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHAPE_CONTACT_QUERY_3D_H
#define TOOLBOX_SHAPE_CONTACT_QUERY_3D_H
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/ShapeContact3D.h"
namespace Toolbox
{
/**
 * 球と静止した球の現在位置での符号付き距離と分離方向を求める（f64）。
 * 中心間の距離が、中心差と半径の和の大きさの2^-30倍以下（同心とみなせる）なら法線は空。
 * 無効な形状はFException。
 * @param Shape 調べる球（半径0は点）。
 * @param Target 静止した対象の球。
 */
FShapeContact3D FindShapeContact(const FSphere& Shape, const FSphere& Target);
/**
 * 球と静止したOBBの現在位置での符号付き距離と分離方向を求める（f64）。
 * OBBは格納した実際の軸（IntersectsSphere・SweepToCenterと同じ平行六面体）で扱い、軸を正規化・直交化しない。
 * 球の中心が内部（または丸め誤差の範囲で境界上）なら、最も近い面の外向き法線とその面までの距離を使う。
 * 同じ距離の面が複数ある場合は法線を空にし、距離だけを返す。無効な形状はFException。
 * FindContact（軸を単位化する既存の接触計算）とは形状の扱いが異なる。
 * @param Shape 調べる球（半径0は点）。
 * @param Target 静止した対象のOBB（半幅0は面・辺・点）。
 */
FShapeContact3D FindShapeContact(const FSphere& Shape, const FOBB& Target);
} // namespace Toolbox
#endif
