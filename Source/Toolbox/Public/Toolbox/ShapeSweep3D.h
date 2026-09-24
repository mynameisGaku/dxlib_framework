// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHAPE_SWEEP_3D_H
#define TOOLBOX_SHAPE_SWEEP_3D_H
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/Optional.h"
#include "Toolbox/ShapeSweepHit3D.h"
namespace Toolbox
{
/**
 * 球を現在の中心からEndCenterまで直線移動させ、静止した球へ最初に接触する割合を返す。非接触は空。
 * 許容距離は0（接触を含む）。開始時の接触・重なりはTime=0、bInitialContact=true。開始＝終点は静止した重なりの判定。
 * 中心差と移動量はf64で求める。無効な形状・非有限値、各成分がf32で表現できない移動はFException。
 * 接触法線は対象から移動する球の中心へ向く単位方向。f64の相対位置・接触時刻から求め、f32の結果から引き直さない。
 * 初期接触・移動する球の半径0では空。最終残差の最大成分が、残差の計算に使った相対位置（OBBでは移動量・半幅も）と
 * 半径の最大成分の2^-30倍以下の場合も、方向を丸め誤差から区別できないとして空にする（ヒットと割合は返す）。
 * @param Moving 移動する球（開始時の中心と半径。半径0は点）。
 * @param EndCenter 終点の中心。変位ではない。
 * @param Target 静止した対象の球。
 */
TOptional<FShapeSweepHit3D> SweepToCenter(const FSphere& Moving, FVector3 EndCenter, const FSphere& Target);
/**
 * 球を直線移動させ、静止したOBBへ最初に接触する割合を返す。非接触は空。
 * OBBの6面・12辺・8頂点それぞれの有限範囲への距離から求める。軸は正規化・直交化せず、
 * 丸めを含む実際の軸（IntersectsSphereの距離計算と同じ平行六面体）を使う。半幅0の軸は面・辺・点として扱う。
 * 法線はその平行六面体の面・辺・頂点上の最近点から球の中心への方向。軸を正規化・直交化した別形状の法線ではない。
 * @param Moving 移動する球。
 * @param EndCenter 終点の中心。
 * @param Target 静止した対象のOBB。
 */
TOptional<FShapeSweepHit3D> SweepToCenter(const FSphere& Moving, FVector3 EndCenter, const FOBB& Target);
} // namespace Toolbox
#endif
