// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CAPSULE_CONTACT_3D_H
#define TOOLBOX_CAPSULE_CONTACT_3D_H
#include "Toolbox/Capsule.h"
#include "Toolbox/Contact3D.h"
#include "Toolbox/ShapeContact3D.h"
namespace Toolbox
{
/**
 * カプセルが関わる組の最大の接触点の数（中心線の両端と、最も近い点）。
 */
constexpr uint32 MaxCapsuleContacts = 3;
/**
 * カプセルと球の接触（中心線上の最も近い点の球として判定）。法線は二つ目から一つ目へ向く。不正な値は例外。
 * @param A カプセル。
 * @param B 球。
 */
FContactPoint3D FindContact(const FCapsule& A, const FSphere& B);
/**
 * 球とカプセルの接触。法線は二つ目から一つ目へ向く。不正な値は例外。
 * @param A 球。
 * @param B カプセル。
 */
FContactPoint3D FindContact(const FSphere& A, const FCapsule& B);
/**
 * カプセルと箱の最も深い（近い）接触。中心線の全体（胴体を含む）について箱までの符号付き距離が最小の点で判定する。
 * @param A カプセル。
 * @param B 箱。
 */
FContactPoint3D FindContact(const FCapsule& A, const FOBB& B);
/**
 * カプセル同士の接触（中心線分同士の最も近い点の球として判定）。
 * @param A 一つ目のカプセル。
 * @param B 二つ目のカプセル。
 */
FContactPoint3D FindContact(const FCapsule& A, const FCapsule& B);
/**
 * カプセルと箱の接触点の集まり（中心線の両端と最も近い点のうち、表面間の距離がMargin以下の点。重なる点は一つにまとめる）。
 * 横たわったカプセルが面の上で転がらないよう、両端の点も返す。法線は箱からカプセルへ向く。FeatureIdは0（最も近い点）・1（Start）・2（End）。
 * @param A カプセル。
 * @param B 箱。
 * @param Margin 含める最大の表面間の距離（有限・非負）。
 * @param Out 接触点の書込み先。
 * @return 書き込んだ数（0〜MaxCapsuleContacts）。
 */
uint32 FindCapsuleContacts(const FCapsule& A, const FOBB& B, f32 Margin, FContactPoint3D (&Out)[MaxCapsuleContacts]);
/**
 * カプセル同士の接触点の集まり（最も近い点の組と、Aの両端から最も近いBの点）。規則はカプセルと箱と同じ。法線はBからAへ向く。
 * @param A 一つ目のカプセル。
 * @param B 二つ目のカプセル。
 * @param Margin 含める最大の表面間の距離（有限・非負）。
 * @param Out 接触点の書込み先。
 * @return 書き込んだ数（0〜MaxCapsuleContacts）。
 */
uint32 FindCapsuleContacts(const FCapsule& A, const FCapsule& B, f32 Margin,
                           FContactPoint3D (&Out)[MaxCapsuleContacts]);
/**
 * 球と対象のカプセルの符号付き距離と、対象から球へ向く法線（方向を区別できない場合は空）。
 * @param Shape 調べる球。
 * @param Target 対象のカプセル。
 */
FShapeContact3D FindShapeContact(const FSphere& Shape, const FCapsule& Target);
/**
 * カプセルと対象の球の符号付き距離と法線（対象からカプセルへ向く）。
 * @param Shape 調べるカプセル。
 * @param Target 対象の球。
 */
FShapeContact3D FindShapeContact(const FCapsule& Shape, const FSphere& Target);
/**
 * カプセルと対象の箱の符号付き距離と法線（中心線の全体で箱までの距離が最小の点で判定する）。
 * @param Shape 調べるカプセル。
 * @param Target 対象の箱。
 */
FShapeContact3D FindShapeContact(const FCapsule& Shape, const FOBB& Target);
/**
 * カプセル同士の符号付き距離と法線（中心線分同士の最も近い点で判定する）。
 * 重なっている場合の値は中心線の距離から半径の和を引いた値で、最小の押し出し距離とは限らない。
 * @param Shape 調べるカプセル。
 * @param Target 対象のカプセル。
 */
FShapeContact3D FindShapeContact(const FCapsule& Shape, const FCapsule& Target);
} // namespace Toolbox
#endif
