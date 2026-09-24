// SPDX-License-Identifier: NOASSERTION
// 円／球スイープ（SweepClosest）の幾何・World接続・ゲーム利用。
// 幾何の期待値は手計算の解析値で、製品の変換・距離関数から作らない。実FPhysicsWorld2D／3Dの両方で実行する。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/JobSystem.h"
#include "Toolbox/ShapeSweep2D.h"
#include "Toolbox/ShapeSweep3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr uint32 ObstacleCategory = 1u << 0;
constexpr uint32 CharacterCategory = 1u << 1;
constexpr uint32 PickupCategory = 1u << 2;
constexpr uint32 HighCategory = 0x80000000u;
constexpr f32 HalfPi = 1.57079632679f;
constexpr f32 SixthPi = 0.523598775598f;

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

bool Near_Internal(f64 Value, f64 Expected, f64 Tolerance)
{
	return Abs(Value - Expected) <= Tolerance;
}

FWorldQueryFilter Mask_Internal(uint32 Categories)
{
	FWorldQueryFilter Filter;
	Filter.IncludeCategories = Categories;
	return Filter;
}

// 2D Worldの型と登録操作。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FColliderId = FColliderId2D;
	using FVector = FVector2;
	using FProbe = FCircle2D;
	using FHit = FWorldSweepHit2D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static FProbe Probe(FVector Center, f32 Radius)
	{
		return {Center, Radius};
	}
	static f32 X(FVector Value)
	{
		return Value.X;
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type = EBodyType::Static, f32 Angle = 0)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = Position;
		Description.Angle = Angle;
		return World.CreateBody(Description);
	}
	static FColliderId Ball(FWorld& World, FBodyId Body, FVector Center, f32 Radius, uint32 Category = 1u)
	{
		FColliderDescription2D Description;
		Description.Shape = FCircle2D{Center, Radius};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, uint32 Category = 1u)
	{
		FColliderDescription2D Description;
		Description.Shape = FOrientedBox2D{Center, {HalfX, HalfY}, 0};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static void Move(FWorld& World, FBodyId Body, FVector Position, f32 Angle)
	{
		World.SetBodyTransform(Body, Position, Angle);
	}
	static void Velocity(FWorld& World, FBodyId Body, f32 Speed)
	{
		World.SetVelocity(Body, {Speed, Speed});
		World.SetAngularVelocity(Body, Speed);
	}
	static bool SameState(const FWorld& A, FBodyId BodyA, const FWorld& B, FBodyId BodyB)
	{
		return A.GetPosition(BodyA) == B.GetPosition(BodyB) && A.GetAngle(BodyA) == B.GetAngle(BodyB) &&
		       A.GetVelocity(BodyA) == B.GetVelocity(BodyB) &&
		       A.GetAngularVelocity(BodyA) == B.GetAngularVelocity(BodyB) && A.IsSleeping(BodyA) == B.IsSleeping(BodyB);
	}
	static void Push(FWorld& World, FBodyId Body)
	{
		World.ApplyForce(Body, {2, 3});
		World.ApplyTorque(Body, 1.5f);
	}
	static void NoGravity(FWorld& World)
	{
		World.SetGravity({0, 0});
	}
};

// 3D Worldの型と登録操作。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FProbe = FSphere;
	using FHit = FWorldSweepHit3D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static FProbe Probe(FVector Center, f32 Radius)
	{
		return {Center, Radius};
	}
	static f32 X(FVector Value)
	{
		return Value.X;
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type = EBodyType::Static, f32 Angle = 0)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = Position;
		Description.Orientation = FQuaternion::FromAxisAngle({0, 0, 1}, Angle);
		return World.CreateBody(Description);
	}
	static FColliderId Ball(FWorld& World, FBodyId Body, FVector Center, f32 Radius, uint32 Category = 1u)
	{
		FColliderDescription3D Description;
		Description.Shape = FSphere{Center, Radius};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, uint32 Category = 1u)
	{
		FColliderDescription3D Description;
		Description.Shape = FOBB{Center, {HalfX, HalfY, HalfX > HalfY ? HalfX : HalfY}};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static void Move(FWorld& World, FBodyId Body, FVector Position, f32 Angle)
	{
		World.SetBodyTransform(Body, Position, FQuaternion::FromAxisAngle({0, 0, 1}, Angle));
	}
	static void Velocity(FWorld& World, FBodyId Body, f32 Speed)
	{
		World.SetVelocity(Body, {Speed, Speed, Speed});
		World.SetAngularVelocity(Body, {Speed, 0, Speed});
	}
	static bool SameState(const FWorld& A, FBodyId BodyA, const FWorld& B, FBodyId BodyB)
	{
		const FQuaternion RotationA = A.GetOrientation(BodyA);
		const FQuaternion RotationB = B.GetOrientation(BodyB);
		return A.GetPosition(BodyA) == B.GetPosition(BodyB) && RotationA.X == RotationB.X &&
		       RotationA.Y == RotationB.Y && RotationA.Z == RotationB.Z && RotationA.W == RotationB.W &&
		       A.GetVelocity(BodyA) == B.GetVelocity(BodyB) &&
		       A.GetAngularVelocity(BodyA) == B.GetAngularVelocity(BodyB) && A.IsSleeping(BodyA) == B.IsSleeping(BodyB);
	}
	static void Push(FWorld& World, FBodyId Body)
	{
		World.ApplyForce(Body, {2, 3, 4});
		World.ApplyTorque(Body, {1, 2, 3});
	}
	static void NoGravity(FWorld& World)
	{
		World.SetGravity({0, 0, 0});
	}
};

