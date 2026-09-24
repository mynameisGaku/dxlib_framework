// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHAPE_SWEEP_2D_H
#define TOOLBOX_SHAPE_SWEEP_2D_H
#include "Toolbox/Collision2D.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/ShapeSweepHit2D.h"
namespace Toolbox
{
/**
 * 円を現在の中心からEndCenterまで直線移動させ、静止した円へ最初に接触する割合を返す。非接触は空。
 * 許容距離は0（接触を含む）。開始時の接触・重なりはTime=0、bInitialContact=true。開始＝終点は静止した重なりの判定。
 * 中心差と移動量はf64で求める。無効な形状・非有限値、各成分がf32で表現できない移動はFException。
 * 接触法線は対象から移動する円の中心へ向く単位方向。f64の相対位置・接触時刻から求め、f32の結果から引き直さない。
 * 初期接触・移動する円の半径0では空。最終残差の最大成分が、残差の計算に使った相対位置（回転矩形では移動量・半幅も）と
 * 半径の最大成分の2^-30倍以下の場合も、方向を丸め誤差から区別できないとして空にする（ヒットと割合は返す）。
 * @param Moving 移動する円（開始時の中心と半径。半径0は点）。
 * @param EndCenter 終点の中心。変位ではない。
 * @param Target 静止した対象の円。
 */
TOptional<FShapeSweepHit2D> SweepToCenter(const FCircle2D& Moving, FVector2 EndCenter, const FCircle2D& Target);
/**
 * 円を直線移動させ、静止した回転矩形へ最初に接触する割合を返す。非接触は空。
 * 矩形の4辺・4頂点それぞれの有限範囲への距離から求め、半径分だけ膨らませた外接矩形では代用しない。
 * 角度はContact2Dと同じ反時計回り。半幅0の軸は辺・点として扱う。その他の契約は円同士と同じ。
 * 法線は接触時刻の辺・頂点上の最近点から円の中心への方向で、辺・頂点で区別する。
 * @param Moving 移動する円。
 * @param EndCenter 終点の中心。
 * @param Target 静止した対象の回転矩形。
 */
TOptional<FShapeSweepHit2D> SweepToCenter(const FCircle2D& Moving, FVector2 EndCenter, const FOrientedBox2D& Target);
} // namespace Toolbox
#endif
