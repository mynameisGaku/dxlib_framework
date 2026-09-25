// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_MOVE_SETTINGS_3D_H
#define DXF_CHARACTER_MOVE_SETTINGS_3D_H
#include "Dxf/CharacterMoveTuning.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 3Dキャラクター移動の設定。共通の値はFCharacterMoveTuningと同じ名前・単位。
 */
struct FCharacterMoveSettings3D : FCharacterMoveTuning
{
	/**
	 * 上方向（長さが正の有限値。内部で単位化する）。床・天井・ジャンプ・重力の向きの基準で、Worldの重力とは独立。
	 */
	Toolbox::FVector3 Up{0, 1, 0};
};
} // namespace Dxf
#endif
