// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_PUSH_SET_3D_H
#define DXF_CHARACTER_PUSH_SET_3D_H
#include "Dxf/BodyId3D.h"
#include "Dxf/ColliderId3D.h"
#include "Toolbox/Array.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * キャラクターがDynamicの剛体を押す要求（1つの剛体あたり1件）。呼出し側が物理Stepの前に適用する。
 */
struct FCharacterPush3D
{
	/**
	 * 押す剛体。
	 */
	FBodyId3D Body;
	/**
	 * 最初に押したCollider（その剛体の、移動を止めた面）。
	 */
	FColliderId3D Collider;
	/**
	 * 重心へ与える並進のImpulse（Upに直交、大きさはMaxPushImpulse以下）。
	 */
	Toolbox::FVector3 Impulse;
};
/**
 * 1回の固定更新の押す要求の集まり。固定容量で配列を確保しない。
 */
struct FCharacterPushSet3D
{
	/**
	 * 保持できる要求の最大数。
	 */
	static constexpr Toolbox::uint32 Capacity = 8;
	/**
	 * 保持した要求（移動を止めた面の順）。
	 */
	Toolbox::TArray<FCharacterPush3D, Capacity> Items;
	/**
	 * Itemsのうち有効な件数（Capacity以下）。
	 */
	Toolbox::uint32 Count = 0;
	/**
	 * 押す剛体の全件数。Countより大きければ、容量を超えた剛体は押さない。
	 */
	Toolbox::uint32 TotalFound = 0;
};
} // namespace Dxf
#endif
