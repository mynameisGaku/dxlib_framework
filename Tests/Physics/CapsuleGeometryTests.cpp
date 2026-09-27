// SPDX-License-Identifier: NOASSERTION
// 2D／3Dのカプセルの幾何（S1）。期待値は配置から手で求めた解析値（製品の関数を期待値に使わない）。
// 確認:
// 端の球・胴体・平行・直交・角・接線・内部開始・長さ0・回転した箱・大きな共通の位置・線分・移動（最初の接触）・境界・不正値。
#include "TestCases.h"
#include "Toolbox/CapsuleContact2D.h"
#include "Toolbox/CapsuleContact3D.h"
#include "Toolbox/CapsuleQuery2D.h"
#include "Toolbox/CapsuleQuery3D.h"
#include "Toolbox/ShapeContactQuery2D.h"
#include "Toolbox/ShapeContactQuery3D.h"
using namespace Toolbox;
namespace
{
constexpr f64 Tolerance = 1e-5;

bool Near_Internal(f64 A, f64 B, f64 Epsilon = Tolerance)
{
	return Abs(A - B) <= Epsilon;
}
bool Near_Internal(FVector3 A, FVector3 B, f64 Epsilon = Tolerance)
{
	return Near_Internal(A.X, B.X, Epsilon) && Near_Internal(A.Y, B.Y, Epsilon) && Near_Internal(A.Z, B.Z, Epsilon);
}
bool Near_Internal(FVector2 A, FVector2 B, f64 Epsilon = Tolerance)
{
	return Near_Internal(A.X, B.X, Epsilon) && Near_Internal(A.Y, B.Y, Epsilon);
}
template <typename F> bool Throws_Internal(F&& Run)
{
	try
	{
		Run();
	}
	catch (const FException&)
	{
		return true;
	}
	return false;
}
// Z軸回りに回したOBB。
FOBB Rotated_Internal(FVector3 Center, FVector3 Half, f64 Angle)
{
	FOBB Box{Center, Half};
	const f32 C = static_cast<f32>(Cos(Angle));
	const f32 S = static_cast<f32>(Sin(Angle));
	Box.Axes[0] = {C, S, 0};
	Box.Axes[1] = {-S, C, 0};
	Box.Axes[2] = {0, 0, 1};
	return Box;
}

// 3D: 球・カプセル同士。端の球と胴体、平行・直交・端と胴体。
void Capsule3DBalls_Internal()
{
	const FCapsule Axis{{-1, 0, 0}, {1, 0, 0}, 0.5f};
	// 胴体の上の球：距離2−0.5−0.5=1、法線は球からカプセルへ（−Y）。
	const auto Body = FindContact(Axis, FSphere{{0, 2, 0}, 0.5f});
	PHYSICS_REQUIRE(Near_Internal(Body.Separation, 1.0) && Near_Internal(Body.Normal, {0, -1, 0}));
	// 端の球：中心(3,0,0)は端(1,0,0)から2、距離2−1=1、法線−X。
	const auto Cap = FindContact(Axis, FSphere{{3, 0, 0}, 0.5f});
	PHYSICS_REQUIRE(Near_Internal(Cap.Separation, 1.0) && Near_Internal(Cap.Normal, {-1, 0, 0}));
	// 順序を入れ替えると法線は逆。
	PHYSICS_REQUIRE(Near_Internal(FindContact(FSphere{{0, 2, 0}, 0.5f}, Axis).Normal, {0, 1, 0}));
	// 平行：1.5−1=0.5。
	const FCapsule Above{{-1, 1.5f, 0}, {1, 1.5f, 0}, 0.5f};
	PHYSICS_REQUIRE(Near_Internal(FindContact(Axis, Above).Separation, 0.5));
	FContactPoint3D Points[MaxCapsuleContacts];
	const uint32 Count = FindCapsuleContacts(Axis, Above, 0.6f, Points);
	// 平行では最も近い点がStartと重なり、Startと端の二点になる。
	PHYSICS_REQUIRE(Count == 2 && Points[0].FeatureId == 0 && Points[1].FeatureId == 2);
	PHYSICS_REQUIRE(Near_Internal(Points[1].Position, {1, 0.75f, 0}) && Near_Internal(Points[1].Normal, {0, -1, 0}));
	PHYSICS_REQUIRE(FindCapsuleContacts(Axis, Above, 0.4f, Points) == 0);
	// 直交（Z向き、高さ1.2）：1.2−1=0.2。
	const FCapsule Cross{{0, 1.2f, -1}, {0, 1.2f, 1}, 0.5f};
	const auto Crossed = FindContact(Axis, Cross);
	PHYSICS_REQUIRE(Near_Internal(Crossed.Separation, 0.2) && Near_Internal(Crossed.Normal, {0, -1, 0}));
	// 端と胴体：Aの端(1,0,0)とBの胴体(2,0,0)で接する（距離0）。
	const FCapsule Side{{2, -1, 0}, {2, 1, 0}, 0.5f};
	PHYSICS_REQUIRE(Near_Internal(FindContact(Axis, Side).Separation, 0.0));
	// 形状の問い合わせ（対象から形状へ向く法線）。
	const auto Query = FindShapeContact(FSphere{{0, 2, 0}, 0.5f}, Axis);
	PHYSICS_REQUIRE(Near_Internal(Query.Separation, 1.0) && Query.Normal && Near_Internal(*Query.Normal, {0, 1, 0}));
	// 中心線上の同心：方向は区別できない（法線なし）。
	PHYSICS_REQUIRE(!FindShapeContact(FSphere{{0.3f, 0, 0}, 0.1f}, Axis).Normal);
	// 長さ0は球と同じ。
	const FCapsule Point{{0, 0, 0}, {0, 0, 0}, 0.5f};
	PHYSICS_REQUIRE(Near_Internal(FindContact(Point, FSphere{{0, 2, 0}, 0.5f}).Separation, 1.0));
	PHYSICS_REQUIRE(Near_Internal(FindShapeContact(Point, FSphere{{3, 4, 0}, 0}).Separation, 4.5));
}

// 3D: 箱。胴体の接触（端の球だけでは遠い）、面の上に横たわる二点、回転した箱、角、内部開始。
void Capsule3DBox_Internal()
{
	// 長いカプセルの真下の小さな箱：胴体(0,2,0)から上面0.5まで1.5、距離1.5−0.5=1。
	// 両端(±3,2,0)から箱までは√(2.5²+1.5²)=2.915なので、端だけの判定なら2.415になる。
	const FCapsule Long{{-3, 2, 0}, {3, 2, 0}, 0.5f};
	const FOBB Small{{0, 0, 0}, {0.5f, 0.5f, 0.5f}};
	const auto Body = FindContact(Long, Small);
	PHYSICS_REQUIRE(Near_Internal(Body.Separation, 1.0, 1e-4) && Near_Internal(Body.Normal, {0, 1, 0}));
	PHYSICS_REQUIRE(Near_Internal(FindShapeContact(Long, Small).Separation, 1.0, 1e-4));
	// 床の上に横たわる：軸0.2、上面−0.5、距離0.7−0.5=0.2。両端の二点。
	const FOBB Floor{{0, -1, 0}, {2, 0.5f, 2}};
	const FCapsule Lying{{-1, 0.2f, 0}, {1, 0.2f, 0}, 0.5f};
	FContactPoint3D Points[MaxCapsuleContacts];
	const uint32 Count = FindCapsuleContacts(Lying, Floor, 0.3f, Points);
	PHYSICS_REQUIRE(Count == 2);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		PHYSICS_REQUIRE(Near_Internal(Points[Index].Separation, 0.2, 1e-4) &&
		                Near_Internal(Points[Index].Normal, {0, 1, 0}));
	}
	PHYSICS_REQUIRE(FindCapsuleContacts(Lying, Floor, 0.1f, Points) == 0);
	// 45度回した箱（頂点の高さ0.5√2）と水平のカプセル（高さ2）：2−0.7071−0.5=0.7929。
	const FOBB Diamond = Rotated_Internal({0, 0, 0}, {0.5f, 0.5f, 0.5f}, 0.7853981633974483);
	PHYSICS_REQUIRE(Near_Internal(FindContact(FCapsule{{-2, 2, 0}, {2, 2, 0}, 0.5f}, Diamond).Separation,
	                              2 - 0.5 * 1.4142135623730951 - 0.5, 1e-4));
	// 角：縦のカプセル（x=1、y=1〜3）の下端(1,1,0)から箱の辺(0.5,0.5,z)へ(0.5,0.5,0)、距離√0.5−0.5、法線は斜め45度。
	const auto Corner = FindContact(FCapsule{{1, 1, 0}, {1, 3, 0}, 0.5f}, Small);
	PHYSICS_REQUIRE(Near_Internal(Corner.Separation, Sqrt(0.5) - 0.5, 1e-4));
	PHYSICS_REQUIRE(Near_Internal(Corner.Normal, {static_cast<f32>(Sqrt(0.5)), static_cast<f32>(Sqrt(0.5)), 0}, 1e-4));
	// 内部開始：中心線が箱を貫く。最も深い点は箱の中心で、上面までの0.5と半径で−1。
	PHYSICS_REQUIRE(FindContact(FCapsule{{-3, 0, 0}, {3, 0, 0}, 0.5f}, Small).Separation < -0.99f);
	// 大きな共通の位置（1e4）でも同じ距離（f32の丸めの範囲）。
	const FVector3 Far{1e4f, 1e4f, 1e4f};
	const FCapsule FarLong{Long.Start + Far, Long.End + Far, 0.5f};
	const FOBB FarSmall{Small.Center + Far, Small.HalfExtents};
	PHYSICS_REQUIRE(Near_Internal(FindContact(FarLong, FarSmall).Separation, 1.0, 5e-3));
}

