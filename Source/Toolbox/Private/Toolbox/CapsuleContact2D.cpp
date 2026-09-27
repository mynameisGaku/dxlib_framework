// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/CapsuleContact2D.h"
#include "Toolbox/ShapeContactQuery2D.h"
#include "CapsuleMath.h"
namespace Toolbox
{
namespace
{
using CapsulePrivate::FPoint;

// 公開型の点を倍精度へ。
FPoint Load_Internal(FVector2 Value) noexcept
{
	return {Value.X, Value.Y, 0};
}
// 倍精度の点を公開型へ。
FVector2 Store_Internal(const FPoint& Value) noexcept
{
	return {static_cast<f32>(Value.X), static_cast<f32>(Value.Y)};
}
// カプセルの値を検査する。
void Validate_Internal(const FCapsule2D& Capsule)
{
	if (!IsValid(Capsule))
	{
		throw FException("Invalid 2D capsule");
	}
}
// 中心線上のパラメーターの点。
FVector2 AxisPoint_Internal(const FCapsule2D& Capsule, f64 Time) noexcept
{
	const FPoint A = Load_Internal(Capsule.Start);
	const FPoint B = Load_Internal(Capsule.End);
	return Store_Internal(CapsulePrivate::Add(A, CapsulePrivate::Scale(CapsulePrivate::Sub(B, A), Time)));
}
// 中心線上の、箱までの符号付き距離が最小のパラメーター（点と箱の符号付き距離は凸なので、胴体を含めて探索できる）。
f64 NearestToBox_Internal(const FCapsule2D& Capsule, const FOrientedBox2D& Box)
{
	return CapsulePrivate::MinimizeConvex(
	    [&](f64 Time)
	    {
		    return FindShapeContact(FCircle2D{AxisPoint_Internal(Capsule, Time), 0}, Box).Separation;
	    });
}
// 線分同士の最も近い点のパラメーター。
void NearestToCapsule_Internal(const FCapsule2D& A, const FCapsule2D& B, f64& S, f64& T) noexcept
{
	CapsulePrivate::ClosestParams(Load_Internal(A.Start), Load_Internal(A.End), Load_Internal(B.Start),
	                              Load_Internal(B.End), S, T);
}
// 接触点の候補を、重複を除いて加える。
void AddContact_Internal(const FContactPoint2D& Hit, f64 Time, f64 AxisLength, f32 Margin,
                         FContactPoint2D (&Out)[MaxCapsuleContacts2D], f64 (&Times)[MaxCapsuleContacts2D],
                         uint32& Count)
{
	if (!(Hit.Separation <= Margin))
	{
		return;
	}
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		if (Abs(Times[Index] - Time) * AxisLength <= 1e-4)
		{
			return;
		}
	}
	Out[Count] = Hit;
	Times[Count] = Time;
	++Count;
}
// 中心線の長さ。
f64 AxisLength_Internal(const FCapsule2D& Capsule) noexcept
{
	const FPoint D = CapsulePrivate::Sub(Load_Internal(Capsule.End), Load_Internal(Capsule.Start));
	return Sqrt(CapsulePrivate::Dot(D, D));
}
// 余裕の値を検査する。
void ValidateMargin_Internal(f32 Margin)
{
	if (!IsFinite(Margin) || Margin < 0)
	{
		throw FException("Invalid 2D capsule contact margin");
	}
}
} // namespace

