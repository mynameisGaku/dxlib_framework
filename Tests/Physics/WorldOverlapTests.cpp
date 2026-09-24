// SPDX-License-Identifier: NOASSERTION
// 範囲問い合わせ（2Dの円／3Dの球と重なる全Collider）と、候補抽出→射線判定のゲーム利用。
// 幾何の期待値は手計算しやすい配置の解析値で、製品の変換・判定関数から作らない。
// 同じWorld契約を実FPhysicsWorld2D／FPhysicsWorld3Dの両方で実行する（2DをZ=0の3Dで代用しない）。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/JobSystem.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// ゲーム側で定義する想定のカテゴリ。
constexpr uint32 ObstacleCategory = 1u << 0;
constexpr uint32 CharacterCategory = 1u << 1;
constexpr uint32 PickupCategory = 1u << 2;
constexpr uint32 HighCategory = 0x80000000u;
// f32で表した直角と45度。
constexpr f32 HalfPi = 1.57079632679f;
constexpr f32 QuarterPi = 0.785398163397f;
constexpr f32 SixthPi = 0.523598775598f;

// 指定処理がFExceptionを送出するか調べる。
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

// マスクだけを指定したフィルター。
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
	using FArea = FCircle2D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static FArea Area(FVector Center, f32 Radius)
	{
		return {Center, Radius};
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
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, uint32 Category = 1u,
	                       f32 Angle = 0)
	{
		FColliderDescription2D Description;
		Description.Shape = FOrientedBox2D{Center, {HalfX, HalfY}, Angle};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static void Move(FWorld& World, FBodyId Body, FVector Position, f32 Angle)
	{
		World.SetBodyTransform(Body, Position, Angle);
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
	using FArea = FSphere;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static FArea Area(FVector Center, f32 Radius)
	{
		return {Center, Radius};
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
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, uint32 Category = 1u,
	                       f32 Angle = 0)
	{
		FColliderDescription3D Description;
		// Z軸回りの回転。Z方向の半幅は大きい方の半幅とする。
		const f32 C = static_cast<f32>(Cos(f64(Angle)));
		const f32 S = static_cast<f32>(Sin(f64(Angle)));
		FOBB Box{Center, {HalfX, HalfY, HalfX > HalfY ? HalfX : HalfY}};
		Box.Axes = {FVector3{C, S, 0}, FVector3{-S, C, 0}, FVector3{0, 0, 1}};
		Description.Shape = Box;
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static void Move(FWorld& World, FBodyId Body, FVector Position, f32 Angle)
	{
		World.SetBodyTransform(Body, Position, FQuaternion::FromAxisAngle({0, 0, 1}, Angle));
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

// 結果がちょうど指定のIDの並びか調べる。
template <typename TId> bool Equals_Internal(const TVector<TId>& Actual, const TVector<TId>& Expected)
{
	if (Actual.Size() != Expected.Size())
	{
		return false;
	}
	for (size_t Index = 0; Index < Actual.Size(); ++Index)
	{
		if (!(Actual[Index] == Expected[Index]))
		{
			return false;
		}
	}
	return true;
}

// 2D円と回転矩形の真偽判定。許容距離0の接触、角、回転の向き、退化形状、不正入力。
void CircleBoxMath_Internal()
{
	// 中心(0,0)・半幅(1,1)の矩形。
	const FOrientedBox2D Square{{0, 0}, {1, 1}, 0};
	// 辺x=1へ中心(2,0)・半径1の円が接する（G01）。
	PHYSICS_REQUIRE(Intersects(FCircle2D{{2, 0}, 1}, Square, 0.0f));
	// 正の隙間約4.1e-6。既定の許容距離1e-5なら含むが、0では含まない（G02）。
	PHYSICS_REQUIRE(!Intersects(FCircle2D{{2.000004f, 0}, 1}, Square, 0.0f));
	PHYSICS_REQUIRE(Intersects(FCircle2D{{2.000004f, 0}, 1}, Square, 1e-5f));
	// 角(1,1)の外側。距離の二乗は0.8^2×2=1.28>1（G06）。各軸を半径だけ膨らませた矩形なら含んでしまう位置。
	PHYSICS_REQUIRE(!Intersects(FCircle2D{{1.8f, 1.8f}, 1}, Square, 0.0f));
	PHYSICS_REQUIRE(Intersects(FCircle2D{{1.5f, 1.5f}, 1}, Square, 0.0f));
	// 半幅(2,0.5)・30度。中心(1.5,1)は局所(1.799,0.116)で内部。-30度なら局所y=1.616で届かない（G07）。
	PHYSICS_REQUIRE(Intersects(FCircle2D{{1.5f, 1}, 0.1f}, FOrientedBox2D{{0, 0}, {2, 0.5f}, SixthPi}, 0.0f));
	PHYSICS_REQUIRE(!Intersects(FCircle2D{{1.5f, 1}, 0.1f}, FOrientedBox2D{{0, 0}, {2, 0.5f}, -SixthPi}, 0.0f));
	// 半径0の円（点）: 内部・境界・外部（G05）。
	PHYSICS_REQUIRE(Intersects(FCircle2D{{0.5f, 0}, 0}, Square, 0.0f));
	PHYSICS_REQUIRE(Intersects(FCircle2D{{1, 1}, 0}, Square, 0.0f));
	PHYSICS_REQUIRE(!Intersects(FCircle2D{{1.5f, 0}, 0}, Square, 0.0f));
	// 完全包含。円が矩形の内部、矩形が円の内部（G04）。
	PHYSICS_REQUIRE(Intersects(FCircle2D{{0, 0}, 0.1f}, Square, 0.0f));
	PHYSICS_REQUIRE(Intersects(FCircle2D{{0, 0}, 10}, Square, 0.0f));
	// 半幅0の矩形（線分・点）への接触（G10）。
	PHYSICS_REQUIRE(Intersects(FCircle2D{{0, 1}, 1}, FOrientedBox2D{{0, 0}, {2, 0}, 0}, 0.0f));
	PHYSICS_REQUIRE(!Intersects(FCircle2D{{0, 1.001f}, 1}, FOrientedBox2D{{0, 0}, {2, 0}, 0}, 0.0f));
	PHYSICS_REQUIRE(Intersects(FCircle2D{{3, 0}, 0}, FOrientedBox2D{{3, 0}, {0, 0}, 0}, 0.0f));
	// 中心差はf64で求める。f32では差があふれる距離でも、正常な非交差（G11）。
	PHYSICS_REQUIRE(!Intersects(FCircle2D{{-3e38f, 0}, 1}, FOrientedBox2D{{3e38f, 0}, {1, 1}, 0}, 0.0f));
	// 2^20付近で表現できる小さな隙間0.125。
	PHYSICS_REQUIRE(Intersects(FCircle2D{{1048576, 0}, 1}, FOrientedBox2D{{1048578, 0}, {1, 1}, 0}, 0.0f));
	PHYSICS_REQUIRE(!Intersects(FCircle2D{{1048576, 0}, 1}, FOrientedBox2D{{1048578.125f, 0}, {1, 1}, 0}, 0.0f));
	// 不正形状・負の許容距離は例外。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)Intersects(FCircle2D{{0, 0}, -1}, Square, 0.0f);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)Intersects(FCircle2D{{0, 0}, 1}, FOrientedBox2D{{0, 0}, {1, 1}, TNumericLimits<f32>::QuietNaN()},
		                     0.0f);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)Intersects(FCircle2D{{0, 0}, 1}, Square, -1.0f);
	    }));
}