// 3D: 線分と移動。
void Capsule3DQueries_Internal()
{
	const FCapsule Axis{{-1, 0, 0}, {1, 0, 0}, 0.5f};
	// 端の球：x=−1−√(0.25−0.04)、割合(−1.4583+5)/10。
	const f64 Cap = (-1 - Sqrt(0.25 - 0.04) + 5) / 10;
	PHYSICS_REQUIRE(Near_Internal(*IntersectSegment(FVector3{-5, 0.2f, 0}, FVector3{5, 0.2f, 0}, Axis), Cap));
	// 胴体：y=0.5で、割合0.45。
	PHYSICS_REQUIRE(Near_Internal(*IntersectSegment(FVector3{0.3f, 5, 0}, FVector3{0.3f, -5, 0}, Axis), 0.45));
	// 内部開始は0、外れは空、接線（ちょうど半径）は当たる。
	PHYSICS_REQUIRE(*IntersectSegment(FVector3{0, 0.1f, 0}, FVector3{0, 5, 0}, Axis) == 0);
	PHYSICS_REQUIRE(!IntersectSegment(FVector3{-5, 2, 0}, FVector3{5, 2, 0}, Axis));
	PHYSICS_REQUIRE(IntersectSegment(FVector3{-5, 0.5f, 0}, FVector3{5, 0.5f, 0}, Axis));
	// 球の移動：中心が高さ1で接触、割合(5−1)/10=0.4、法線+Y。
	const auto Sphere = SweepToCenter(FSphere{{0, 5, 0}, 0.5f}, FVector3{0, -5, 0}, Axis);
	PHYSICS_REQUIRE(Sphere && !Sphere->bInitialContact && Near_Internal(Sphere->Time, 0.4) && Sphere->Normal &&
	                Near_Internal(*Sphere->Normal, {0, 1, 0}));
	PHYSICS_REQUIRE(SweepToCenter(FSphere{{0, 0.8f, 0}, 0.5f}, FVector3{0, 5, 0}, Axis)->bInitialContact);
	// カプセルの移動：縦(0,1..2)半径0.5を中点(0,1.5)から(0,−5)へ、床の上面0。下端の球の底0.5が0になるまで0.5／6.5。
	const FOBB Floor{{0, -1, 0}, {5, 1, 5}};
	const auto Drop = SweepToCenter(FCapsule{{0, 1, 0}, {0, 2, 0}, 0.5f}, FVector3{0, -5, 0}, Floor);
	PHYSICS_REQUIRE(Drop && !Drop->bInitialContact && Near_Internal(Drop->Time, 0.5 / 6.5, 1e-4) && Drop->Normal &&
	                Near_Internal(*Drop->Normal, {0, 1, 0}, 1e-4));
	// 当たらない移動と、開始時の接触。
	PHYSICS_REQUIRE(!SweepToCenter(FCapsule{{0, 1, 0}, {0, 2, 0}, 0.5f}, FVector3{0, 5, 0}, Floor));
	PHYSICS_REQUIRE(SweepToCenter(FCapsule{{0, 0.2f, 0}, {0, 2, 0}, 0.5f}, FVector3{0, 5, 0}, Floor)->bInitialContact);
	// カプセル同士の移動：横向きのカプセルを右へ4。Bの胴体x=2ならAの右端1＋半径0.5＋0.5で開始時に接し、x=3なら1だけ進んだ1／4で接する。
	const auto Side = SweepToCenter(Axis, FVector3{4, 0, 0}, FCapsule{{2, -1, 0}, {2, 1, 0}, 0.5f});
	PHYSICS_REQUIRE(Side && Near_Internal(Side->Time, 0.0, 1e-4));
	const auto Gap = SweepToCenter(Axis, FVector3{4, 0, 0}, FCapsule{{3, -1, 0}, {3, 1, 0}, 0.5f});
	PHYSICS_REQUIRE(Gap && Near_Internal(Gap->Time, 1.0 / 4.0, 1e-4) && Near_Internal(*Gap->Normal, {-1, 0, 0}, 1e-4));
	// 境界は半径を含む。
	const FAABB Bounds = CapsuleBounds(FCapsule{{-1, 2, 3}, {1, -2, 3}, 0.5f});
	PHYSICS_REQUIRE(Near_Internal(Bounds.Min, {-1.5f, -2.5f, 2.5f}) && Near_Internal(Bounds.Max, {1.5f, 2.5f, 3.5f}));
	// 不正な値は例外。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)FindContact(FCapsule{{0, 0, 0}, {1, 0, 0}, -1}, FSphere{{0, 0, 0}, 1});
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)IntersectSegment(FVector3{0, 0, 0}, FVector3{1, 0, 0}, FCapsule{{0, NAN, 0}, {1, 0, 0}, 0.5f});
	    }));
	PHYSICS_REQUIRE(!IsValid(FCapsule{{0, 0, 0}, {INFINITY, 0, 0}, 0.5f}) &&
	                IsValid(FCapsule{{1, 1, 1}, {1, 1, 1}, 0}));
}

