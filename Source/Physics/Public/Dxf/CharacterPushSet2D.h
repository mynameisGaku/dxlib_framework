// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_PUSH_SET_2D_H
#define DXF_CHARACTER_PUSH_SET_2D_H
#include "Dxf/BodyId2D.h"
#include "Dxf/ColliderId2D.h"
#include "Toolbox/Array.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * キャラクターがDynamicの剛体を押す要求（1つの剛体あたり1件）。呼出し側が物理Stepの前に適用する。
 */
struct FCharacterPush2D
{
	/**
	 * 押す剛体。
	 */
	FBodyId2D Body;
	/**
	 * 最初に押したCollider（その剛体の、移動を止めた面）。
	 */
	FColliderId2D Collider;
	/**
	 * 重心へ与える並進のImpulse（Upに直交、大きさはMaxPushImpulse以下）。
	 */
	Toolbox::FVector2 Impulse;
};
/**
 * 1回の固定更新の押す要求の集まり。固定容量で配列を確保しない。
 */
struct FCharacterPushSet2D
{
	/**
	 * 保持できる要求の最大数。
	 */
	static constexpr Toolbox::uint32 Capacity = 8;
	/**
	 * 保持した要求（移動を止めた面の順）。
	 */
	Toolbox::TArray<FCharacterPush2D, Capacity> Items;
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
