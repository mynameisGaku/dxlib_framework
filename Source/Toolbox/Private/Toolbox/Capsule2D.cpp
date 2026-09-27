// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Capsule2D.h"
#include "CapsuleMath.h"
namespace Toolbox
{
bool IsValid(const FCapsule2D& Capsule) noexcept
{
	if (!Capsule.Start.IsValid() || !Capsule.End.IsValid() || !IsFinite(Capsule.Radius) || Capsule.Radius < 0)
	{
		return false;
	}
	// 中心線の長さ（差）が有限であること。
	const f64 X = f64(Capsule.End.X) - Capsule.Start.X;
	const f64 Y = f64(Capsule.End.Y) - Capsule.Start.Y;
	return IsFinite(X * X + Y * Y);
}
FAABB2D CapsuleBounds(const FCapsule2D& Capsule) noexcept
{
	const f32 R = Capsule.Radius;
	FAABB2D Bounds;
	Bounds.Min = {Min(Capsule.Start.X, Capsule.End.X) - R, Min(Capsule.Start.Y, Capsule.End.Y) - R};
	Bounds.Max = {Max(Capsule.Start.X, Capsule.End.X) + R, Max(Capsule.Start.Y, Capsule.End.Y) + R};
	return Bounds;
}
FVector2 ClosestPointOnCapsuleAxis(const FCapsule2D& Capsule, FVector2 Point) noexcept
{
	using namespace CapsulePrivate;
	const FPoint A{Capsule.Start.X, Capsule.Start.Y, 0};
	const FPoint B{Capsule.End.X, Capsule.End.Y, 0};
	const FPoint P{Point.X, Point.Y, 0};
	const FPoint Q = Add(A, Scale(Sub(B, A), ClosestParam(A, B, P)));
	return {static_cast<f32>(Q.X), static_cast<f32>(Q.Y)};
}
FVector2 CapsuleCenter(const FCapsule2D& Capsule) noexcept
{
	return {static_cast<f32>((f64(Capsule.Start.X) + Capsule.End.X) * 0.5),
	        static_cast<f32>((f64(Capsule.Start.Y) + Capsule.End.Y) * 0.5)};
}
} // namespace Toolbox
