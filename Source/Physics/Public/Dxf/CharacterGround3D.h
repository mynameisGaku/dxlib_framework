// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_GROUND_3D_H
#define DXF_CHARACTER_GROUND_3D_H
#include "Dxf/CharacterGroundState.h"
#include "Dxf/ColliderId3D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 問い合わせ時点の足元の支持。後のWorld変更に対するIDの生存保証はない。
 */
struct FCharacterGround3D
{
	/**
	 * 支持の状態。
	 */
	ECharacterGroundState State = ECharacterGroundState::Airborne;
	/**
	 * 支持しているCollider（Airborneなら空）。
	 */
	Toolbox::TOptional<FColliderId3D> Collider;
	/**
	 * 支持面の単位法線（Airborneなら空）。
	 */
	Toolbox::TOptional<Toolbox::FVector3> Normal;
	/**
	 * 支持面との符号付き距離（Airborneなら0）。
	 */
	Toolbox::f64 Separation = 0;
	/**
	 * 足元の候補になる接触をすべて確認できたか。falseなら接触が容量を超え、より適した支持面を見落とした可能性がある。
	 */
	bool bComplete = true;
};
} // namespace Dxf
#endif