// 2D: 3Dと同じ配置（Z=0の平面）で同じ意味。
void Capsule2D_Internal()
{
	const FCapsule2D Axis{{-1, 0}, {1, 0}, 0.5f};
	const auto Body = FindContact(Axis, FCircle2D{{0, 2}, 0.5f});
	PHYSICS_REQUIRE(Near_Internal(Body.Separation, 1.0) && Near_Internal(Body.Normal, {0, -1}));
	PHYSICS_REQUIRE(Near_Internal(FindContact(Axis, FCircle2D{{3, 0}, 0.5f}).Separation, 1.0));
	const FCapsule2D Above{{-1, 1.5f}, {1, 1.5f}, 0.5f};
	FContactPoint2D Points[MaxCapsuleContacts2D];
	PHYSICS_REQUIRE(FindCapsuleContacts(Axis, Above, 0.6f, Points) == 2);
	PHYSICS_REQUIRE(Near_Internal(FindContact(Axis, FCapsule2D{{2, -1}, {2, 1}, 0.5f}).Separation, 0.0));
	// 胴体と矩形：端だけなら遠い。
	const FCapsule2D Long{{-3, 2}, {3, 2}, 0.5f};
	const FOrientedBox2D Small{{0, 0}, {0.5f, 0.5f}, 0};
	PHYSICS_REQUIRE(Near_Internal(FindContact(Long, Small).Separation, 1.0, 1e-4));
	const FOrientedBox2D Diamond{{0, 0}, {0.5f, 0.5f}, 0.7853981633974483f};
	PHYSICS_REQUIRE(Near_Internal(FindContact(FCapsule2D{{-2, 2}, {2, 2}, 0.5f}, Diamond).Separation,
	                              2 - 0.5 * 1.4142135623730951 - 0.5, 1e-4));
	const FOrientedBox2D Floor{{0, -1}, {2, 0.5f}, 0};
	PHYSICS_REQUIRE(FindCapsuleContacts(FCapsule2D{{-1, 0.2f}, {1, 0.2f}, 0.5f}, Floor, 0.3f, Points) == 2);
	PHYSICS_REQUIRE(Near_Internal(Points[0].Normal, {0, 1}, 1e-4));
	// 線分と移動。
	PHYSICS_REQUIRE(Near_Internal(*IntersectSegment(FVector2{0.3f, 5}, FVector2{0.3f, -5}, Axis), 0.45));
	PHYSICS_REQUIRE(Near_Internal(*IntersectSegment(FVector2{-5, 0.2f}, FVector2{5, 0.2f}, Axis),
	                              (-1 - Sqrt(0.25 - 0.04) + 5) / 10));
	const auto Circle = SweepToCenter(FCircle2D{{0, 5}, 0.5f}, FVector2{0, -5}, Axis);
	PHYSICS_REQUIRE(Circle && Near_Internal(Circle->Time, 0.4) && Near_Internal(*Circle->Normal, {0, 1}));
	const auto Drop =
	    SweepToCenter(FCapsule2D{{0, 1}, {0, 2}, 0.5f}, FVector2{0, -5}, FOrientedBox2D{{0, -1}, {5, 1}, 0});
	PHYSICS_REQUIRE(Drop && Near_Internal(Drop->Time, 0.5 / 6.5, 1e-4));
	const FAABB2D Bounds = CapsuleBounds(FCapsule2D{{-1, 2}, {1, -2}, 0.5f});
	PHYSICS_REQUIRE(Near_Internal(Bounds.Min, {-1.5f, -2.5f}) && Near_Internal(Bounds.Max, {1.5f, 2.5f}));
	PHYSICS_REQUIRE(!FindShapeContact(FCircle2D{{0.3f, 0}, 0.1f}, Axis).Normal);
}

const PhysicsTest::FCase Cases_Internal[] = {{"3D capsule balls and capsules", &Capsule3DBalls_Internal},
                                             {"3D capsule boxes use the body", &Capsule3DBox_Internal},
                                             {"3D capsule segment and sweeps", &Capsule3DQueries_Internal},
                                             {"2D capsule geometry matches 3D meaning", &Capsule2D_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetCapsuleGeometryCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
