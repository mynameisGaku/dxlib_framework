// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_STEP_RESULT_3D_H
#define DXF_CHARACTER_STEP_RESULT_3D_H
#include "Dxf/CharacterMoveResult3D.h"
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
	 * 行ったWorld問い合わせの合計回数。
	 */
	Toolbox::int32 Queries = 0;
};
} // namespace Dxf
#endif
