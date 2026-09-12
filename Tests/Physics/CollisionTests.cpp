// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Toolbox/CollisionShapes.h"
using namespace Toolbox;
namespace
{
// 引数の順序によって形状の交差結果が変わらないことも同時に確認する。
void CheckPair_Internal(const FCollisionShape& A, const FCollisionShape& B, bool bExpected, f32 Tolerance = 1e-5f)
{
	PHYSICS_REQUIRE(Intersects(A, B, Tolerance) == bExpected);
	PHYSICS_REQUIRE(Intersects(B, A, Tolerance) == bExpected);
}
// 六面の端を含む格子上で、球の厳密な面接触を検査する。
void FaceTangencies_Internal()
{
	const FAABB Box{{-1, -1, -1}, {1, 1, 1}};
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (int32 Sign = -1; Sign <= 1; Sign += 2)
		{
			for (int32 I = -8; I <= 8; ++I)
			{
				for (int32 J = -8; J <= 8; ++J)
				{
					FVector3 Center{};
					// FVector3の別メンバーを配列として辿らず、軸ごとに値を構築する。
					const f32 U = static_cast<f32>(I) * 0.125f;
					const f32 V = static_cast<f32>(J) * 0.125f;
					const f32 Face = 2.0f * static_cast<f32>(Sign);
					Center =
					    Axis == 0 ? FVector3{Face, U, V} : (Axis == 1 ? FVector3{U, Face, V} : FVector3{U, V, Face});
					CheckPair_Internal(FSphere{Center, 1}, Box, true);
				}
			}
		}
	}
}
// 明らかな非交差を許容誤差の拡大で隠さない。
void SeparatedCorners_Internal()
{
	const FAABB Box{{-1, -1, -1}, {1, 1, 1}};
	CheckPair_Internal(FSphere{{2, 2, 2}, 1.5f}, Box, false);
	CheckPair_Internal(FSphere{{2, 2, 2}, 2.0f}, Box, true);
	CheckPair_Internal(FSphere{{2, 0, 0}, 0.5f}, Box, false);
}
// 許容幅の内外を、二進数で厳密に表せる値で検査する。
void ToleranceBand_Internal()
{
	const FAABB Box{{-1, -1, -1}, {1, 1, 1}};
	CheckPair_Internal(FSphere{{2.0625f, 0.25f, -0.5f}, 1}, Box, true, 0.0625f);
	CheckPair_Internal(FSphere{{2.125f, 0.25f, -0.5f}, 1}, Box, false, 0.0625f);
}
// 点の球、厚さゼロの箱、箱内部の球にも境界を含む契約を適用する。
void DegenerateShapes_Internal()
{
	const FAABB Plane{{-1, -1, 0}, {1, 1, 0}};
	CheckPair_Internal(FSphere{{0.25f, 0.5f, 0}, 0}, Plane, true);
	CheckPair_Internal(FSphere{{0.25f, 0.5f, 1}, 1}, Plane, true);
	CheckPair_Internal(FSphere{{0.25f, 0.5f, 2}, 1}, Plane, false);
	CheckPair_Internal(FSphere{{0, 0, 0}, 0}, FAABB{{-1, -1, -1}, {1, 1, 1}}, true);
}
// 同じ箱を別の公開型で表しても接触が失われない。
void OrientedBoxes_Internal()
{
	const FSphere Sphere{{-2, -1, -0.875f}, 1};
	CheckPair_Internal(Sphere, FOBB{{}, {1, 1, 1}}, true);
	CheckPair_Internal(Sphere, FCube{{}, 1}, true);
	// 回転軸を含め、表現された平行六面体の面に接する球。
	FOBB Rotated;
	Rotated.HalfExtents = {1, 2, 1};
	Rotated.Axes = {FVector3{0.6f, 0.8f, 0}, FVector3{-0.8f, 0.6f, 0}, FVector3{0, 0, 1}};
	CheckPair_Internal(FSphere{{1.2f, 1.6f, 0.5f}, 1}, Rotated, true);
	CheckPair_Internal(FSphere{{3, 4, 0}, 1}, Rotated, false);
}
// f32で表せる入力から差を取る前にf64へ広げる必要がある。
void LargeCoordinates_Internal()
{
	const FAABB Box{{-1e20f, -1e20f, -1e20f}, {1e20f, 1e20f, 1e20f}};
	CheckPair_Internal(FSphere{{2e20f, 0, 0}, 1e20f}, Box, true);
	CheckPair_Internal(FSphere{{2e20f, 2e20f, 0}, 1e20f}, Box, false);
	CheckPair_Internal(FSphere{{2e20f, 2e20f, 0}, 1e20f}, FSphere{{0, 0, 0}, 1e20f}, false);
}
// f32の広域境界を膨張させるだけでは近接判定の精度を保証できない。
void LargeOffsetSmallGap_Internal()
{
	const FAABB Box{{99999992.0f, -1, -1}, {100000000.0f, 1, 1}};
	CheckPair_Internal(FSphere{{100000008.0f, 0, 0}, 8.0f}, Box, true);
	CheckPair_Internal(FSphere{{100000008.0f, 0, 0}, 7.99f}, Box, false);
}
// 一枚の三角形メッシュの表面判定を維持する。
void MeshRegression_Internal()
{
	const FMesh Triangle({{-2, -2, 0}, {2, -2, 0}, {0, 2, 0}}, {0, 1, 2});
	CheckPair_Internal(FSphere{{0, 0, 0.1f}, 0.5f}, Triangle, true);
	CheckPair_Internal(FSphere{{0, 0, 3}, 0.5f}, Triangle, false);
}
// 不正な入力を単なる非交差として隠さない。
void InvalidInputs_Internal()
{
	bool bThrown = false;
	try
	{
		(void)Intersects(FSphere{{}, -1}, FAABB{{-1, -1, -1}, {1, 1, 1}});
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
	bThrown = false;
	try
	{
		(void)Intersects(FSphere{}, FSphere{}, TNumericLimits<f32>::QuietNaN());
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
}
// 既存のOptionalとVariantの構築制約を実体化するGCC回帰ケース。
void Constructibility_Internal()
{
	TOptional<FVector3> A{FVector3{1, 2, 3}};
	TOptional<FVector3> B{A};
	B = A;
	PHYSICS_REQUIRE(B->X == 1);
	FCollisionShape Shape = FSphere{{}, 2};
	FCollisionShape Copy(Shape);
	Copy = Shape;
	PHYSICS_REQUIRE(Get<FSphere>(Copy).Radius == 2);
}
const PhysicsTest::FCase Cases[] = {
    {"sphere touches 1734 face-grid positions in both orders", FaceTangencies_Internal},
    {"sphere corner separation uses Euclidean distance", SeparatedCorners_Internal},
    {"sphere box tolerance band", ToleranceBand_Internal},
    {"zero-radius sphere and zero-thickness box", DegenerateShapes_Internal},
    {"sphere OBB and cube contacts", OrientedBoxes_Internal},
    {"finite large coordinates do not overflow distances", LargeCoordinates_Internal},
    {"large offset preserves a small real gap", LargeOffsetSmallGap_Internal},
    {"mesh surface regression", MeshRegression_Internal},
    {"invalid collision inputs are rejected", InvalidInputs_Internal},
    {"constructibility constraints remain copyable", Constructibility_Internal},
};
} // namespace
namespace PhysicsTest
{
const FCase* GetCollisionCases(size_t& Count) noexcept
{
	Count = sizeof(Cases) / sizeof(Cases[0]);
	return Cases;
}
} // namespace PhysicsTest
