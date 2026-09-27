// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CAPSULE_2D_H
#define TOOLBOX_CAPSULE_2D_H
#include "Toolbox/Collision2D.h"
#include "Toolbox/Vector2.h"
namespace Toolbox
{
/**
 * 2Dのカプセル（stadium）。中心線分（Start〜End）から半径Radius以内の点の集合（両端は半円）。
 * 中心線の長さ0は、Startを中心とする円として有効。3DのFCapsuleと同じ定義。
 */
struct FCapsule2D
{
	/**
	 * 中心線分の一端。
	 */
	FVector2 Start;
	/**
	 * 中心線分の他端。
	 */
	FVector2 End;
	/**
	 * 中心線分からの半径（有限・非負）。
	 */
	f32 Radius = 0.5f;
};
/**
 * 端点が有限で、半径が有限・非負で、中心線の長さが有限か。
 * @param Capsule 調べるカプセル。
 */
bool IsValid(const FCapsule2D& Capsule) noexcept;
/**
 * カプセルを覆う軸平行境界（両端点の範囲を半径だけ広げる）。
 * @param Capsule 有効なカプセル。
 */
FAABB2D CapsuleBounds(const FCapsule2D& Capsule) noexcept;
/**
 * 中心線分上で、指定点に最も近い点。
 * @param Capsule 有効なカプセル。
 * @param Point 点。
 */
FVector2 ClosestPointOnCapsuleAxis(const FCapsule2D& Capsule, FVector2 Point) noexcept;
} // namespace Toolbox
#endif