// 結果の割合と初期接触を確認する。
template <typename THit> bool Hit_Internal(const TOptional<THit>& Hit, f64 Fraction, bool bInitial, f64 Tolerance)
{
	return Hit && Near_Internal(Hit->Time, Fraction, Tolerance) && Hit->bInitialContact == bInitial;
}

// 円／球同士の移動（G01〜G07の幾何部分、G18）。2Dと3Dで同じ数値を独立に確認する。
void CircleSphereMath_Internal()
{
	// G01: 対象(5,0)半径1、問い合わせ半径0.5で0→10。中心間1.5で接触し、中心x=3.5、割合0.35。
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 0}, 1}), 0.35,
	                             false, 1e-12));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{5, 0, 0}, 1}),
	                             0.35, false, 1e-12));
	// G02: 開始(2,0)、終点(10,0)。移動8のうち1.5で接触し0.1875。終点を変位と取り違えると値が変わる。
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FCircle2D{{2, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 0}, 1}), 0.1875,
	                             false, 1e-12));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{2, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{5, 0, 0}, 1}),
	                             0.1875, false, 1e-12));
	// G03: 対象(5,1.5)は途中で接線接触（割合0.5）。1.5+2^-10なら非交差。
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 1.5f}, 1}), 0.5,
	                             false, 1e-12));
	PHYSICS_REQUIRE(!SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 1.5f + 1.0f / 1024}, 1}));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{5, 0, 1.5f}, 1}),
	                             0.5, false, 1e-12));
	PHYSICS_REQUIRE(
	    !SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{5, 0, 1.5f + 1.0f / 1024}, 1}));
	// G04: 終点でちょうど接触（割合1）。Time=1を非交差とみなす誤りを検出する。
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{11.5f, 0}, 1}), 1, false, 0));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{11.5f, 0, 0}, 1}),
	                             1, false, 0));
	// G05: 開始時の重なり・境界接触は、離れる向きでも割合0の初期接触。
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{0.5f, 0}, 0.5f}, FVector2{-10, 0}, FCircle2D{{0, 0}, 1}), 0, true, 0));
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{1.5f, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{0, 0}, 1}), 0, true, 0));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{1.5f, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{0, 0, 0}, 1}),
	                             0, true, 0));
	// G06: 開始＝終点。外部は空、接触・内部は0。
	PHYSICS_REQUIRE(!SweepToCenter(FCircle2D{{2, 0}, 0.5f}, FVector2{2, 0}, FCircle2D{{0, 0}, 1}));
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{1.5f, 0}, 0.5f}, FVector2{1.5f, 0}, FCircle2D{{0, 0}, 1}), 0, true, 0));
	PHYSICS_REQUIRE(Hit_Internal(
	    SweepToCenter(FSphere{{0.2f, 0, 0}, 0.5f}, FVector3{0.2f, 0, 0}, FSphere{{0, 0, 0}, 1}), 0, true, 0));
	PHYSICS_REQUIRE(!SweepToCenter(FSphere{{2, 0, 0}, 0.5f}, FVector3{2, 0, 0}, FSphere{{0, 0, 0}, 1}));
	// G07の幾何部分: 半径0・ゼロ移動は点の重なり。
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{0.5f, 0}, 0}, FVector2{0.5f, 0}, FCircle2D{{0, 0}, 1}), 0, true, 0));
	PHYSICS_REQUIRE(!SweepToCenter(FSphere{{1.5f, 0, 0}, 0}, FVector3{1.5f, 0, 0}, FSphere{{0, 0, 0}, 1}));
	// 半径0の対象円／球（点）。
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 0}, 0}), 0.45,
	                             false, 1e-12));
	// G18: 大きい共通オフセット（2^20）でも同じ割合。
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{1048576, 0}, 0.5f}, FVector2{1048586, 0}, FCircle2D{{1048581, 0}, 1}),
	                 0.35, false, 1e-12));
	PHYSICS_REQUIRE(Hit_Internal(
	    SweepToCenter(FSphere{{0, 0, 1048576}, 0.5f}, FVector3{10, 0, 1048576}, FSphere{{5, 0, 1048576}, 1}), 0.35,
	    false, 1e-12));
	// G18: 長い移動と小さい半径。対象(0, y)半径r、問い合わせ半径rの接触はx=-sqrt((2r)^2-y^2)。
	const f64 Radius = f64(1e-3f);
	const f64 Offset = f64(1.5e-3f);
	const f64 Expected = (1e5 - Sqrt(4 * Radius * Radius - Offset * Offset)) / 2e5;
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{-1e5f, 0}, 1e-3f}, FVector2{1e5f, 0}, FCircle2D{{0, 1.5e-3f}, 1e-3f}),
	                 Expected, false, 1e-12));
	PHYSICS_REQUIRE(!SweepToCenter(FCircle2D{{-1e5f, 0}, 1e-3f}, FVector2{1e5f, 0}, FCircle2D{{0, 2.5e-3f}, 1e-3f}));
	// 無効値と、f32で表現できない移動は例外。
	const f32 Max = TNumericLimits<f32>::Max();
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)SweepToCenter(FCircle2D{{0, 0}, -1}, FVector2{1, 0}, FCircle2D{{5, 0}, 1});
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)SweepToCenter(FSphere{{0, 0, 0}, 1}, FVector3{TNumericLimits<f32>::QuietNaN(), 0, 0},
		                        FSphere{{5, 0, 0}, 1});
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)SweepToCenter(FCircle2D{{-Max, 0}, 1}, FVector2{Max, 0}, FCircle2D{{5, 0}, 1});
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)SweepToCenter(FSphere{{0, 0, 0}, 1}, FVector3{1, 0, 0}, FSphere{{5, 0, 0}, -1});
	    }));
}

