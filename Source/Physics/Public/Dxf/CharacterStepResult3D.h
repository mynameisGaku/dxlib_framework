// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_STEP_RESULT_3D_H
#define DXF_CHARACTER_STEP_RESULT_3D_H
#include "Dxf/CharacterMoveResult3D.h"
#include "Dxf/CharacterPushSet3D.h"
#include "Dxf/CharacterRecovery3D.h"
#include "Dxf/CharacterState3D.h"
namespace Dxf
{
/**
 * StepCharacterの結果。すべての計算が成功した場合だけ返る値で、Worldは変更しない。
 */
struct FCharacterStepResult3D
{
	/**
	 * 更新後の状態（位置・速度・足元）。
	 */
	FCharacterState3D State;
	/**
	 * 初期重なりの解消の結果。解消できなかった場合は移動していない。
	 */
	FCharacterRecovery3D Recovery;
	/**
	 * 水平方向の移動（段差上りを採用した場合はその結果）。
	 */
	FCharacterMoveResult3D Horizontal;
	/**
	 * Up方向の移動（ジャンプ・重力）。
	 */
	FCharacterMoveResult3D Vertical;
	/**
	 * 歩ける床からジャンプした。
	 */
	bool bJumped = false;
	/**
	 * 空中または急斜面から歩ける床へ着地した。
	 */
	bool bLanded = false;
	/**
	 * 歩ける床を離れた（ジャンプ・崖・急斜面への移動）。
	 */
	bool bLeftGround = false;
	/**
	 * 上昇中に天井へ当たり、上向きの速度を失った。
	 */
	bool bHitCeiling = false;
	/**
	 * 段差を上った。
	 */
	bool bSteppedUp = false;
	/**
	 * 床へ吸い付いた（下り坂・小さな段差の下り）。
	 */
	bool bSnapped = false;
	/**
	 * 固定更新の開始時に乗っていた動く床（KinematicのBody）のCollider。追従を試みなかった場合は空。
	 */
	Toolbox::TOptional<FColliderId3D> Carrier;
	/**
	 * 床の運動が求めた中心の移動量（物理Step1回分）。
	 */
	Toolbox::FVector3 CarryRequested;
	/**
	 * 床の運動の追従の移動（床以外のSolidとの経路を検査した結果）。
	 */
	FCharacterMoveResult3D Carry;
	/**
	 * 床の運動に追従した。
	 */
	bool bCarried = false;
	/**
	 * 追従の途中で床以外のSolid（壁・天井）に止められ、床の運動の一部を追従できなかった（挟まれ）。
	 */
	bool bCarryBlocked = false;
	/**
	 * 床の速さ・一回の回転量・曲線分割の上限を超え、追従を始めずに拒否した。床の変位・速度を加えていない。
	 */
	bool bCarryRejected = false;
	/**
	 * 床を離れた固定更新で、床の点の速度を速度へ加えた。
	 */
	bool bInheritedGroundVelocity = false;
	/**
	 * 行ったWorld問い合わせの合計回数。
	 */
	Toolbox::int32 Queries = 0;
	/**
	 * Dynamicの剛体を押す要求（bPushDynamicBodiesの場合）。呼出し側が物理Stepの前に適用する。
	 */
	FCharacterPushSet3D Pushes;
	/**
	 * 近づく剛体に押されて退いた移動（bReceiveDynamicPushの場合。押されなければNoMovement）。
	 */
	FCharacterMoveResult3D Received;
	/**
	 * 押されて退いたか。
	 */
	bool bPushedByBody = false;
};
} // namespace Dxf
#endif
