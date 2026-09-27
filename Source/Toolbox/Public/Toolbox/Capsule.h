// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CAPSULE_H
#define TOOLBOX_CAPSULE_H
#include "Toolbox/CollisionShapes.h"
#include "Toolbox/Vector3.h"
namespace Toolbox
{
/**
 * 3Dのカプセル。中心線分（Start〜End）から半径Radius以内の点の集合（両端は半球）。
 * 中心線の長さ0は、Startを中心とする球として有効。
 */
struct FCapsule
{
	/**
	 * 中心線分の一端。
	 */
	FVector3 Start;
	/**
	 * 中心線分の他端。
	 */
	FVector3 End;
	/**
	 * 中心線分からの半径（有限・非負）。
	 */
	f32 Radius = 0.5f;
};
/**
 * 端点が有限で、半径が有限・非負で、中心線の長さが有限か。
 * @param Capsule 調べるカプセル。
 */
bool IsValid(const FCapsule& Capsule) noexcept;
/**
 * カプセルを覆う軸平行境界（両端点の範囲を半径だけ広げる）。
 * @param Capsule 有効なカプセル。
 */
FAABB CapsuleBounds(const FCapsule& Capsule) noexcept;
/**
 * 中心線分上で、指定点に最も近い点。
 * @param Capsule 有効なカプセル。
 * @param Point 点。
 */
FVector3 ClosestPointOnCapsuleAxis(const FCapsule& Capsule, FVector3 Point) noexcept;
} // namespace Toolbox
#endif