// 箱の角・辺・面、回転、実際の軸、退化形状、薄い障害物（G08〜G13、G15〜G17）。
void BoxMath_Internal()
{
	// G08: 2D箱[-1,1]^2、半径1、(3,3)→(1,1)。角(1,1)まで√2(2-2t)=1。膨張AABBなら0.5になる。
	const FOrientedBox2D Square{{0, 0}, {1, 1}, 0};
	const f64 Corner2D = 1 - 1 / (2 * Sqrt(2.0));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FCircle2D{{3, 3}, 1}, FVector2{1, 1}, Square), Corner2D, false, 1e-9));
	// G09: 角の外側を通る。膨張矩形なら初期接触になる位置。
	PHYSICS_REQUIRE(!SweepToCenter(FCircle2D{{1.8f, 1.8f}, 1}, FVector2{2, 1.8f}, Square));
	// G10: 3D箱[-1,1]^3、半径1、(3,3,3)→(1,1,1)。頂点まで√3(2-2t)=1。
	const FOBB Cube{{0, 0, 0}, {1, 1, 1}};
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{3, 3, 3}, 1}, FVector3{1, 1, 1}, Cube), 1 - 1 / (2 * Sqrt(3.0)),
	                             false, 1e-9));
	// G11: 辺(1,1,z)へ。G08と同じ割合。
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{3, 3, 0}, 1}, FVector3{1, 1, 0}, Cube), Corner2D, false, 1e-9));
	// G12: 面z=-1へ半径0.5で。中心z=-1.5、割合0.25。
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FSphere{{0, 0, -3}, 0.5f}, FVector3{0, 0, 3}, Cube), 0.25, false, 1e-12));
	// 有限の面の外側（面を無限平面として扱うと当たる）。
	PHYSICS_REQUIRE(!SweepToCenter(FSphere{{1.6f, 0, -3}, 0.5f}, FVector3{1.6f, 0, 3}, Cube));
	PHYSICS_REQUIRE(!SweepToCenter(FCircle2D{{1.6f, -3}, 0.5f}, FVector2{1.6f, 3}, Square));
	// 区間外（終点より奥、遠ざかる向き）。
	PHYSICS_REQUIRE(!SweepToCenter(FSphere{{0, 0, -5}, 0.5f}, FVector3{0, 0, -2}, Cube));
	PHYSICS_REQUIRE(!SweepToCenter(FCircle2D{{0, -3}, 0.5f}, FVector2{0, -10}, Square));
	// 開始時の内部・境界は初期接触（離れる向きでも）。
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{0, 0, 10}, Cube), 0, true, 0));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FCircle2D{{2, 0}, 1}, FVector2{9, 0}, Square), 0, true, 0));
	// G13: 半幅(2,0.5)、+30度、半径0.25、(-5,1)→(5,1)。最初の中心x=(√3/2-0.75)/0.5。-30度ならx=(0.5-2.25)/(√3/2)。
	// f32の角度・中心を使うため、許容差は1e-6とする。
	const f64 PlusX = (Sqrt(3.0) / 2 - 0.75) / 0.5;
	const f64 MinusX = (0.5 - 2.25) / (Sqrt(3.0) / 2);
	PHYSICS_REQUIRE(Hit_Internal(
	    SweepToCenter(FCircle2D{{-5, 1}, 0.25f}, FVector2{5, 1}, FOrientedBox2D{{0, 0}, {2, 0.5f}, SixthPi}),
	    (PlusX + 5) / 10, false, 1e-6));
	PHYSICS_REQUIRE(Hit_Internal(
	    SweepToCenter(FCircle2D{{-5, 1}, 0.25f}, FVector2{5, 1}, FOrientedBox2D{{0, 0}, {2, 0.5f}, -SixthPi}),
	    (MinusX + 5) / 10, false, 1e-6));
	// G15: 有効範囲の軸長誤差。最初の中心x=2s+0.5（sは格納したf32値）。軸を正規化すると変わる。
	const f32 Scale = static_cast<f32>(1 + 3e-5);
	FOBB Long{{0, 0, 0}, {2, 0.5f, 0.5f}};
	Long.Axes = {FVector3{Scale, 0, 0}, FVector3{0, 1, 0}, FVector3{0, 0, 1}};
	const f64 LongX = 2 * f64(Scale) + 0.5;
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FSphere{{5, 0, 0}, 0.5f}, FVector3{0, 0, 0}, Long), (5 - LongX) / 5, false, 1e-12));
	// G15: せん断。側面x-εy=1、中心x=1+0.5ε+0.25√(1+ε^2)。
	const f32 Epsilon = static_cast<f32>(5e-5);
	FOBB Sheared{{0, 0, 0}, {1, 1, 1}};
	Sheared.Axes = {FVector3{1, 0, 0}, FVector3{Epsilon, 1, 0}, FVector3{0, 0, 1}};
	const f64 ShearX = 1 + 0.5 * f64(Epsilon) + 0.25 * Sqrt(1 + f64(Epsilon) * f64(Epsilon));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{3, 0.5f, 0}, 0.25f}, FVector3{0, 0.5f, 0}, Sheared),
	                             (3 - ShearX) / 3, false, 1e-12));
	// G16: 半幅0の辺・点・面。
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{0, 3}, 0.5f}, FVector2{0, -3}, FOrientedBox2D{{0, 0}, {1, 0}, 0}),
	                 2.5 / 6, false, 1e-12));
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{-3, 0}, 0.5f}, FVector2{3, 0}, FOrientedBox2D{{0, 0}, {0, 0}, 0}),
	                 2.5 / 6, false, 1e-12));
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FSphere{{0, 0, 3}, 0.5f}, FVector3{0, 0, -3}, FOBB{{0, 0, 0}, {1, 1, 0}}), 2.5 / 6,
	                 false, 1e-12));
	PHYSICS_REQUIRE(!SweepToCenter(FSphere{{1.6f, 0, 3}, 0.5f}, FVector3{1.6f, 0, -3}, FOBB{{0, 0, 0}, {1, 1, 0}}));
	// G17: 両端は離れていて、途中の厚さ0の板を横切る。半径は格納したf32の0.1で、割合は(5-r)/10。
	const f64 Plate = (5 - f64(0.1f)) / 10;
	PHYSICS_REQUIRE(
	    Hit_Internal(SweepToCenter(FCircle2D{{-5, 0}, 0.1f}, FVector2{5, 0}, FOrientedBox2D{{0, 0}, {0, 2}, 0}), Plate,
	                 false, 1e-12));
	PHYSICS_REQUIRE(Hit_Internal(
	    SweepToCenter(FSphere{{-5, 0, 0}, 0.1f}, FVector3{5, 0, 0}, FOBB{{0, 0, 0}, {0, 2, 2}}), Plate, false, 1e-12));
	// 静止（開始＝終点）: 外部は空、接触は0。
	PHYSICS_REQUIRE(!SweepToCenter(FCircle2D{{2.5f, 0}, 1}, FVector2{2.5f, 0}, Square));
	PHYSICS_REQUIRE(Hit_Internal(SweepToCenter(FSphere{{2, 0, 0}, 1}, FVector3{2, 0, 0}, Cube), 0, true, 0));
	// 無効な対象形状は例外。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    FOBB Broken = Cube;
		    Broken.Axes[0] = {2, 0, 0};
		    (void)SweepToCenter(FSphere{{5, 0, 0}, 1}, FVector3{0, 0, 0}, Broken);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)SweepToCenter(FCircle2D{{5, 0}, 1}, FVector2{0, 0}, FOrientedBox2D{{0, 0}, {-1, 1}, 0});
	    }));
}

