// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Capsule.h"
#include "CapsuleMath.h"
namespace Toolbox
{
bool IsValid(const FCapsule& Capsule) noexcept
{
	if (!Capsule.Start.IsValid() || !Capsule.End.IsValid() || !IsFinite(Capsule.Radius) || Capsule.Radius < 0)
	{
		return false;
	}
	// 中心線の長さ（差）が有限であること。
	const f64 X = f64(Capsule.End.X) - Capsule.Start.X;
	const f64 Y = f64(Capsule.End.Y) - Capsule.Start.Y;
	const f64 Z = f64(Capsule.End.Z) - Capsule.Start.Z;
	return IsFinite(X * X + Y * Y + Z * Z);
}
FAABB CapsuleBounds(const FCapsule& Capsule) noexcept
{
	const f32 R = Capsule.Radius;
	FAABB Bounds;
	Bounds.Min = {Min(Capsule.Start.X, Capsule.End.X) - R, Min(Capsule.Start.Y, Capsule.End.Y) - R,
	              Min(Capsule.Start.Z, Capsule.End.Z) - R};
	Bounds.Max = {Max(Capsule.Start.X, Capsule.End.X) + R, Max(Capsule.Start.Y, Capsule.End.Y) + R,
	              Max(Capsule.Start.Z, Capsule.End.Z) + R};
	return Bounds;
}
FVector3 ClosestPointOnCapsuleAxis(const FCapsule& Capsule, FVector3 Point) noexcept
{
	using namespace CapsulePrivate;
	const FPoint A{Capsule.Start.X, Capsule.Start.Y, Capsule.Start.Z};
	const FPoint B{Capsule.End.X, Capsule.End.Y, Capsule.End.Z};
	const FPoint P{Point.X, Point.Y, Point.Z};
	const FPoint Q = Add(A, Scale(Sub(B, A), ClosestParam(A, B, P)));
	return {static_cast<f32>(Q.X), static_cast<f32>(Q.Y), static_cast<f32>(Q.Z)};
}
} // namespace Toolbox