// 3D球と球／OBBの許容距離指定の判定。接触、角、XY平面外の軸、退化形状、不正入力。
void SphereMath_Internal()
{
	PHYSICS_REQUIRE(IntersectsSphere(FSphere{{0, 0, 0}, 1}, FSphere{{2, 0, 0}, 1}, 0.0f));
	PHYSICS_REQUIRE(!IntersectsSphere(FSphere{{0, 0, 0}, 1}, FSphere{{2.000004f, 0, 0}, 1}, 0.0f));
	PHYSICS_REQUIRE(IntersectsSphere(FSphere{{0, 0, 0}, 1}, FSphere{{2.000004f, 0, 0}, 1}, 1e-5f));
	// 半幅(1,1,1)の角の外側。距離の二乗は0.6^2×3=1.08>1。
	const FOBB Cube{{0, 0, 0}, {1, 1, 1}};
	PHYSICS_REQUIRE(!IntersectsSphere(FSphere{{1.6f, 1.6f, 1.6f}, 1}, Cube, 0.0f));
	PHYSICS_REQUIRE(IntersectsSphere(FSphere{{1.5f, 1.5f, 1.5f}, 1}, Cube, 0.0f));
	PHYSICS_REQUIRE(IntersectsSphere(FSphere{{2, 0, 0}, 1}, Cube, 0.0f));
	PHYSICS_REQUIRE(!IntersectsSphere(FSphere{{2.000004f, 0, 0}, 1}, Cube, 0.0f));
	// X軸回り90度: 局所Y→ワールドZ。半幅(1,2,0.5)はワールドでx±1、y±0.5、z±2（G09）。
	FOBB Tilted{{0, 0, 0}, {1, 2, 0.5f}};
	Tilted.Axes = {FVector3{1, 0, 0}, FVector3{0, 0, 1}, FVector3{0, -1, 0}};
	PHYSICS_REQUIRE(IntersectsSphere(FSphere{{0, 0, 2.4f}, 0.5f}, Tilted, 0.0f));
	PHYSICS_REQUIRE(!IntersectsSphere(FSphere{{0, 1.2f, 0}, 0.5f}, Tilted, 0.0f));
	// 半幅0の面・点、半径0の球。
	PHYSICS_REQUIRE(IntersectsSphere(FSphere{{0, 0, 1}, 1}, FOBB{{0, 0, 0}, {1, 1, 0}}, 0.0f));
	PHYSICS_REQUIRE(!IntersectsSphere(FSphere{{0, 0, 1.001f}, 1}, FOBB{{0, 0, 0}, {1, 1, 0}}, 0.0f));
	PHYSICS_REQUIRE(IntersectsSphere(FSphere{{1, 1, 1}, 0}, Cube, 0.0f));
	PHYSICS_REQUIRE(IntersectsSphere(FSphere{{3, 0, 0}, 0}, FSphere{{3, 0, 0}, 0}, 0.0f));
	// f32では差があふれる距離でも正常な非交差。
	PHYSICS_REQUIRE(!IntersectsSphere(FSphere{{-3e38f, 0, 0}, 1}, FSphere{{3e38f, 0, 0}, 1}, 0.0f));
	PHYSICS_REQUIRE(!IntersectsSphere(FSphere{{-3e38f, 0, 0}, 1}, FOBB{{3e38f, 0, 0}, {1, 1, 1}}, 0.0f));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)IntersectsSphere(FSphere{{0, 0, 0}, -1}, Cube, 0.0f);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    FOBB Broken = Cube;
		    Broken.Axes[0] = {2, 0, 0};
		    (void)IntersectsSphere(FSphere{{0, 0, 0}, 1}, Broken, 0.0f);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)IntersectsSphere(FSphere{{0, 0, 0}, 1}, FSphere{{0, 0, 0}, 1}, -1.0f);
	    }));
}