// 空World・Colliderなし・最短・結果メンバー・同距離・初期接触の複数（W01・W02）。
template <typename T> void Basics_Internal()
{
	typename T::FWorld World;
	PHYSICS_REQUIRE(!World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10)));
	T::Body(World, T::At(0));
	PHYSICS_REQUIRE(!World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10)));
	// 遠いものを先に登録する（登録順と距離順を逆にする）。
	const auto Far = T::Ball(World, T::Body(World, T::At(8), EBodyType::Dynamic), T::At(0), 1);
	const auto Near = T::Ball(World, T::Body(World, T::At(5), EBodyType::Kinematic), T::At(0), 1);
	const auto Hit = World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10));
	// G01と同じ配置。中心は接触表面(4,0)ではなく(3.5,0)。
	PHYSICS_REQUIRE(Hit && Hit->Collider == Near && Near_Internal(Hit->Fraction, 0.35, 1e-12) && !Hit->bInitialContact);
	PHYSICS_REQUIRE(Hit->CenterAtHit == T::At(3.5f));
	// 自己除外で次の候補へ。
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10), Near.Body)->Collider == Far);
	// 同じ割合はスロット昇順。初期接触が複数あっても同じ規則（割合0）。
	const auto Twin = T::Ball(World, T::Body(World, T::At(5)), T::At(0), 1);
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10))->Collider == Near);
	const auto Initial = World.SweepClosest(T::Probe(T::At(5), 0.5f), T::At(-10));
	PHYSICS_REQUIRE(Initial && Initial->Collider == Near && Initial->Fraction == 0 && Initial->bInitialContact);
	PHYSICS_REQUIRE(Initial->CenterAtHit == T::At(5));
	PHYSICS_REQUIRE(World.DetachCollider(Near));
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10))->Collider == Twin);
	// 静止: 開始＝終点。
	PHYSICS_REQUIRE(!World.SweepClosest(T::Probe(T::At(2), 0.5f), T::At(2)));
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(3.6f), 0.5f), T::At(3.6f))->Collider == Twin);
}

