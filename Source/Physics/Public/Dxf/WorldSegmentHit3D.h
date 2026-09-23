// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_SEGMENT_HIT_3D_H
#define DXF_WORLD_SEGMENT_HIT_3D_H
#include "Dxf/ColliderId3D.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 問い合わせ時点の最短交点。後のWorld変更に対するIDの生存保証はない。
 */
struct FWorldSegmentHit3D
{
	/**
	 * World/Body/Colliderの世代を含む非所有ID。
	 */
	FColliderId3D Collider;
	/**
	 * 始点0～終点1の割合。距離や時刻ではない。
	 */
	Toolbox::f64 Fraction = 0;
	/**
	 * 有限なワールド交点。始点内部なら始点。
	 */
	Toolbox::FVector3 Position;
};
} // namespace Dxf
#endif
