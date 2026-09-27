// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CAPSULE_CONTACT_2D_H
#define TOOLBOX_CAPSULE_CONTACT_2D_H
#include "Toolbox/Capsule2D.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/ShapeContact2D.h"
namespace Toolbox
{
/**
 * カプセルが関わる組の最大の接触点の数（中心線の両端と、最も近い点）。
 */
constexpr uint32 MaxCapsuleContacts2D = 3;
/**
 * カプセルと円の接触（中心線上の最も近い点の円として判定）。法線は二つ目から一つ目へ向く。不正な値は例外。
 * @param A カプセル。
 * @param B 円。
 */
FContactPoint2D FindContact(const FCapsule2D& A, const FCircle2D& B);
/**
 * 円とカプセルの接触。法線は二つ目から一つ目へ向く。不正な値は例外。
 * @param A 円。
 * @param B カプセル。
 */
FContactPoint2D FindContact(const FCircle2D& A, const FCapsule2D& B);
/**
 * カプセルと矩形の最も深い（近い）接触。中心線の全体（胴体を含む）について矩形までの符号付き距離が最小の点で判定する。
 * @param A カプセル。
 * @param B 矩形。
 */
FContactPoint2D FindContact(const FCapsule2D& A, const FOrientedBox2D& B);
/**
 * カプセル同士の接触（中心線分同士の最も近い点の円として判定）。
 * @param A 一つ目のカプセル。
 * @param B 二つ目のカプセル。
 */
FContactPoint2D FindContact(const FCapsule2D& A, const FCapsule2D& B);
/**
 * カプセルと矩形の接触点の集まり（中心線の両端と最も近い点のうち、表面間の距離がMargin以下の点。重なる点は一つにまとめる）。
 * 横たわったカプセルが面の上で転がらないよう、両端の点も返す。法線は矩形からカプセルへ向く。FeatureIdは0（最も近い点）・1（Start）・2（End）。
 * @param A カプセル。
 * @param B 矩形。
 * @param Margin 含める最大の表面間の距離（有限・非負）。
 * @param Out 接触点の書込み先。
 * @return 書き込んだ数（0〜MaxCapsuleContacts2D）。
 */
uint32 FindCapsuleContacts(const FCapsule2D& A, const FOrientedBox2D& B, f32 Margin,
                           FContactPoint2D (&Out)[MaxCapsuleContacts2D]);
/**
 * カプセル同士の接触点の集まり（最も近い点の組と、Aの両端から最も近いBの点）。規則はカプセルと矩形と同じ。法線はBからAへ向く。
 * @param A 一つ目のカプセル。
 * @param B 二つ目のカプセル。
 * @param Margin 含める最大の表面間の距離（有限・非負）。
 * @param Out 接触点の書込み先。
 * @return 書き込んだ数（0〜MaxCapsuleContacts2D）。
 */
uint32 FindCapsuleContacts(const FCapsule2D& A, const FCapsule2D& B, f32 Margin,
                           FContactPoint2D (&Out)[MaxCapsuleContacts2D]);
/**
 * 円と対象のカプセルの符号付き距離と、対象から円へ向く法線（方向を区別できない場合は空）。
 * @param Shape 調べる円。
 * @param Target 対象のカプセル。
 */
FShapeContact2D FindShapeContact(const FCircle2D& Shape, const FCapsule2D& Target);
/**
 * カプセルと対象の円の符号付き距離と法線（対象からカプセルへ向く）。
 * @param Shape 調べるカプセル。
 * @param Target 対象の円。
 */
FShapeContact2D FindShapeContact(const FCapsule2D& Shape, const FCircle2D& Target);
/**
 * カプセルと対象の矩形の符号付き距離と法線（中心線の全体で矩形までの距離が最小の点で判定する）。
 * @param Shape 調べるカプセル。
 * @param Target 対象の矩形。
 */
FShapeContact2D FindShapeContact(const FCapsule2D& Shape, const FOrientedBox2D& Target);
/**
 * カプセル同士の符号付き距離と法線（中心線分同士の最も近い点で判定する）。
 * 重なっている場合の値は中心線の距離から半径の和を引いた値で、最小の押し出し距離とは限らない。
 * @param Shape 調べるカプセル。
 * @param Target 対象のカプセル。
 */
FShapeContact2D FindShapeContact(const FCapsule2D& Shape, const FCapsule2D& Target);
} // namespace Toolbox
#endif
