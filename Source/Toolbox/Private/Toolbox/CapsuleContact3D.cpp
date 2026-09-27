// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/CapsuleContact3D.h"
#include "Toolbox/ShapeContactQuery3D.h"
#include "CapsuleMath.h"
namespace Toolbox
{
namespace
{
using CapsulePrivate::FPoint;

// 公開型の点を倍精度へ。
FPoint Load_Internal(FVector3 Value) noexcept
{
	return {Value.X, Value.Y, Value.Z};
}
// 倍精度の点を公開型へ。
FVector3 Store_Internal(const FPoint& Value) noexcept
{
	return {static_cast<f32>(Value.X), static_cast<f32>(Value.Y), static_cast<f32>(Value.Z)};
}
// カプセルの値を検査する。
void Validate_Internal(const FCapsule& Capsule)
{
	if (!IsValid(Capsule))
	{
		throw FException("Invalid 3D capsule");
	}
}
// 中心線上のパラメーターの点。
FVector3 AxisPoint_Internal(const FCapsule& Capsule, f64 Time) noexcept
{
	const FPoint A = Load_Internal(Capsule.Start);
	const FPoint B = Load_Internal(Capsule.End);
	return Store_Internal(CapsulePrivate::Add(A, CapsulePrivate::Scale(CapsulePrivate::Sub(B, A), Time)));
}
// 中心線上の、箱までの符号付き距離が最小のパラメーター（点と箱の符号付き距離は凸なので、胴体を含めて探索できる）。
f64 NearestToBox_Internal(const FCapsule& Capsule, const FOBB& Box)
{
	return CapsulePrivate::MinimizeConvex(
	    [&](f64 Time)
	    {
		    return FindShapeContact(FSphere{AxisPoint_Internal(Capsule, Time), 0}, Box).Separation;
	    });
}
// 線分同士の最も近い点のパラメーター。
void NearestToCapsule_Internal(const FCapsule& A, const FCapsule& B, f64& S, f64& T) noexcept
{
	CapsulePrivate::ClosestParams(Load_Internal(A.Start), Load_Internal(A.End), Load_Internal(B.Start),
	                              Load_Internal(B.End), S, T);
}
// 接触点の候補を、重複を除いて加える。
void AddContact_Internal(const FContactPoint3D& Hit, f64 Time, f64 AxisLength, f32 Margin,
                         FContactPoint3D (&Out)[MaxCapsuleContacts], f64 (&Times)[MaxCapsuleContacts], uint32& Count)
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
f64 AxisLength_Internal(const FCapsule& Capsule) noexcept
{
	const FPoint D = CapsulePrivate::Sub(Load_Internal(Capsule.End), Load_Internal(Capsule.Start));
	return Sqrt(CapsulePrivate::Dot(D, D));
}
// 余裕の値を検査する。
void ValidateMargin_Internal(f32 Margin)
{
	if (!IsFinite(Margin) || Margin < 0)
	{
		throw FException("Invalid 3D capsule contact margin");
	}
}
} // namespace

FContactPoint3D FindContact(const FCapsule& A, const FSphere& B)
{
	Validate_Internal(A);
	return FindContact(FSphere{ClosestPointOnCapsuleAxis(A, B.Center), A.Radius}, B);
}
FContactPoint3D FindContact(const FSphere& A, const FCapsule& B)
{
	Validate_Internal(B);
	return FindContact(A, FSphere{ClosestPointOnCapsuleAxis(B, A.Center), B.Radius});
}
FContactPoint3D FindContact(const FCapsule& A, const FOBB& B)
{
	Validate_Internal(A);
	return FindContact(FSphere{AxisPoint_Internal(A, NearestToBox_Internal(A, B)), A.Radius}, B);
}
FContactPoint3D FindContact(const FCapsule& A, const FCapsule& B)
{
	Validate_Internal(A);
	Validate_Internal(B);
	f64 S = 0;
	f64 T = 0;
	NearestToCapsule_Internal(A, B, S, T);
	return FindContact(FSphere{AxisPoint_Internal(A, S), A.Radius}, FSphere{AxisPoint_Internal(B, T), B.Radius});
}
uint32 FindCapsuleContacts(const FCapsule& A, const FOBB& B, f32 Margin, FContactPoint3D (&Out)[MaxCapsuleContacts])
{
	Validate_Internal(A);
	ValidateMargin_Internal(Margin);
	const f64 Length = AxisLength_Internal(A);
	f64 Times[MaxCapsuleContacts]{};
	uint32 Count = 0;
	// 最も近い点、Start、Endの順（FeatureIdは0・1・2）。
	const f64 Candidates[MaxCapsuleContacts] = {NearestToBox_Internal(A, B), 0.0, 1.0};
	for (uint32 Index = 0; Index < MaxCapsuleContacts; ++Index)
	{
		FContactPoint3D Hit = FindContact(FSphere{AxisPoint_Internal(A, Candidates[Index]), A.Radius}, B);
		Hit.FeatureId = Index;
		AddContact_Internal(Hit, Candidates[Index], Length, Margin, Out, Times, Count);
	}
	return Count;
}
uint32 FindCapsuleContacts(const FCapsule& A, const FCapsule& B, f32 Margin, FContactPoint3D (&Out)[MaxCapsuleContacts])
{
	Validate_Internal(A);
	Validate_Internal(B);
	ValidateMargin_Internal(Margin);
	const f64 Length = AxisLength_Internal(A);
	f64 Times[MaxCapsuleContacts]{};
	uint32 Count = 0;
	f64 S = 0;
	f64 T = 0;
	NearestToCapsule_Internal(A, B, S, T);
	const f64 Candidates[MaxCapsuleContacts] = {S, 0.0, 1.0};
	for (uint32 Index = 0; Index < MaxCapsuleContacts; ++Index)
	{
		const FVector3 OnA = AxisPoint_Internal(A, Candidates[Index]);
		FContactPoint3D Hit =
		    FindContact(FSphere{OnA, A.Radius},
		                FSphere{Index == 0 ? AxisPoint_Internal(B, T) : ClosestPointOnCapsuleAxis(B, OnA), B.Radius});
		Hit.FeatureId = Index;
		AddContact_Internal(Hit, Candidates[Index], Length, Margin, Out, Times, Count);
	}
	return Count;
}
FShapeContact3D FindShapeContact(const FSphere& Shape, const FCapsule& Target)
{
	Validate_Internal(Target);
	return FindShapeContact(Shape, FSphere{ClosestPointOnCapsuleAxis(Target, Shape.Center), Target.Radius});
}
FShapeContact3D FindShapeContact(const FCapsule& Shape, const FSphere& Target)
{
	Validate_Internal(Shape);
	return FindShapeContact(FSphere{ClosestPointOnCapsuleAxis(Shape, Target.Center), Shape.Radius}, Target);
}
FShapeContact3D FindShapeContact(const FCapsule& Shape, const FOBB& Target)
{
	Validate_Internal(Shape);
	return FindShapeContact(FSphere{AxisPoint_Internal(Shape, NearestToBox_Internal(Shape, Target)), Shape.Radius},
	                        Target);
}
FShapeContact3D FindShapeContact(const FCapsule& Shape, const FCapsule& Target)
{
	Validate_Internal(Shape);
	Validate_Internal(Target);
	f64 S = 0;
	f64 T = 0;
	NearestToCapsule_Internal(Shape, Target, S, T);
	return FindShapeContact(FSphere{AxisPoint_Internal(Shape, S), Shape.Radius},
	                        FSphere{AxisPoint_Internal(Target, T), Target.Radius});
}
bool IntersectsSphere(const FSphere& Sphere, const FCapsule& Capsule, f32 Tolerance)
{
	if (!IsFinite(Tolerance) || Tolerance < 0)
	{
		throw FException("Invalid 3D capsule overlap tolerance");
	}
	return FindShapeContact(Sphere, Capsule).Separation <= Tolerance;
}
} // namespace Toolbox