FContactPoint2D FindContact(const FCapsule2D& A, const FCircle2D& B)
{
	Validate_Internal(A);
	return FindContact(FCircle2D{ClosestPointOnCapsuleAxis(A, B.Center), A.Radius}, B);
}
FContactPoint2D FindContact(const FCircle2D& A, const FCapsule2D& B)
{
	Validate_Internal(B);
	return FindContact(A, FCircle2D{ClosestPointOnCapsuleAxis(B, A.Center), B.Radius});
}
FContactPoint2D FindContact(const FCapsule2D& A, const FOrientedBox2D& B)
{
	Validate_Internal(A);
	return FindContact(FCircle2D{AxisPoint_Internal(A, NearestToBox_Internal(A, B)), A.Radius}, B);
}
FContactPoint2D FindContact(const FCapsule2D& A, const FCapsule2D& B)
{
	Validate_Internal(A);
	Validate_Internal(B);
	f64 S = 0;
	f64 T = 0;
	NearestToCapsule_Internal(A, B, S, T);
	return FindContact(FCircle2D{AxisPoint_Internal(A, S), A.Radius}, FCircle2D{AxisPoint_Internal(B, T), B.Radius});
}
uint32 FindCapsuleContacts(const FCapsule2D& A, const FOrientedBox2D& B, f32 Margin,
                           FContactPoint2D (&Out)[MaxCapsuleContacts2D])
{
	Validate_Internal(A);
	ValidateMargin_Internal(Margin);
	const f64 Length = AxisLength_Internal(A);
	f64 Times[MaxCapsuleContacts2D]{};
	uint32 Count = 0;
	// 最も近い点、Start、Endの順（FeatureIdは0・1・2）。
	const f64 Candidates[MaxCapsuleContacts2D] = {NearestToBox_Internal(A, B), 0.0, 1.0};
	for (uint32 Index = 0; Index < MaxCapsuleContacts2D; ++Index)
	{
		FContactPoint2D Hit = FindContact(FCircle2D{AxisPoint_Internal(A, Candidates[Index]), A.Radius}, B);
		Hit.FeatureId = Index;
		AddContact_Internal(Hit, Candidates[Index], Length, Margin, Out, Times, Count);
	}
	return Count;
}
uint32 FindCapsuleContacts(const FCapsule2D& A, const FCapsule2D& B, f32 Margin,
                           FContactPoint2D (&Out)[MaxCapsuleContacts2D])
{
	Validate_Internal(A);
	Validate_Internal(B);
	ValidateMargin_Internal(Margin);
	const f64 Length = AxisLength_Internal(A);
	f64 Times[MaxCapsuleContacts2D]{};
	uint32 Count = 0;
	f64 S = 0;
	f64 T = 0;
	NearestToCapsule_Internal(A, B, S, T);
	const f64 Candidates[MaxCapsuleContacts2D] = {S, 0.0, 1.0};
	for (uint32 Index = 0; Index < MaxCapsuleContacts2D; ++Index)
	{
		const FVector2 OnA = AxisPoint_Internal(A, Candidates[Index]);
		FContactPoint2D Hit =
		    FindContact(FCircle2D{OnA, A.Radius},
		                FCircle2D{Index == 0 ? AxisPoint_Internal(B, T) : ClosestPointOnCapsuleAxis(B, OnA), B.Radius});
		Hit.FeatureId = Index;
		AddContact_Internal(Hit, Candidates[Index], Length, Margin, Out, Times, Count);
	}
	return Count;
}
FShapeContact2D FindShapeContact(const FCircle2D& Shape, const FCapsule2D& Target)
{
	Validate_Internal(Target);
	return FindShapeContact(Shape, FCircle2D{ClosestPointOnCapsuleAxis(Target, Shape.Center), Target.Radius});
}
FShapeContact2D FindShapeContact(const FCapsule2D& Shape, const FCircle2D& Target)
{
	Validate_Internal(Shape);
	return FindShapeContact(FCircle2D{ClosestPointOnCapsuleAxis(Shape, Target.Center), Shape.Radius}, Target);
}
FShapeContact2D FindShapeContact(const FCapsule2D& Shape, const FOrientedBox2D& Target)
{
	Validate_Internal(Shape);
	return FindShapeContact(FCircle2D{AxisPoint_Internal(Shape, NearestToBox_Internal(Shape, Target)), Shape.Radius},
	                        Target);
}
FShapeContact2D FindShapeContact(const FCapsule2D& Shape, const FCapsule2D& Target)
{
	Validate_Internal(Shape);
	Validate_Internal(Target);
	f64 S = 0;
	f64 T = 0;
	NearestToCapsule_Internal(Shape, Target, S, T);
	return FindShapeContact(FCircle2D{AxisPoint_Internal(Shape, S), Shape.Radius},
	                        FCircle2D{AxisPoint_Internal(Target, T), Target.Radius});
}
bool Intersects(const FCircle2D& Circle, const FCapsule2D& Capsule, f32 Tolerance)
{
	if (!IsFinite(Tolerance) || Tolerance < 0)
	{
		throw FException("Invalid 2D capsule overlap tolerance");
	}
	return FindShapeContact(Circle, Capsule).Separation <= Tolerance;
}
} // namespace Toolbox