// 半径0の非ゼロ移動は同じ条件のRaycastClosestと同じ結果。半径0・ゼロ移動は点の重なり（G07）。
template <typename T> void RadiusZero_Internal()
{
	typename T::FWorld World;
	const auto Target = T::Box(World, T::Body(World, T::At(5)), T::At(0), 1, 2);
	T::Ball(World, T::Body(World, T::At(9)), T::At(0), 1);
	const auto Ray = World.RaycastClosest(T::At(0, 0.5f), T::At(10, 0.5f));
	const auto Sweep = World.SweepClosest(T::Probe(T::At(0, 0.5f), 0), T::At(10, 0.5f));
	PHYSICS_REQUIRE(Ray && Sweep && Sweep->Collider == Ray->Collider && Sweep->Fraction == Ray->Fraction);
	PHYSICS_REQUIRE(Sweep->CenterAtHit == Ray->Position && !Sweep->bInitialContact && Sweep->Collider == Target);
	// 半径0の開始点が形状内部なら、Raycastの割合0を初期接触として扱う。
	const auto Inside = World.SweepClosest(T::Probe(T::At(5), 0), T::At(10));
	PHYSICS_REQUIRE(Inside && Inside->Fraction == 0 && Inside->bInitialContact && Inside->Collider == Target);
	// 点の重なり（ゼロ移動）。Raycastのゼロ長拒否とは別の契約。
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(5), 0), T::At(5))->Collider == Target);
	PHYSICS_REQUIRE(!World.SweepClosest(T::Probe(T::At(2), 0), T::At(2)));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.RaycastClosest(T::At(2), T::At(2));
	    }));
}

// カテゴリ・自己除外・ID・即時反映（W03〜W06、W14）。
template <typename T> void Filters_Internal()
{
	typename T::FWorld World;
	const auto Self = T::Body(World, T::At(0));
	const auto SelfFirst = T::Ball(World, Self, T::At(2), 0.5f, CharacterCategory);
	T::Ball(World, Self, T::At(3), 0.5f, ObstacleCategory);
	const auto Other = T::Body(World, T::At(0));
	const auto Pickup = T::Ball(World, Other, T::At(4), 0.5f, PickupCategory);
	const auto Hidden = T::Ball(World, Other, T::At(5), 0.5f, 0u);
	const auto Wall = T::Box(World, Other, T::At(7), 0.25f, 2, ObstacleCategory | HighCategory);
	const auto Probe = T::Probe(T::At(-2), 0.25f);
	// 自己Bodyの全Colliderを除外し、カテゴリで絞ってから最短を選ぶ。
	PHYSICS_REQUIRE(World.SweepClosest(Probe, T::At(10), Self)->Collider == Pickup);
	PHYSICS_REQUIRE(World.SweepClosest(Probe, T::At(10), Self, Mask_Internal(ObstacleCategory))->Collider == Wall);
	PHYSICS_REQUIRE(World.SweepClosest(Probe, T::At(10), Self, Mask_Internal(HighCategory))->Collider == Wall);
	PHYSICS_REQUIRE(!World.SweepClosest(Probe, T::At(10), {}, Mask_Internal(0u)));
	PHYSICS_REQUIRE(World.GetColliderQueryCategory(Hidden) == 0u);
	// カテゴリ0は全ビットでも対象外。変更はStepなしで反映。
	World.SetColliderQueryCategory(Pickup, 0u);
	PHYSICS_REQUIRE(World.SweepClosest(Probe, T::At(10), Self)->Collider == Wall);
	World.SetColliderQueryCategory(Hidden, PickupCategory);
	PHYSICS_REQUIRE(World.SweepClosest(Probe, T::At(10), Self)->Collider == Hidden);
	// 空Optionalと明示した無効ID・別World・旧世代は別。
	PHYSICS_REQUIRE(World.SweepClosest(Probe, T::At(10), {})->Collider == SelfFirst);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(Probe, T::At(10), typename T::FBodyId{}, Mask_Internal(0u));
	    }));
	typename T::FWorld Foreign;
	const auto ForeignBody = T::Body(Foreign, T::At(0));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(Probe, T::At(10), ForeignBody);
	    }));
	// 姿勢の即時反映。壁を移動・回転すると結果が変わる。
	T::Move(World, Other, T::At(0, 30), 0);
	PHYSICS_REQUIRE(!World.SweepClosest(Probe, T::At(10), Self));
	T::Move(World, Other, T::At(-7), HalfPi);
	const auto Turned = World.SweepClosest(Probe, T::At(10), Self, Mask_Internal(ObstacleCategory));
	// 90度回転でBodyローカル(7,0)はワールド(-7,7)付近へ移り、y=0の経路から外れる。
	PHYSICS_REQUIRE(!Turned);
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 0);
	// 削除・同スロット再生成で保存IDを読み替えない。World破棄後も保存結果は読める。
	T::Move(World, Other, T::At(0), 0);
	const auto Saved = World.SweepClosest(Probe, T::At(10), Self, Mask_Internal(ObstacleCategory));
	PHYSICS_REQUIRE(Saved && Saved->Collider == Wall);
	PHYSICS_REQUIRE(World.DestroyBody(Other));
	const auto Reused = T::Body(World, T::At(0));
	PHYSICS_REQUIRE(Reused.Index == Other.Index && !(Reused == Other) && !World.IsColliderAlive(Saved->Collider));
	TOptional<typename T::FHit> Kept;
	{
		typename T::FWorld Temporary;
		T::Ball(Temporary, T::Body(Temporary, T::At(5)), T::At(0), 1);
		Kept = Temporary.SweepClosest(Probe, T::At(10));
	}
	PHYSICS_REQUIRE(Kept && Kept->Fraction > 0);
}