// 空World・Colliderなし・全非交差は空配列。接触境界は含み、正の隙間は含まない。重心が範囲外の大きい形状も含む。
template <typename T> void Basics_Internal()
{
	typename T::FWorld World;
	PHYSICS_REQUIRE(World.OverlapAll(T::Area(T::At(0), 5)).IsEmpty());
	T::Body(World, T::At(0));
	PHYSICS_REQUIRE(World.OverlapAll(T::Area(T::At(0), 5)).IsEmpty());
	const auto Static = T::Body(World, T::At(2), EBodyType::Static);
	const auto Touch = T::Ball(World, Static, T::At(0), 1);
	PHYSICS_REQUIRE(World.OverlapAll(T::Area(T::At(-10), 1)).IsEmpty());
	// 中心距離2・半径1ずつで接する（G01）。正の隙間は含まない（G02）。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(0), 1)), {Touch}));
	PHYSICS_REQUIRE(World.OverlapAll(T::Area(T::At(-0.000004f), 1)).IsEmpty());
	// 重心(20,0)が範囲外の大きい円／球の表面(x=5)が範囲に接する（G03）。範囲(4.5,r0.5)はx=2の円からは離れている。
	const auto Kinematic = T::Body(World, T::At(20), EBodyType::Kinematic);
	const auto Large = T::Ball(World, Kinematic, T::At(0), 15);
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(4.5f), 0.5f)), {Large}));
	// 範囲(4,r1)は大きい形状とx=2の円の両方にちょうど接する。スロット昇順で両方。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(4), 1)), {Touch, Large}));
	// 範囲が対象の内部・対象が範囲の内部（G04）。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(20), 0.5f)), {Large}));
	const auto Dynamic = T::Body(World, T::At(-30), EBodyType::Dynamic);
	const auto Small = T::Box(World, Dynamic, T::At(0), 0.5f, 0.5f);
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(-30), 5)), {Small}));
	// 半径0の範囲は点。内部・境界・外部（G05）。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(-30.25f), 0)), {Small}));
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(-30.5f), 0)), {Small}));
	PHYSICS_REQUIRE(World.OverlapAll(T::Area(T::At(-31), 0)).IsEmpty());
	// Static／Kinematic／Dynamicを区別しない。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(0), 100)), {Touch, Large, Small}));
}

