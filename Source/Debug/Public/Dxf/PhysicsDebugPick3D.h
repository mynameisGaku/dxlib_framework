// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_DEBUG_PICK_3D_H
#define DXF_PHYSICS_DEBUG_PICK_3D_H
#include "Dxf/RigidBody3D.h"
namespace Dxf
{
/**
 * 保存形状との交点。IDは採取当時の値であり、Live Worldでの生存を保証しない。
 */
struct FPhysicsDebugPick3D
{
	/**
	 * World・Body・Colliderの世代を含む識別情報。
	 */
	FColliderId3D Collider;
	/**
	 * 問い合わせたSnapshotの採取Step。同一Snapshotの識別子ではない。
	 */
	Toolbox::uint64 Step = 0;
	/**
	 * 線分の始点0～終点1に対する最初の交点の割合。
	 */
	Toolbox::f64 Fraction = 0;
	/**
	 * 問い合わせ時点のワールド交点。継続選択時の最新交点ではない。
	 */
	Toolbox::FVector3 Position;
};
} // namespace Dxf
#endif