// 入力検証（空World・マスク0でも）、候補外の変換不能形状、先行0の後の失敗、Step状態、失敗後の再利用（W07〜W10）。
template <typename T> void Failures_Internal()
{
	typename T::FWorld World;
	const f32 Max = TNumericLimits<f32>::Max();
	const f32 Nan = TNumericLimits<f32>::QuietNaN();
	// 定数式の桁あふれ警告を避け、実行時の乗算で無限大を作る。
	f32 Scale = 2;
	const f32 Inf = Max * Scale;
	const FWorldQueryFilter None = Mask_Internal(0u);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(0), -1), T::At(1), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(Nan), 1), T::At(1), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(0), Inf), T::At(1), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(-Max), 1), T::At(Max), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(0), 0), T::At(1), typename T::FBodyId{}, None);
	    }));
	// 先頭スロットで初期接触（割合0）、後ろに変換不能な対象形状。
	const auto First = T::Body(World, T::At(0), EBodyType::Dynamic);
	const auto Leading = T::Ball(World, First, T::At(0), 1, ObstacleCategory);
	const auto Huge = T::Body(World, T::At(Max));
	const auto Broken = T::Ball(World, Huge, T::At(Max), 1, PickupCategory);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1));
	    }));
	// 対象外（カテゴリ・自己除外）の形状は計算しない。
	const auto Filtered = World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1), {}, Mask_Internal(ObstacleCategory));
	PHYSICS_REQUIRE(Filtered && Filtered->Collider == Leading && Filtered->bInitialContact);
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1), Huge)->Collider == Leading);
	World.SetColliderQueryCategory(Broken, ObstacleCategory);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1), {}, Mask_Internal(ObstacleCategory));
	    }));
	// 失敗しても以前の結果は変わらない。入力を直せば同じWorldで使える。
	TOptional<typename T::FHit> Previous = Filtered;
	try
	{
		Previous = World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1));
	}
	catch (const FException&)
	{
	}
	PHYSICS_REQUIRE(Previous && Previous->Collider == Leading && Previous->Fraction == 0);
	PHYSICS_REQUIRE(World.DestroyBody(Huge));
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1))->Collider == Leading);
	// Stepの引数検査だけの失敗では禁止しない。途中失敗したStepの後は拒否し、正常Stepで回復する。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.Step(0);
	    }));
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1)));
	T::NoGravity(World);
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings BrokenJobs;
	BrokenJobs.JobSystem = &Jobs;
	World.SetExecutionSettings(BrokenJobs);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.Step(.25, 1);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(0), 0), T::At(1));
	    }));
	World.SetExecutionSettings({});
	World.Step(.5, 5);
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(1))->Collider == Leading);
}

// 問い合わせあり／なしで数値経過が一致する。対象の速度は結果に影響しない（W11・W12）。
template <typename T> void ReadOnly_Internal()
{
	typename T::FWorld Queried;
	typename T::FWorld Control;
	T::NoGravity(Queried);
	T::NoGravity(Control);
	const auto A = T::Body(Queried, T::At(0), EBodyType::Dynamic);
	const auto B = T::Body(Control, T::At(0), EBodyType::Dynamic);
	T::Box(Queried, A, T::At(0), 1, 0.5f);
	T::Box(Control, B, T::At(0), 1, 0.5f);
	T::Push(Queried, A);
	T::Push(Control, B);
	const typename T::FWorld& Read = Queried;
	const auto Before = Read.SweepClosest(T::Probe(T::At(-5), 0.25f), T::At(5));
	for (int32 Index = 0; Index < 50; ++Index)
	{
		PHYSICS_REQUIRE(Read.SweepClosest(T::Probe(T::At(-5), 0.25f), T::At(5)));
		PHYSICS_REQUIRE(Read.SweepClosest(T::Probe(T::At(0), 0.25f), T::At(0)));
	}
	// 対象の速度だけを変えても、同じ現在姿勢なら結果は同じ（未来位置を予測しない）。
	// 比較のため、対照Worldにも同じ速度の設定と解除を行う。
	T::Velocity(Queried, A, 50);
	T::Velocity(Control, B, 50);
	const auto Moving = Read.SweepClosest(T::Probe(T::At(-5), 0.25f), T::At(5));
	PHYSICS_REQUIRE(Before && Moving && Before->Fraction == Moving->Fraction &&
	                Before->CenterAtHit == Moving->CenterAtHit);
	T::Velocity(Queried, A, 0);
	T::Velocity(Control, B, 0);
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 0);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		(void)Read.SweepClosest(T::Probe(T::At(-20), 1), T::At(20));
		Queried.Step(.01);
		Control.Step(.01);
		PHYSICS_REQUIRE(T::SameState(Queried, A, Control, B));
	}
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 20);
}