// Body回転＋Colliderローカル回転＋ローカル中心を一度だけ適用する（G08）。
template <typename T> void Transform_Internal()
{
	typename T::FWorld World;
	// Body(0,5)を90度回転し、ローカル中心(2,0)・半幅(2,0.5)の矩形。ワールド中心(0,7)、x半幅0.5、y半幅2。
	const auto Turned = T::Body(World, T::At(0, 5), EBodyType::Static, HalfPi);
	const auto Box = T::Box(World, Turned, T::At(2), 2, 0.5f);
	// y=9までの表面に隙間0.3。回転を無視すると(y半幅0.5)届かない。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(0, 9.3f), 0.5f)), {Box}));
	// x=0.5までの表面に隙間0.7。回転を無視すると(x半幅2)含んでしまう。
	PHYSICS_REQUIRE(World.OverlapAll(T::Area(T::At(1.2f, 7), 0.5f)).IsEmpty());
	// Body45度＋Collider45度＝90度（x半幅0.5）。二重に適用すると180度でx半幅2になる。
	const auto Composed = T::Body(World, T::At(40), EBodyType::Static, QuarterPi);
	const auto ComposedBox = T::Box(World, Composed, T::At(0), 2, 0.5f, 1u, QuarterPi);
	PHYSICS_REQUIRE(World.OverlapAll(T::Area(T::At(41.2f), 0.5f)).IsEmpty());
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(40.9f), 0.5f)), {ComposedBox}));
	// 回転Bodyのローカル中心(3,0)の円／球はワールド(0,-7)。中心距離0.8＜半径の和1（f32のπ/2の丸めに余裕を持たせる）。
	const auto Offset = T::Body(World, T::At(0, -10), EBodyType::Static, HalfPi);
	const auto Ball = T::Ball(World, Offset, T::At(3), 0.5f);
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(0, -6.2f), 0.5f)), {Ball}));
	PHYSICS_REQUIRE(World.OverlapAll(T::Area(T::At(3, -10), 0.5f)).IsEmpty());
}

// 同じBodyの複数Collider、異なるカテゴリ、スロット昇順、疎なスロット、ビット規約、自己除外（W03〜W06）。
template <typename T> void Selection_Internal()
{
	typename T::FWorld World;
	const auto Self = T::Body(World, T::At(0));
	const auto SelfHead = T::Ball(World, Self, T::At(0, 1), 0.5f, CharacterCategory);
	const auto SelfBody = T::Ball(World, Self, T::At(0), 0.5f, CharacterCategory);
	// 遠いものを先に登録し、距離順とスロット順を違える。
	const auto Other = T::Body(World, T::At(4));
	const auto Far = T::Ball(World, Other, T::At(2), 0.5f, CharacterCategory);
	const auto Gap = T::Ball(World, Other, T::At(0, 30), 0.5f);
	const auto Near = T::Ball(World, Other, T::At(-2), 0.5f, PickupCategory);
	const auto Multi = T::Ball(World, Other, T::At(0), 0.5f, CharacterCategory | PickupCategory);
	const auto Hidden = T::Ball(World, Other, T::At(0, 1), 0.5f, 0u);
	const auto High = T::Ball(World, Other, T::At(0, -1), 0.5f, HighCategory);
	const auto Area = T::Area(T::At(3), 4);
	PHYSICS_REQUIRE(World.DetachCollider(Gap));
	// 全ビット。カテゴリ0は含まず、スロット昇順。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(Area), {SelfHead, SelfBody, Far, Near, Multi, High}));
	PHYSICS_REQUIRE(
	    Equals_Internal(World.OverlapAll(Area, {}, FWorldQueryFilter{}), {SelfHead, SelfBody, Far, Near, Multi, High}));
	// 1ビット・複数ビット所属・最上位ビット・複数ビット検索・マスク0。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(Area, {}, Mask_Internal(PickupCategory)), {Near, Multi}));
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(Area, {}, Mask_Internal(HighCategory)), {High}));
	PHYSICS_REQUIRE(
	    Equals_Internal(World.OverlapAll(Area, {}, Mask_Internal(PickupCategory | HighCategory)), {Near, Multi, High}));
	PHYSICS_REQUIRE(World.OverlapAll(Area, {}, Mask_Internal(0u)).IsEmpty());
	// 自己Bodyの一致Colliderはすべて除外し、別Bodyの一致Colliderは残す。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(Area, Self, Mask_Internal(CharacterCategory)), {Far, Multi}));
	PHYSICS_REQUIRE(
	    Equals_Internal(World.OverlapAll(Area, Other, Mask_Internal(CharacterCategory)), {SelfHead, SelfBody}));
	PHYSICS_REQUIRE(World.GetColliderQueryCategory(Hidden) == 0u);
	// 返したIDはWorld・Body・Colliderの世代を含む完全なID。
	const auto Result = World.OverlapAll(Area, {}, Mask_Internal(HighCategory));
	PHYSICS_REQUIRE(Result[0].Body == Other && Result[0].Index == High.Index &&
	                Result[0].Generation == High.Generation);
}

