// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_SLIDE_MOVE_3D_H
#define DXF_WORLD_SLIDE_MOVE_3D_H
#include "Dxf/RigidBody3D.h"
#include "Dxf/WorldSlideResult3D.h"
namespace Dxf
{
/**
 * 球を希望終点へ動かす候補を、SweepClosestの組合せで計算する（読み取り専用）。Worldは変更しない。
 * 最初の接触では経路に沿ってBackoffDistanceだけ手前へ戻し、有効な法線があれば残りの移動から
 * 法線の内向き成分だけを除いて1回だけ滑らせる。滑り経路で接触したら手前で止め、2回目の補正はしない。
 * 手前へ戻した候補はf32へ丸めた後に同じ条件で再スイープし、接触すればその区間を採用しない。
 * SweepClosestは最大4回（最初の移動・その再検査・滑り経路・その再検査）。開始時の接触では開始中心のまま返し、
 * 離脱・押し出し・接地・多面の反復は行わない。BackoffDistanceは経路上の後退距離で、対象表面との
 * 全方向の最小すき間を保証しない。自己除外とFilterはすべての問い合わせで同じものを使う。
 * 入力・World状態・計算の異常はToolbox::FException（途中の結果は返さない）。同じWorldの変更・Stepとは、
 * この関数全体の間、呼出し側で直列化する。
 * @param World 問い合わせるWorld。
 * @param StartShape 開始時の球。半径は有限の正の値（点の滑りは提供しない）。3D物理ワールド座標。
 * @param DesiredEndCenter 希望する終点の中心。変位・速度・Bodyの重心位置ではない。
 * @param BackoffDistance 接触位置から移動経路に沿って戻す距離（物理ワールドの距離単位、有限の正の値）。
 * @param ExcludedBody 任意の自己Body。除外しない場合は空Optional。
 * @param Filter 対象にする問い合わせカテゴリ。既定は全ビット。
 */
FWorldSlideResult3D ComputeSlideMove(const FPhysicsWorld3D& World, const Toolbox::FSphere& StartShape,
                                     Toolbox::FVector3 DesiredEndCenter, Toolbox::f64 BackoffDistance,
                                     Toolbox::TOptional<FBodyId3D> ExcludedBody = {},
                                     const FWorldQueryFilter& Filter = {});
} // namespace Dxf
#endif
