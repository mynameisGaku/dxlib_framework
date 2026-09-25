// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_STATE_2D_H
#define DXF_CHARACTER_STATE_2D_H
#include "Dxf/CharacterGround2D.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * キャラクターの位置・速度・足元の状態。StepCharacterの入力と結果に使う値。
 */
struct FCharacterState2D
{
	/**
	 * 円の中心（物理ワールド座標）。Bodyの重心とは限らない。
	 */
	Toolbox::FVector2 Center;
	/**
	 * 速度（距離毎秒）。
	 */
	Toolbox::FVector2 Velocity;
	/**
	 * 直近の足元の支持。
	 */
	FCharacterGround2D Ground;
};
} // namespace Dxf
#endif