// Stepなしの移動・回転・着脱・削除・再生成・カテゴリ変更の即時反映。取得済みの配列は値として残る（W07・W08・W17）。
template <typename T> void Immediate_Internal()
{
	typename T::FWorld World;
	const auto Body = T::Body(World, T::At(0), EBodyType::Dynamic);
	const auto First = T::Box(World, Body, T::At(0), 2, 0.25f);
	const auto Area = T::Area(T::At(0, 1.5f), 0.5f);
	PHYSICS_REQUIRE(World.OverlapAll(Area).IsEmpty());
	T::Move(World, Body, T::At(0), HalfPi);
	const auto Rotated = World.OverlapAll(Area);
	PHYSICS_REQUIRE(Equals_Internal(Rotated, {First}));
	T::Move(World, Body, T::At(10), 0);
	PHYSICS_REQUIRE(World.OverlapAll(Area).IsEmpty());
	// 取得済みの配列はWorldの変更後も値として残る。
	PHYSICS_REQUIRE(Rotated.Size() == 1 && Rotated[0] == First);
	T::Move(World, Body, T::At(0), HalfPi);
	World.SetColliderQueryCategory(First, 0u);
	PHYSICS_REQUIRE(World.OverlapAll(Area).IsEmpty());
	World.SetColliderQueryCategory(First, 1u);
	PHYSICS_REQUIRE(World.DetachCollider(First));
	PHYSICS_REQUIRE(World.OverlapAll(Area).IsEmpty());
	// Collider単独の再利用。以前のカテゴリ・世代を持ち越さない。
	const auto Reused = T::Ball(World, Body, T::At(0), 3, PickupCategory);
	PHYSICS_REQUIRE(Reused.Index == First.Index && Reused.Generation != First.Generation);
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(Area), {Reused}));
	PHYSICS_REQUIRE(World.OverlapAll(Area, {}, Mask_Internal(1u)).IsEmpty());
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 0);
	// Bodyの削除と同スロットの再生成。
	PHYSICS_REQUIRE(World.DestroyBody(Body));
	PHYSICS_REQUIRE(World.OverlapAll(Area).IsEmpty());
	const auto NewBody = T::Body(World, T::At(0));
	PHYSICS_REQUIRE(NewBody.Index == Body.Index && NewBody.Generation != Body.Generation);
	const auto Fresh = T::Ball(World, NewBody, T::At(0), 3);
	const auto Result = World.OverlapAll(Area);
	PHYSICS_REQUIRE(Equals_Internal(Result, {Fresh}) && !(Result[0] == Reused) && !World.IsColliderAlive(Reused));
	// 別Worldの同じスロットのIDは別物で、除外指定に使えない。
	typename T::FWorld Other;
	const auto Foreign = T::Body(Other, T::At(0));
	PHYSICS_REQUIRE(Foreign.Index == NewBody.Index);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.OverlapAll(Area, Foreign);
	    }));
	// World破棄後も配列は読める（World内部の借用ではない）。
	TVector<typename T::FColliderId> Saved;
	{
		typename T::FWorld Temporary;
		const auto Id = T::Ball(Temporary, T::Body(Temporary, T::At(0)), T::At(0), 1);
		Saved = Temporary.OverlapAll(T::Area(T::At(0), 1));
		PHYSICS_REQUIRE(Saved.Size() == 1 && Saved[0] == Id);
	}
	PHYSICS_REQUIRE(Saved.Size() == 1 && Saved[0].Index == 0);
}

