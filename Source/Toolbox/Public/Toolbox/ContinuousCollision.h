// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CONTINUOUS_COLLISION_H
#define TOOLBOX_CONTINUOUS_COLLISION_H
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/Collision2D.h"
#include "Toolbox/SweepHit.h"
namespace Toolbox
{
/**
 * 二球が直線移動する区間で最初の接触を求める。速度ではなく区間全体の変位を渡す。
 * 入力は有限値、半径とToleranceは非負とする。不正入力はFException。
 * @param A 一つ目の球の開始形状。
 * @param DisplacementA 一つ目の球の区間全体の変位。
 * @param B 二つ目の球の開始形状。
 * @param DisplacementB 二つ目の球の区間全体の変位。
 * @param Tolerance 半径の和へ加える許容距離。
 */
FSweepHit3D Sweep(const FSphere& A, FVector3 DisplacementA, const FSphere& B, FVector3 DisplacementB,
                  f32 Tolerance = 0);
/**
 * 球と軸平行箱が平行移動する区間で最初の接触を求める。
 * 丸い辺・角を距離式で扱い、単純なAABB膨張による角の誤検出を避ける。回転は扱わない。
 * @param Sphere 球の開始形状。
 * @param DisplacementSphere 球の変位。
 * @param Box 箱の開始形状。
 * @param DisplacementBox 箱の変位。
 * @param Tolerance 球の半径へ加える有限・非負の許容距離。
 */
FSweepHit3D Sweep(const FSphere& Sphere, FVector3 DisplacementSphere, const FAABB& Box, FVector3 DisplacementBox,
                  f32 Tolerance = 0);
/**
 * 箱と球の順序を反転した線形スイープ。法線も反転する。
 * @param Box 箱の開始形状。
 * @param DisplacementBox 箱の変位。
 * @param Sphere 球の開始形状。
 * @param DisplacementSphere 球の変位。
 * @param Tolerance 球の半径へ加える許容距離。
 */
FSweepHit3D Sweep(const FAABB& Box, FVector3 DisplacementBox, const FSphere& Sphere, FVector3 DisplacementSphere,
                  f32 Tolerance = 0);
/**
 * 円同士の線形スイープ。3Dと同じ数値処理をXY平面で使用する。
 * @param A 一つ目の円。
 * @param DisplacementA 一つ目の変位。
 * @param B 二つ目の円。
 * @param DisplacementB 二つ目の変位。
 * @param Tolerance 半径の和へ加える許容距離。
 */
FSweepHit2D Sweep(const FCircle2D& A, FVector2 DisplacementA, const FCircle2D& B, FVector2 DisplacementB,
                  f32 Tolerance = 0);
/**
 * 円と軸平行矩形の線形スイープ。
 * @param Circle 円の開始形状。
 * @param DisplacementCircle 円の変位。
 * @param Box 矩形の開始形状。
 * @param DisplacementBox 矩形の変位。
 * @param Tolerance 円の半径へ加える許容距離。
 */
FSweepHit2D Sweep(const FCircle2D& Circle, FVector2 DisplacementCircle, const FAABB2D& Box, FVector2 DisplacementBox,
                  f32 Tolerance = 0);
/**
 * 矩形と円の順序を反転した線形スイープ。
 * @param Box 矩形の開始形状。
 * @param DisplacementBox 矩形の変位。
 * @param Circle 円の開始形状。
 * @param DisplacementCircle 円の変位。
 * @param Tolerance 円の半径へ加える許容距離。
 */
FSweepHit2D Sweep(const FAABB2D& Box, FVector2 DisplacementBox, const FCircle2D& Circle, FVector2 DisplacementCircle,
                  f32 Tolerance = 0);
} // namespace Toolbox
#endif
