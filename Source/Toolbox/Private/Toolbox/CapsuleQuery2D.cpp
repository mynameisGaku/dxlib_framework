// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/CapsuleQuery2D.h"
#include "Toolbox/CapsuleContact2D.h"
#include "Toolbox/SegmentIntersection2D.h"
#include "Toolbox/ShapeContactQuery2D.h"
#include "CapsuleMath.h"
namespace Toolbox
{
namespace
{
using CapsulePrivate::FPoint;

FPoint Load_Internal(FVector2 Value) noexcept
{
	return {Value.X, Value.Y, 0};
}
// 最小の値（空は無視する）。
TOptional<f64> Earliest_Internal(const TOptional<f64>& A, const TOptional<f64>& B) noexcept
{
	if (!A)
	{
		return B;
	}
	if (!B)
	{
		return A;
	}
	return *A < *B ? A : B;
}
// 中心線の中点を指定の割合だけ移動したカプセル。
FCapsule2D Moved_Internal(const FCapsule2D& Capsule, const FPoint& Move, f64 Time) noexcept
{
	FCapsule2D Result = Capsule;
	const f32 X = static_cast<f32>(Move.X * Time);
	const f32 Y = static_cast<f32>(Move.Y * Time);
	Result.Start = {Capsule.Start.X + X, Capsule.Start.Y + Y};
	Result.End = {Capsule.End.X + X, Capsule.End.Y + Y};
	return Result;
}
// 保守的な前進の許容距離。移動量・形状の規模に比例させ、f32の丸めより大きくする。
f64 Tolerance_Internal(const FCapsule2D& Capsule, f64 Length) noexcept
{
	const f64 Scale = Max(Max(Abs(f64(Capsule.Start.X)), Abs(f64(Capsule.Start.Y))), Max(Length, f64(Capsule.Radius)));
	return 1e-6 * Max(1.0, Scale);
}
// カプセルの平行移動の共通部分。ContactはShapeと対象の符号付き距離と法線を返す関数。
template <typename TContact>
TOptional<FShapeSweepHit2D> SweepCapsule_Internal(const FCapsule2D& Moving, FVector2 EndCenter, TContact&& Contact)
{
	if (!IsValid(Moving) || !EndCenter.IsValid())
	{
		throw FException("Invalid 2D capsule sweep");
	}
	const FVector2 Center = CapsuleCenter(Moving);
	const FPoint Move{f64(EndCenter.X) - Center.X, f64(EndCenter.Y) - Center.Y, 0};
	const f64 Length = Sqrt(CapsulePrivate::Dot(Move, Move));
	if (!IsFinite(Length))
	{
		throw FException("Invalid 2D capsule sweep displacement");
	}
	const FShapeContact2D Start = Contact(Moving);
	if (Start.Separation <= 0)
	{
		FShapeSweepHit2D Hit;
		Hit.Time = 0;
		Hit.bInitialContact = true;
		return Hit;
	}
	const f64 Tolerance = Tolerance_Internal(Moving, Length);
	const TOptional<f64> Time = CapsulePrivate::AdvanceConservatively(
	    [&](f64 T)
	    {
		    return Contact(Moved_Internal(Moving, Move, T)).Separation;
	    },
	    Length, Tolerance);
	if (!Time)
	{
		return {};
	}
	FShapeSweepHit2D Hit;
	Hit.Time = *Time;
	Hit.Normal = Contact(Moved_Internal(Moving, Move, *Time)).Normal;
	return Hit;
}
} // namespace

TOptional<f64> IntersectSegment(FVector2 Start, FVector2 End, const FCapsule2D& Capsule)
{
	if (!Start.IsValid() || !End.IsValid() || !IsValid(Capsule))
	{
		throw FException("Invalid 2D capsule segment intersection");
	}
	// 始点が内部なら0。
	if (FindShapeContact(FCircle2D{Start, 0}, Capsule).Separation <= 0)
	{
		return 0.0;
	}
	TOptional<f64> Body =
	    CapsulePrivate::SegmentBody(Load_Internal(Start), Load_Internal(End), Load_Internal(Capsule.Start),
	                                Load_Internal(Capsule.End), Capsule.Radius);
	Body = Earliest_Internal(Body, IntersectSegment(Start, End, FCircle2D{Capsule.Start, Capsule.Radius}));
	return Earliest_Internal(Body, IntersectSegment(Start, End, FCircle2D{Capsule.End, Capsule.Radius}));
}
TOptional<FShapeSweepHit2D> SweepToCenter(const FCircle2D& Moving, FVector2 EndCenter, const FCapsule2D& Target)
{
	if (!Moving.Center.IsValid() || !EndCenter.IsValid() || !IsFinite(Moving.Radius) || Moving.Radius < 0 ||
	    !IsValid(Target))
	{
		throw FException("Invalid 2D sphere capsule sweep");
	}
	if (FindShapeContact(Moving, Target).Separation <= 0)
	{
		FShapeSweepHit2D Hit;
		Hit.bInitialContact = true;
		return Hit;
	}
	// 球の中心の線分と、半径の和のカプセルの交点。
	FCapsule2D Grown = Target;
	Grown.Radius = static_cast<f32>(f64(Target.Radius) + Moving.Radius);
	const TOptional<f64> Time = IntersectSegment(Moving.Center, EndCenter, Grown);
	if (!Time)
	{
		return {};
	}
	FShapeSweepHit2D Hit;
	Hit.Time = *Time;
	const FVector2 At = {static_cast<f32>(Moving.Center.X + (f64(EndCenter.X) - Moving.Center.X) * *Time),
	                     static_cast<f32>(Moving.Center.Y + (f64(EndCenter.Y) - Moving.Center.Y) * *Time)};
	Hit.Normal = FindShapeContact(FCircle2D{At, Moving.Radius}, Target).Normal;
	return Hit;
}
TOptional<FShapeSweepHit2D> SweepToCenter(const FCapsule2D& Moving, FVector2 EndCenter, const FCircle2D& Target)
{
	return SweepCapsule_Internal(Moving, EndCenter,
	                             [&](const FCapsule2D& Shape)
	                             {
		                             return FindShapeContact(Shape, Target);
	                             });
}
TOptional<FShapeSweepHit2D> SweepToCenter(const FCapsule2D& Moving, FVector2 EndCenter, const FOrientedBox2D& Target)
{
	return SweepCapsule_Internal(Moving, EndCenter,
	                             [&](const FCapsule2D& Shape)
	                             {
		                             return FindShapeContact(Shape, Target);
	                             });
}
TOptional<FShapeSweepHit2D> SweepToCenter(const FCapsule2D& Moving, FVector2 EndCenter, const FCapsule2D& Target)
{
	return SweepCapsule_Internal(Moving, EndCenter,
	                             [&](const FCapsule2D& Shape)
	                             {
		                             return FindShapeContact(Shape, Target);
	                             });
}
} // namespace Toolbox