// 接地・休止中のBodyも対象で、問い合わせで起こさない。
template <typename T> void Sleeping_Internal()
{
	typename T::FWorld World;
	T::Box(World, T::Body(World, T::At(0, -1)), T::At(0), 5, 1, ObstacleCategory);
	const auto Body = T::Body(World, T::At(0, 0.6f), EBodyType::Dynamic);
	const auto Collider = T::Box(World, Body, T::At(0), 0.5f, 0.5f, CharacterCategory);
	for (int32 Index = 0; Index < 600; ++Index)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(World.IsSleeping(Body));
	const auto Position = World.GetPosition(Body);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		const auto Hit =
		    World.SweepClosest(T::Probe(T::At(-5, 0.5f), 0.25f), T::At(5, 0.5f), {}, Mask_Internal(CharacterCategory));
		PHYSICS_REQUIRE(Hit && Hit->Collider == Collider);
	}
	PHYSICS_REQUIRE(World.IsSleeping(Body) && World.GetPosition(Body) == Position);
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 600);
}

// Debug表示の上限に依存しない。後ろのスロットの最短対象と計算失敗（W16）。
template <typename T> void Unlimited_Internal()
{
	typename T::FWorld World;
	const auto Body = T::Body(World, T::At(0));
	for (int32 Index = 0; Index < 300; ++Index)
	{
		T::Ball(World, Body, T::At(static_cast<f32>(Index), 50), 0.5f);
	}
	const auto Last = T::Ball(World, Body, T::At(5), 0.5f);
	PHYSICS_REQUIRE(World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10))->Collider == Last);
	const f32 Max = TNumericLimits<f32>::Max();
	T::Ball(World, T::Body(World, T::At(Max)), T::At(Max), 1);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10));
	    }));
}

