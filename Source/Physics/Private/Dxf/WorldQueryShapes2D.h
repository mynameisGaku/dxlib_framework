// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_WORLD_QUERY_SHAPES_2D_H
#define DXF_PRIVATE_PHYSICS_WORLD_QUERY_SHAPES_2D_H
#include "QueryBounds.h"
#include "Toolbox/Collision2D.h"
#include "Toolbox/Contact2D.h"
namespace Dxf::PhysicsPrivate
{
/**
 * World座標の円を覆う索引用の境界を返す。問い合わせの詳細判定が拒否する無効な形状は索引に入れない。
 * @param World 現在の姿勢へ移した円。
 */
inline TQueryShapeBounds<2> QueryShapeBounds_Internal(const Toolbox::FCircle2D& World) noexcept
{
	TQueryShapeBounds<2> Result;
	Result.bIndexable = Toolbox::IsValid(World);
	if (!Result.bIndexable)
	{
		return Result;
	}
	Result.Tight.Min[0] = Toolbox::f64(World.Center.X) - World.Radius;
	Result.Tight.Max[0] = Toolbox::f64(World.Center.X) + World.Radius;
	Result.Tight.Min[1] = Toolbox::f64(World.Center.Y) - World.Radius;
	Result.Tight.Max[1] = Toolbox::f64(World.Center.Y) + World.Radius;
	Result.Tight = Padded_Internal(Result.Tight);
	return Result;
}
/**
 * World座標の回転矩形を覆う索引用の境界を返す。詳細判定と同じf64の角度の余弦・正弦を使う。
 * @param World 現在の姿勢へ移した回転矩形。
 */
inline TQueryShapeBounds<2> QueryShapeBounds_Internal(const Toolbox::FOrientedBox2D& World) noexcept
{
	TQueryShapeBounds<2> Result;
	Result.bIndexable = Toolbox::IsValid(World);
	if (!Result.bIndexable)
	{
		return Result;
	}
	const Toolbox::f64 Cosine = Toolbox::Abs(Toolbox::Cos(Toolbox::f64(World.Angle)));
	const Toolbox::f64 Sine = Toolbox::Abs(Toolbox::Sin(Toolbox::f64(World.Angle)));
	const Toolbox::f64 ReachX = Cosine * World.HalfExtents.X + Sine * World.HalfExtents.Y;
	const Toolbox::f64 ReachY = Sine * World.HalfExtents.X + Cosine * World.HalfExtents.Y;
	Result.Tight.Min[0] = Toolbox::f64(World.Center.X) - ReachX;
	Result.Tight.Max[0] = Toolbox::f64(World.Center.X) + ReachX;
	Result.Tight.Min[1] = Toolbox::f64(World.Center.Y) - ReachY;
	Result.Tight.Max[1] = Toolbox::f64(World.Center.Y) + ReachY;
	Result.Tight = Padded_Internal(Result.Tight);
	return Result;
}
} // namespace Dxf::PhysicsPrivate
#endif