// 入力・状態・除外IDの検証はマスク0や空Worldでも省略しない。後続の対象形状の異常を隠さない（W09〜W13）。
template <typename T> void Failures_Internal()
{
	typename T::FWorld World;
	const f32 Nan = TNumericLimits<f32>::QuietNaN();
	const f32 Max = TNumericLimits<f32>::Max();
	const FWorldQueryFilter None = Mask_Internal(0u);
	// 定数式の桁あふれ警告を避け、実行時の乗算で無限大の半径を作る。
	f32 Scale = 2;
	const f32 Inf = Max * Scale;
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.OverlapAll(T::Area(T::At(Nan), 1), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.OverlapAll(T::Area(T::At(0), -1), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.OverlapAll(T::Area(T::At(0), Inf), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.OverlapAll(T::Area(T::At(0), 1), typename T::FBodyId{}, None);
	    }));
	// 先に正常な一致、後ろに変換不能な対象形状。全体が失敗する。
	const auto First = T::Body(World, T::At(0), EBodyType::Dynamic);
	const auto Leading = T::Ball(World, First, T::At(0), 1, ObstacleCategory);
	const auto Huge = T::Body(World, T::At(Max));
	const auto Broken = T::Ball(World, Huge, T::At(Max), 1, PickupCategory);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.OverlapAll(T::Area(T::At(0), 1));
	    }));
	// カテゴリ・Bodyで除外すれば、その形状は計算しない。
	PHYSICS_REQUIRE(
	    Equals_Internal(World.OverlapAll(T::Area(T::At(0), 1), {}, Mask_Internal(ObstacleCategory)), {Leading}));
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(0), 1), Huge), {Leading}));
	World.SetColliderQueryCategory(Broken, ObstacleCategory);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.OverlapAll(T::Area(T::At(0), 1), {}, Mask_Internal(ObstacleCategory));
	    }));
	PHYSICS_REQUIRE(World.DestroyBody(Huge));
	// 入力エラー・計算エラーの後もWorldは使用可能（W13）。Stepの引数検査の失敗でも同じ。
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(0), 1)), {Leading}));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.Step(0);
	    }));
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(0), 1)), {Leading}));
	// 有効な引数で開始したStepの途中失敗後は拒否し、正常Stepで回復する（W12）。
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
		    (void)World.OverlapAll(T::Area(T::At(0), 1), {}, None);
	    }));
	World.SetExecutionSettings({});
	World.Step(.5, 5);
	PHYSICS_REQUIRE(Equals_Internal(World.OverlapAll(T::Area(T::At(0), 1)), {Leading}));
}

// 問い合わせあり／なしの2つのWorldへ同じ外力を与え、状態とStep数が一致する（W14）。
template <typename T> void ReadOnly_Internal()
{
	typename T::FWorld Queried;
	typename T::FWorld Control;
	T::NoGravity(Queried);
	T::NoGravity(Control);
	const auto A = T::Body(Queried, T::At(0), EBodyType::Dynamic);
	const auto B = T::Body(Control, T::At(0), EBodyType::Dynamic);
	T::Box(Queried, A, T::At(0), 1, 0.5f, CharacterCategory);
	T::Box(Control, B, T::At(0), 1, 0.5f, CharacterCategory);
	T::Push(Queried, A);
	T::Push(Control, B);
	const typename T::FWorld& Read = Queried;
	for (int32 Index = 0; Index < 50; ++Index)
	{
		PHYSICS_REQUIRE(Read.OverlapAll(T::Area(T::At(0), 3)).Size() == 1);
		PHYSICS_REQUIRE(Read.OverlapAll(T::Area(T::At(0), 3), A).IsEmpty());
	}
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 0);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		(void)Read.OverlapAll(T::Area(T::At(0), 20));
		Queried.Step(.01);
		Control.Step(.01);
		PHYSICS_REQUIRE(T::SameState(Queried, A, Control, B));
	}
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 20);
}

// 接地・休止中のBodyも取得し、問い合わせで起こさない。カテゴリ0でも接触とSnapshotは従来どおり（W15・W19）。
template <typename T> void SleepingAndContacts_Internal()
{
	typename T::FWorld Hidden;
	typename T::FWorld Control;
	typename T::FBodyId Bodies[2];
	typename T::FColliderId Colliders[2];
	typename T::FWorld* Worlds[2] = {&Hidden, &Control};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		typename T::FWorld& World = *Worlds[Index];
		T::Box(World, T::Body(World, T::At(0, -1)), T::At(0), 5, 1, Index == 0 ? 0u : 1u);
		Bodies[Index] = T::Body(World, T::At(0, 0.6f), EBodyType::Dynamic);
		Colliders[Index] = T::Box(World, Bodies[Index], T::At(0), 0.5f, 0.5f, Index == 0 ? 0u : CharacterCategory);
		for (int32 StepIndex = 0; StepIndex < 600; ++StepIndex)
		{
			World.Step(1.0 / 120.0);
		}
	}
	PHYSICS_REQUIRE(T::SameState(Hidden, Bodies[0], Control, Bodies[1]));
	PHYSICS_REQUIRE(Control.IsSleeping(Bodies[1]));
	PHYSICS_REQUIRE(Hidden.CaptureSnapshot().Colliders.Size() == 2);
	PHYSICS_REQUIRE(Hidden.OverlapAll(T::Area(T::At(0, 0.5f), 2)).IsEmpty());
	const auto Position = Control.GetPosition(Bodies[1]);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		const auto Found = Control.OverlapAll(T::Area(T::At(0, 0.5f), 2), {}, Mask_Internal(CharacterCategory));
		PHYSICS_REQUIRE(Equals_Internal(Found, {Colliders[1]}));
	}
	PHYSICS_REQUIRE(Control.IsSleeping(Bodies[1]) && Control.GetPosition(Bodies[1]) == Position);
	PHYSICS_REQUIRE(Control.CaptureSnapshot().StepIndex == 600);
}

