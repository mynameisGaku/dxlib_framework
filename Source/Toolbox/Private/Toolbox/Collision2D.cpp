// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Collision2D.h"
namespace Toolbox
{
namespace
{
// 無効な入力を非交差と区別する。
void Validate_Internal(bool bShapesValid, f32 Tolerance)
{
	if (!bShapesValid || !IsFinite(Tolerance) || Tolerance < 0)
	{
		throw FException("Invalid 2D collision input");
	}
}
} // namespace
bool IsValid(const FCircle2D& Circle) noexcept
{
	return Circle.Center.IsValid() && IsFinite(Circle.Radius) && Circle.Radius >= 0;
}
bool IsValid(const FAABB2D& Box) noexcept
{
	return Box.Min.IsValid() && Box.Max.IsValid() && Box.Min.X <= Box.Max.X && Box.Min.Y <= Box.Max.Y;
}
bool Intersects(const FCircle2D& A, const FCircle2D& B, f32 Tolerance)
{
	Validate_Internal(IsValid(A) && IsValid(B), Tolerance);
	const f64 X = f64(A.Center.X) - B.Center.X;
	const f64 Y = f64(A.Center.Y) - B.Center.Y;
	const f64 Limit = f64(A.Radius) + B.Radius + Tolerance;
	return X * X + Y * Y <= Limit * Limit;
}
bool Intersects(const FCircle2D& Circle, const FAABB2D& Box, f32 Tolerance)
{
	Validate_Internal(IsValid(Circle) && IsValid(Box), Tolerance);
	const f64 X = f64(Circle.Center.X) - Clamp(f64(Circle.Center.X), f64(Box.Min.X), f64(Box.Max.X));
	const f64 Y = f64(Circle.Center.Y) - Clamp(f64(Circle.Center.Y), f64(Box.Min.Y), f64(Box.Max.Y));
	const f64 Limit = f64(Circle.Radius) + Tolerance;
	return X * X + Y * Y <= Limit * Limit;
}
bool Intersects(const FAABB2D& A, const FAABB2D& B, f32 Tolerance)
{
	Validate_Internal(IsValid(A) && IsValid(B), Tolerance);
	const f64 X = Max(0.0, Max(f64(A.Min.X) - B.Max.X, f64(B.Min.X) - A.Max.X));
	const f64 Y = Max(0.0, Max(f64(A.Min.Y) - B.Max.Y, f64(B.Min.Y) - A.Max.Y));
	return X * X + Y * Y <= f64(Tolerance) * Tolerance;
}
} // namespace Toolbox
