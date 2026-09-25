// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_WORLD_QUERY_SHAPES_3D_H
#define DXF_PRIVATE_PHYSICS_WORLD_QUERY_SHAPES_3D_H
#include "QueryBounds.h"
#include "Toolbox/CollisionShapes.h"
namespace Dxf::PhysicsPrivate
{
/**
 * World座標の球を覆う索引用の境界を返す。問い合わせの詳細判定が拒否する無効な形状は索引に入れない。
 * @param World 現在の姿勢へ移した球。
 */
inline TQueryShapeBounds<3> QueryShapeBounds_Internal(const Toolbox::FSphere& World) noexcept
{
	TQueryShapeBounds<3> Result;
	Result.bIndexable = World.Center.IsValid() && Toolbox::IsFinite(World.Radius) && World.Radius >= 0;
	if (!Result.bIndexable)
	{
		return Result;
	}
	for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
	{
		Result.Tight.Min[Axis] = Toolbox::f64(World.Center.Component(Axis)) - World.Radius;
		Result.Tight.Max[Axis] = Toolbox::f64(World.Center.Component(Axis)) + World.Radius;
	}
	Result.Tight = Padded_Internal(Result.Tight);
	return Result;
}
/**
 * World座標のOBBを覆う索引用の境界を返す。格納した実際の軸の平行六面体を覆う（軸を正規化・直交化しない）。
 * 問い合わせの詳細判定と同じ条件（IsValid）で無効な形状は索引に入れない。
 * @param World 現在の姿勢へ移したOBB。
 */
inline TQueryShapeBounds<3> QueryShapeBounds_Internal(const Toolbox::FOBB& World) noexcept
{
	TQueryShapeBounds<3> Result;
	Result.bIndexable = Toolbox::IsValid(World);
	if (!Result.bIndexable)
	{
		return Result;
	}
	for (Toolbox::int32 Component = 0; Component < 3; ++Component)
	{
		// 中心から各軸方向へ半幅だけ進んだ変位の、この成分の絶対値の和。
		Toolbox::f64 Reach = 0;
		for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
		{
			Reach += Toolbox::Abs(Toolbox::f64(World.Axes[static_cast<Toolbox::size_t>(Axis)].Component(Component))) *
			         World.HalfExtents.Component(Axis);
		}
		Result.Tight.Min[Component] = Toolbox::f64(World.Center.Component(Component)) - Reach;
		Result.Tight.Max[Component] = Toolbox::f64(World.Center.Component(Component)) + Reach;
	}
	Result.Tight = Padded_Internal(Result.Tight);
	return Result;
}
} // namespace Dxf::PhysicsPrivate
#endif