// Debug表示の上限や固定バッファで切り詰めない（W16）。
template <typename T> void Unlimited_Internal()
{
	typename T::FWorld World;
	const auto Body = T::Body(World, T::At(0));
	TVector<typename T::FColliderId> Expected;
	for (int32 Index = 0; Index < 300; ++Index)
	{
		T::Ball(World, Body, T::At(static_cast<f32>(Index), 50), 0.5f, PickupCategory);
		Expected.PushBack(T::Ball(World, Body, T::At(static_cast<f32>(Index) * 0.01f), 0.5f, CharacterCategory));
	}
	const auto Result = World.OverlapAll(T::Area(T::At(0), 10), {}, Mask_Internal(CharacterCategory));
	PHYSICS_REQUIRE(Result.Size() == 300 && Equals_Internal(Result, Expected));
}

// ゲーム側の利用: 範囲で候補を集め、Body単位にまとめ、障害物込みの射線で遮られていない候補を選ぶ。
// 一体を一Bodyで表す前提。射線の目標点はBodyの重心とするゲーム側の方針（視野角・透明・部分可視は扱わない）。
template <typename T>
TVector<typename T::FBodyId> FindVisibleCharacters_Internal(const typename T::FWorld& World, typename T::FBodyId Self,
                                                            typename T::FVector Eye, f32 Radius)
{
	const auto Candidates = World.OverlapAll(T::Area(Eye, Radius), Self, Mask_Internal(CharacterCategory));
	TVector<typename T::FBodyId> Visible;
	TVector<typename T::FBodyId> Checked;
	for (const auto& Collider : Candidates)
	{
		// 同じBodyの複数Colliderはゲーム側で一回にまとめる（線形の重複確認）。
		bool bDuplicate = false;
		for (const auto& Body : Checked)
		{
			bDuplicate = bDuplicate || Body == Collider.Body;
		}
		if (bDuplicate)
		{
			continue;
		}
		Checked.PushBack(Collider.Body);
		const auto Target = World.GetPosition(Collider.Body);
		// 目標点が視点と同じなら、0長の射線を投げずに見えるものとする（ゲーム側の方針）。
		if (Target == Eye)
		{
			Visible.PushBack(Collider.Body);
			continue;
		}
		// 遮るものも含めたマスクで最短を調べる。キャラクターだけでは壁越しに当たってしまう。
		const auto Hit = World.RaycastClosest(Eye, Target, Self, Mask_Internal(ObstacleCategory | CharacterCategory));
		if (Hit && Hit->Collider.Body == Collider.Body)
		{
			Visible.PushBack(Collider.Body);
		}
	}
	return Visible;
}

