// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHAPE_CONTACT_QUERY_2D_H
#define TOOLBOX_SHAPE_CONTACT_QUERY_2D_H
#include "Toolbox/Collision2D.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/ShapeContact2D.h"
namespace Toolbox
{
/**
 * 円と静止した円の現在位置での符号付き距離と分離方向を求める（f64）。
 * 中心間の距離が、中心差と半径の和の大きさの2^-30倍以下（同心とみなせる）なら法線は空。
 * 無効な形状はFException。
 * @param Shape 調べる円（半径0は点）。
 * @param Target 静止した対象の円。
 */
FShapeContact2D FindShapeContact(const FCircle2D& Shape, const FCircle2D& Target);
/**
 * 円と静止した回転矩形の現在位置での符号付き距離と分離方向を求める（f64）。
 * 矩形はSweepToCenter・Intersectsと同じ反時計回りの角度規約の軸で扱い、辺・頂点への最近点から求める。
 * 円の中心が矩形の内部（または丸め誤差の範囲で境界上）なら、最も近い辺の外向き法線とその辺までの距離を使う。
 * 同じ距離の辺が複数ある場合（中心・角の二等分線上、半幅0の辺の上など）は法線を空にし、距離だけを返す。
 * 無効な形状はFException。
 * @param Shape 調べる円（半径0は点）。
 * @param Target 静止した対象の回転矩形（半幅0は辺・点）。
 */
FShapeContact2D FindShapeContact(const FCircle2D& Shape, const FOrientedBox2D& Target);
} // namespace Toolbox
#endif
