// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_SEGMENT_HIT_2D_H
#define DXF_WORLD_SEGMENT_HIT_2D_H
#include "Dxf/ColliderId2D.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * 2D Worldへの問い合わせ時点の最短交点。後のWorld変更に対するIDの生存保証はない。
 */
struct FWorldSegmentHit2D
{
	/**
	 * World/Body/Colliderの世代を含む非所有ID。
	 */
	FColliderId2D Collider;
	/**
	 * 始点0～終点1の割合。距離や時刻ではない。
	 */
	Toolbox::f64 Fraction = 0;
	/**
	 * 有限な2D物理ワールドの交点（メートル、Y上向き）。始点が内部・境界上なら始点。
	 */
	Toolbox::FVector2 Position;
};
} // namespace Dxf
#endif
