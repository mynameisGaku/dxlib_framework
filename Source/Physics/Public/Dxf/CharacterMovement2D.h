// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_MOVEMENT_2D_H
#define DXF_CHARACTER_MOVEMENT_2D_H
#include "Dxf/CharacterGround2D.h"
#include "Dxf/CharacterMoveInput2D.h"
#include "Dxf/CharacterMoveResult2D.h"
#include "Dxf/CharacterMoveSettings2D.h"
#include "Dxf/CharacterRecovery2D.h"
#include "Dxf/CharacterResize2D.h"
#include "Dxf/CharacterState2D.h"
#include "Dxf/CharacterStepResult2D.h"
#include "Dxf/RigidBody2D.h"
namespace Dxf
{
/*
 * 形状：Settings.ShapeがRound（既定）なら中心と半径の円、Capsuleなら中心線がSettings.Upに沿うカプセル
 * （中心は中心線の中点、両端は中心からUpの向きに±HalfHeight、半径はRadius）で、すべての問い合わせを行う。
 * 各関数の「円の中心」は、Capsuleでは中心線の中点を指す。
 */
/**
 * 現在の中心で重なっている（符号付き距離が負の）Colliderから、円を押し出す候補を求める（Worldは変更しない）。
 * 最も深い重なりから順に、その法線方向へ接触余裕（SkinWidth）の位置まで動かし、丸めた位置で接触を取り直す。
 * 補正の経路は、開始時に接触していない他のColliderに当たらないことをSweepで確認する。方向を決められない、
 * 深すぎる、経路が塞がれる、反復・問い合わせ・接触数の上限に達した場合は解消せず、元の中心と理由を返す。
 * 境界だけの接触（距離0）は重なりではない。入力・World状態の異常はToolbox::FException。
 * @param World 問い合わせるWorld。
 * @param Center 円の中心。
 * @param Settings 半径・接触余裕・解消の上限。
 * @param ExcludedBody 任意の自己Body。除外しない場合は空Optional。
 * @param Filter 対象にする問い合わせカテゴリ。
 */
FCharacterRecovery2D ResolveCharacterOverlap(const FPhysicsWorld2D& World, Toolbox::FVector2 Center,
                                             const FCharacterMoveSettings2D& Settings,
                                             Toolbox::TOptional<FBodyId2D> ExcludedBody = {},
                                             const FWorldQueryFilter& Filter = {});
/**
 * 円をDisplacementだけ動かす候補を、接触面に沿って反復的に解決する（Worldは変更しない）。
 * 開始時に接触余裕以内の面と、移動中に当たった面を制約にし、残りの移動からすべての制約の内向き成分を除いて進む
 * （2Dは二つの面の角で止まる）。当たった面へは接触余裕の距離まで近づく。
 * 反復・問い合わせ・接触数の上限、法線なし、丸めの限界では止まり、理由と未処理の移動を返す。すべての経路は
 * 開始時に接触していないColliderについてSweepで検査し、開始時の接触面は制約で内向きの移動を禁じる。
 * 対象のIDを除外して進むことはない。入力・World状態の異常はToolbox::FException（途中の結果は返さない）。
 * @param World 問い合わせるWorld。
 * @param Center 開始時の円の中心。
 * @param Displacement 希望する変位（終点ではない）。
 * @param Settings 半径・接触余裕・反復と問い合わせの上限。
 * @param ExcludedBody 任意の自己Body。除外しない場合は空Optional。
 * @param Filter 対象にする問い合わせカテゴリ。
 */
FCharacterMoveResult2D MoveAndSlide(const FPhysicsWorld2D& World, Toolbox::FVector2 Center,
                                    Toolbox::FVector2 Displacement, const FCharacterMoveSettings2D& Settings,
                                    Toolbox::TOptional<FBodyId2D> ExcludedBody = {},
                                    const FWorldQueryFilter& Filter = {});
/**
 * 足元の支持を調べる（Worldは変更しない）。接触余裕の2倍以内の、法線がUp側を向く面を候補にし、
 * 歩ける傾斜の面を優先して最も近いものを選ぶ。初期接触（距離0）や重なりでも接触の法線から判定する。
 * 下向きに何かがあるかだけでは接地としない。入力・World状態の異常はToolbox::FException。
 * @param World 問い合わせるWorld。
 * @param Center 円の中心。
 * @param Settings 半径・接触余裕・Up・最大傾斜。
 * @param ExcludedBody 任意の自己Body。除外しない場合は空Optional。
 * @param Filter 対象にする問い合わせカテゴリ。
 */
FCharacterGround2D ProbeCharacterGround(const FPhysicsWorld2D& World, Toolbox::FVector2 Center,
                                        const FCharacterMoveSettings2D& Settings,
                                        Toolbox::TOptional<FBodyId2D> ExcludedBody = {},
                                        const FWorldQueryFilter& Filter = {});
/**
 * キャラクターを1回の固定更新だけ進めた結果を計算する（Worldは変更しない。結果を採用するのは呼出し側）。
 * 順に、初期重なりの解消、足元の確認、速度の更新（Upに直交する成分の加減速・ジャンプ・重力）、水平の移動と段差上り、
 * Up方向の移動（天井・着地）、下向きの吸い付き、足元の再確認を行う。同じ入力と状態・Worldなら同じ結果になる。
 * 重なりを解消できなければ移動せず、その理由を返す。すべての問い合わせで同じ自己除外とFilterを使う。
 * 入力・設定・World状態の異常はToolbox::FException（部分的な結果は返さない）。同じWorldの変更・Stepとは直列化する。
 * @param World 問い合わせるWorld。
 * @param Settings 設定。
 * @param State 更新前の状態。
 * @param Input この更新の移動要求。
 * @param DeltaSeconds 固定更新の秒数（有限の正の値）。
 * @param ExcludedBody 任意の自己Body。除外しない場合は空Optional。
 * @param Filter 対象にする問い合わせカテゴリ。
 */
FCharacterStepResult2D StepCharacter(const FPhysicsWorld2D& World, const FCharacterMoveSettings2D& Settings,
                                     const FCharacterState2D& State, const FCharacterMoveInput2D& Input,
                                     Toolbox::f64 DeltaSeconds, Toolbox::TOptional<FBodyId2D> ExcludedBody = {},
                                     const FWorldQueryFilter& Filter = {});
/**
 * カプセルのキャラクターの半高を、足元（中心線の下端の円の底）を保って変えられるか調べる（Worldは変更しない）。
 * 縮める変更は常にできる。伸ばす変更は、新しい形状がSolidのColliderと重なる（低い天井の下など）場合はできず、
 * 元の中心と妨げたColliderを返す（伸ばした形状は途中の高さの形状をすべて含むため、最終の形状だけを調べる）。
 * 結果を採用する場合は、呼出し側が中心とSettings.HalfHeight（と登録したColliderの形状）を同時に更新する。
 * Settings.ShapeがCapsuleでない、NewHalfHeightが有限・非負でない、設定・中心が不正な場合はToolbox::FException。
 * @param World 問い合わせるWorld。
 * @param Center 現在の中心（中心線の中点）。
 * @param Settings 現在の設定（HalfHeightは変更前の値）。
 * @param NewHalfHeight 新しい半高。
 * @param ExcludedBody 任意の自己Body。除外しない場合は空Optional。
 * @param Filter 対象にする問い合わせカテゴリ（Sensorは妨げにしない）。
 */
FCharacterResize2D ResizeCharacterCapsule(const FPhysicsWorld2D& World, Toolbox::FVector2 Center,
                                          const FCharacterMoveSettings2D& Settings, Toolbox::f64 NewHalfHeight,
                                          Toolbox::TOptional<FBodyId2D> ExcludedBody = {},
                                          const FWorldQueryFilter& Filter = {});
} // namespace Dxf
#endif