// G14: XY平面に閉じない3D回転。Body姿勢（X軸回り90度）＋ローカル中心＋非等方の半幅。
// Body(0,0,10)、ローカル中心(0,2,0)→ワールド(0,0,12)。半幅(1,2,0.5)はワールドでx±1、y±0.5、z±2。
void NonPlanarTransform_Internal()
{
	FPhysicsWorld3D World;
	FBodyDescription3D Description;
	Description.Type = EBodyType::Static;
	Description.Position = {0, 0, 10};
	Description.Orientation = FQuaternion::FromAxisAngle({1, 0, 0}, HalfPi);
	const auto Body = World.CreateBody(Description);
	FColliderDescription3D Box;
	Box.Shape = FOBB{{0, 2, 0}, {1, 2, 0.5f}};
	const auto Id = World.AttachCollider(Body, Box);
	// y方向: 面y=-0.5へ半径0.25で。中心y=-0.75、割合4.25/10。
	const auto AlongY = World.SweepClosest(FSphere{{0, -5, 12}, 0.25f}, {0, 5, 12});
	PHYSICS_REQUIRE(AlongY && AlongY->Collider == Id && Near_Internal(AlongY->Fraction, 0.425, 1e-5));
	// z方向: 面z=14へ。中心z=14.25、割合5.75/20。
	const auto AlongZ = World.SweepClosest(FSphere{{0, 0, 20}, 0.25f}, {0, 0, 0});
	PHYSICS_REQUIRE(AlongZ && AlongZ->Collider == Id && Near_Internal(AlongZ->Fraction, 0.2875, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(AlongZ->CenterAtHit.Z, 14.25, 1e-4));
	// 回転を無視する（ローカルのまま）と当たる位置でも当たらない。
	PHYSICS_REQUIRE(!World.SweepClosest(FSphere{{0, 1.5f, 20}, 0.25f}, {0, 1.5f, 0}));
}

// 2Dの回転矩形（Body角度＋Colliderローカル角度）とG13のWorld経路。
void RotatedBoxWorld2D_Internal()
{
	FPhysicsWorld2D World;
	FBodyDescription2D Description;
	Description.Type = EBodyType::Static;
	Description.Angle = SixthPi / 2;
	const auto Body = World.CreateBody(Description);
	FColliderDescription2D Box;
	Box.Shape = FOrientedBox2D{{0, 0}, {2, 0.5f}, SixthPi / 2};
	const auto Id = World.AttachCollider(Body, Box);
	const auto Hit = World.SweepClosest(FCircle2D{{-5, 1}, 0.25f}, {5, 1});
	const f64 PlusX = (Sqrt(3.0) / 2 - 0.75) / 0.5;
	PHYSICS_REQUIRE(Hit && Hit->Collider == Id && Near_Internal(Hit->Fraction, (PlusX + 5) / 10, 1e-6));
	PHYSICS_REQUIRE(Near_Internal(Hit->CenterAtHit.X, PlusX, 1e-5) && Hit->CenterAtHit.Y == 1);
}

// ゲーム側の利用: 中心線の射線は外れるが、半径のある移動は当たる。途中の障害物、初期接触、カテゴリ変更（W15）。
template <typename T> void GameUsage_Internal()
{
	typename T::FWorld World;
	const auto Self = T::Body(World, T::At(0));
	T::Ball(World, Self, T::At(0), 0.5f, CharacterCategory);
	// 下面がy=0.4の壁（半幅0.5の箱、中心(5,0.9)）。中心線y=0は外れるが半径0.5の円／球は当たる。
	// 最初の接触は角(4.5,0.4)で、中心x=4.5-√(0.25-0.16)=4.2。
	const auto WallBody = T::Body(World, T::At(5, 0.9f));
	const auto Wall = T::Box(World, WallBody, T::At(0), 0.5f, 0.5f, ObstacleCategory);
	T::Ball(World, T::Body(World, T::At(3, -0.3f)), T::At(0), 0.2f, PickupCategory);
	FWorldQueryFilter Obstacles;
	Obstacles.IncludeCategories = ObstacleCategory;
	PHYSICS_REQUIRE(!World.RaycastClosest(T::At(0), T::At(10), Self, Obstacles));
	const auto Hit = World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10), Self, Obstacles);
	PHYSICS_REQUIRE(Hit && Hit->Collider == Wall && !Hit->bInitialContact);
	// 返るのは円／球の中心（y=0の経路上）で、接触表面の点ではない。
	PHYSICS_REQUIRE(Hit->CenterAtHit == T::At(T::X(Hit->CenterAtHit)) &&
	                Near_Internal(T::X(Hit->CenterAtHit), 4.2, 1e-5));
	// 移動候補の選び方（ゲーム側の方針）: 初期接触なら動かさず、それ以外は接触時の中心で止める。
	typename T::FVector Chosen = T::At(10);
	if (Hit)
	{
		Chosen = Hit->bInitialContact ? T::At(0) : Hit->CenterAtHit;
	}
	PHYSICS_REQUIRE(T::X(Chosen) == T::X(Hit->CenterAtHit));
	// 距離単位の余白Skinは、区間長Lに対してmax(0, Fraction - Skin/L)で割合へ変える。
	const f64 Length = 10;
	const f64 Skin = 0.05;
	const f64 Safe = Max(0.0, Hit->Fraction - Skin / Length);
	PHYSICS_REQUIRE(Safe < Hit->Fraction && Safe > 0);
	// 接触時の中心からさらに進もうとすると、すぐに同じ壁で止まる（押し出しの解は返さない）。
	// CenterAtHitはf32へ丸めた値なので、接触ちょうどとは限らない（わずかに手前なら初期接触ではない）。
	const auto Again = World.SweepClosest(T::Probe(Hit->CenterAtHit, 0.5f), T::At(10), Self, Obstacles);
	PHYSICS_REQUIRE(Again && Again->Collider == Wall && Again->Fraction < 1e-6);
	// 壁をカテゴリ0にすると、Stepなしで移動候補が終点まで通る。壁の登録は残る。
	World.SetColliderQueryCategory(Wall, 0u);
	PHYSICS_REQUIRE(!World.SweepClosest(T::Probe(T::At(0), 0.5f), T::At(10), Self, Obstacles));
	PHYSICS_REQUIRE(World.IsColliderAlive(Wall) && World.CaptureSnapshot().Colliders.Size() == 3);
}

// 2D／3Dの各ケース。
const PhysicsTest::FCase Cases_Internal[] = {
    {"sweep circle and sphere math", &CircleSphereMath_Internal},
    {"sweep box features axes and degenerate shapes", &BoxMath_Internal},
    {"2D sweep basics order and initial contact", &Basics_Internal<F2D>},
    {"3D sweep basics order and initial contact", &Basics_Internal<F3D>},
    {"2D sweep radius zero matches raycast", &RadiusZero_Internal<F2D>},
    {"3D sweep radius zero matches raycast", &RadiusZero_Internal<F3D>},
    {"2D sweep filters ids and immediate state", &Filters_Internal<F2D>},
    {"3D sweep filters ids and immediate state", &Filters_Internal<F3D>},
    {"2D sweep validation and failures", &Failures_Internal<F2D>},
    {"3D sweep validation and failures", &Failures_Internal<F3D>},
    {"2D sweep preserves dynamics and ignores velocity", &ReadOnly_Internal<F2D>},
    {"3D sweep preserves dynamics and ignores velocity", &ReadOnly_Internal<F3D>},
    {"2D sweep sleeping body", &Sleeping_Internal<F2D>},
    {"3D sweep sleeping body", &Sleeping_Internal<F3D>},
    {"2D sweep beyond debug limit", &Unlimited_Internal<F2D>},
    {"3D sweep beyond debug limit", &Unlimited_Internal<F3D>},
    {"3D sweep non-planar body transform", &NonPlanarTransform_Internal},
    {"2D sweep rotated box in world", &RotatedBoxWorld2D_Internal},
    {"2D sweep movement candidate game usage", &GameUsage_Internal<F2D>},
    {"3D sweep camera candidate game usage", &GameUsage_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetWorldSweepCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