template <typename T> void GameUsage_Internal()
{
	typename T::FWorld World;
	const auto Self = T::Body(World, T::At(0));
	T::Ball(World, Self, T::At(0), 0.5f, CharacterCategory);
	// 見通せる対象A（胴と頭の2 Collider）。
	const auto A = T::Body(World, T::At(0, 4));
	const auto ATorso = T::Ball(World, A, T::At(0), 0.5f, CharacterCategory);
	const auto AHead = T::Ball(World, A, T::At(0, 0.8f), 0.3f, CharacterCategory);
	// 壁の奥の対象B、範囲外の対象C、手前の拾得物。
	const auto Wall = T::Body(World, T::At(5));
	const auto WallCollider = T::Box(World, Wall, T::At(0), 0.5f, 2, ObstacleCategory);
	const auto B = T::Body(World, T::At(8));
	const auto BCollider = T::Ball(World, B, T::At(0), 0.5f, CharacterCategory);
	const auto C = T::Body(World, T::At(30));
	T::Ball(World, C, T::At(0), 0.5f, CharacterCategory);
	T::Ball(World, T::Body(World, T::At(2)), T::At(0), 0.5f, PickupCategory);
	// World結果はCollider単位（A 2件、B 1件）。Cと拾得物・壁は含まない。
	const auto Candidates = World.OverlapAll(T::Area(T::At(0), 10), Self, Mask_Internal(CharacterCategory));
	PHYSICS_REQUIRE(Equals_Internal(Candidates, {ATorso, AHead, BCollider}));
	// ゲーム側ではAだけが見える。Bは範囲内だが壁に遮られる。
	PHYSICS_REQUIRE(Equals_Internal(FindVisibleCharacters_Internal<T>(World, Self, T::At(0), 10), {A}));
	// キャラクターだけで射線を調べると壁越しのBに当たる（遮蔽判定にならない）ことを確認しておく。
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(8), Self, Mask_Internal(CharacterCategory))->Collider ==
	                BCollider);
	// 壁を射線の対象から外すと、Stepなしで見える側へ変わる。壁の物理登録とSnapshotは残る。
	World.SetColliderQueryCategory(WallCollider, 0u);
	PHYSICS_REQUIRE(Equals_Internal(FindVisibleCharacters_Internal<T>(World, Self, T::At(0), 10), {A, B}));
	PHYSICS_REQUIRE(World.IsColliderAlive(WallCollider) && World.CaptureSnapshot().Colliders.Size() == 7);
	World.SetColliderQueryCategory(WallCollider, ObstacleCategory);
	// 視点と同じ位置の候補は、0長の射線を投げずに見えるものとする（例外にならない）。
	// その形状は他の射線の始点を含むため割合0で最初に当たり、A・Bへの射線を遮る。
	const auto D = T::Body(World, T::At(0));
	T::Ball(World, D, T::At(0), 0.2f, CharacterCategory);
	PHYSICS_REQUIRE(Equals_Internal(FindVisibleCharacters_Internal<T>(World, Self, T::At(0), 10), {D}));
	PHYSICS_REQUIRE(World.DestroyBody(D));
	PHYSICS_REQUIRE(Equals_Internal(FindVisibleCharacters_Internal<T>(World, Self, T::At(0), 10), {A}));
	// 取得後に候補Bを削除し、同じスロットへ別Bodyを作っても、保存IDを読み替えない。
	PHYSICS_REQUIRE(World.DestroyBody(B));
	const auto Replacement = T::Body(World, T::At(8));
	PHYSICS_REQUIRE(Replacement.Index == B.Index && !(Replacement == B));
	PHYSICS_REQUIRE(!World.IsAlive(Candidates[2].Body) && !World.IsColliderAlive(Candidates[2]));
	PHYSICS_REQUIRE(World.IsAlive(Candidates[0].Body));
}

// 2D／3Dの各ケース。
const PhysicsTest::FCase Cases_Internal[] = {{"2D overlap circle and rotated box math", &CircleBoxMath_Internal},
                                             {"3D overlap sphere and OBB math", &SphereMath_Internal},
                                             {"2D overlap basics and boundaries", &Basics_Internal<F2D>},
                                             {"3D overlap basics and boundaries", &Basics_Internal<F3D>},
                                             {"2D overlap applies transforms once", &Transform_Internal<F2D>},
                                             {"3D overlap applies transforms once", &Transform_Internal<F3D>},
                                             {"2D overlap selection order and filters", &Selection_Internal<F2D>},
                                             {"3D overlap selection order and filters", &Selection_Internal<F3D>},
                                             {"2D overlap immediate state and identities", &Immediate_Internal<F2D>},
                                             {"3D overlap immediate state and identities", &Immediate_Internal<F3D>},
                                             {"2D overlap validation and failures", &Failures_Internal<F2D>},
                                             {"3D overlap validation and failures", &Failures_Internal<F3D>},
                                             {"2D overlap preserves dynamics", &ReadOnly_Internal<F2D>},
                                             {"3D overlap preserves dynamics", &ReadOnly_Internal<F3D>},
                                             {"2D overlap sleeping and contacts", &SleepingAndContacts_Internal<F2D>},
                                             {"3D overlap sleeping and contacts", &SleepingAndContacts_Internal<F3D>},
                                             {"2D overlap beyond debug limit", &Unlimited_Internal<F2D>},
                                             {"3D overlap beyond debug limit", &Unlimited_Internal<F3D>},
                                             {"2D overlap then line of sight game usage", &GameUsage_Internal<F2D>},
                                             {"3D overlap then line of sight game usage", &GameUsage_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetWorldOverlapCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
