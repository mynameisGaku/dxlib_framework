// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHAPE_SWEEP_2D_H
#define TOOLBOX_SHAPE_SWEEP_2D_H
#include "Toolbox/Collision2D.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/ShapeSweepHit.h"
namespace Toolbox
{
/**
 * 円を現在の中心からEndCenterまで直線移動させ、静止した円へ最初に接触する割合を返す。非接触は空。
 * 許容距離は0（接触を含む）。開始時の接触・重なりはTime=0、bInitialContact=true。開始＝終点は静止した重なりの判定。
 * 中心差と移動量はf64で求める。無効な形状・非有限値、各成分がf32で表現できない移動はFException。
 * @param Moving 移動する円（開始時の中心と半径。半径0は点）。
 * @param EndCenter 終点の中心。変位ではない。
 * @param Target 静止した対象の円。
 */
TOptional<FShapeSweepHit> SweepToCenter(const FCircle2D& Moving, FVector2 EndCenter, const FCircle2D& Target);
/**
 * 円を直線移動させ、静止した回転矩形へ最初に接触する割合を返す。非接触は空。
 * 矩形の4辺・4頂点それぞれの有限範囲への距離から求め、半径分だけ膨らませた外接矩形では代用しない。
 * 角度はContact2Dと同じ反時計回り。半幅0の軸は辺・点として扱う。その他の契約は円同士と同じ。
 * @param Moving 移動する円。
 * @param EndCenter 終点の中心。
 * @param Target 静止した対象の回転矩形。
 */
TOptional<FShapeSweepHit> SweepToCenter(const FCircle2D& Moving, FVector2 EndCenter, const FOrientedBox2D& Target);
} // namespace Toolbox
#endif
