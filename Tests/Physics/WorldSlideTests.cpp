// SPDX-License-Identifier: NOASSERTION
// 円／球の移動候補（ComputeSlideMove）: 最初の接触で1回だけ滑らせ、滑り経路の接触で止める。実World2D／3Dで実行する。
// 期待値は手計算の解析値で、製品の関数から作らない。
// 許容差の根拠: 公開座標はf64の計算をf32へ丸める。座標10前後ではf32の半ulpは2^-21（約4.8e-7）なので、
// 法線（各成分の丸め2^-25）と残り移動（約6）の積を加えても1e-6に収まる。
#include "TestCases.h"
#include "Dxf/WorldSlideMove2D.h"
#include "Dxf/WorldSlideMove3D.h"
#include "Toolbox/JobSystem.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr uint32 ObstacleCategory = 1u << 0;
constexpr uint32 CharacterCategory = 1u << 1;
constexpr uint32 PickupCategory = 1u << 2;
constexpr f64 Backoff = 0.01;
constexpr f64 CenterTolerance = 1e-6;

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
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static FProbe Probe(FVector Center, f32 Radius)
	{
		return {Center, Radius};
	}
	static f64 X(FVector Value)
	{
		return Value.X;
	}
	static f64 Y(FVector Value)
	{
		return Value.Y;
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type = EBodyType::Static)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FColliderId Ball(FWorld& World, FBodyId Body, FVector Center, f32 Radius, uint32 Category = ObstacleCategory)
	{
		FColliderDescription2D Description;
		Description.Shape = FCircle2D{Center, Radius};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	// 軸平行の箱。3Dの奥行きは十分大きい（半幅50）。
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY,
	                       uint32 Category = ObstacleCategory)
	{
		return TiltedBox(World, Body, Center, HalfX, HalfY, 0, Category);
	}
	// Z軸回りに回した箱（2Dの角度と同じ向き）。
	static FColliderId TiltedBox(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, f32 Angle,
	                             uint32 Category = ObstacleCategory)
	{
		FColliderDescription2D Description;
		Description.Shape = FOrientedBox2D{Center, {HalfX, HalfY}, Angle};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	// 動くBody用の小さい箱（2Dは正方形）。
	static FColliderId Cube(FWorld& World, FBodyId Body, f32 Half, uint32 Category)
	{
		return Box(World, Body, At(0), Half, Half, Category);
	}
	static void Move(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 0);
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

// 3D Worldの型と登録操作（Z=0の平面上に2Dと同じ配置を作る）。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FProbe = FSphere;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static FProbe Probe(FVector Center, f32 Radius)
	{
		return {Center, Radius};
	}
	static f64 X(FVector Value)
	{
		return Value.X;
	}
	static f64 Y(FVector Value)
	{
		return Value.Y;
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type = EBodyType::Static)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FColliderId Ball(FWorld& World, FBodyId Body, FVector Center, f32 Radius, uint32 Category = ObstacleCategory)
	{
		FColliderDescription3D Description;
		Description.Shape = FSphere{Center, Radius};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY,
	                       uint32 Category = ObstacleCategory)
	{
		return TiltedBox(World, Body, Center, HalfX, HalfY, 0, Category);
	}
	static FColliderId TiltedBox(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, f32 Angle,
	                             uint32 Category = ObstacleCategory)
	{
		FOBB Shape{Center, {HalfX, HalfY, 50}};
		const f32 Cosine = static_cast<f32>(Cos(f64(Angle)));
		const f32 Sine = static_cast<f32>(Sin(f64(Angle)));
		Shape.Axes = {FVector3{Cosine, Sine, 0}, FVector3{-Sine, Cosine, 0}, FVector3{0, 0, 1}};
		FColliderDescription3D Description;
		Description.Shape = Shape;
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	// 動くBody用の小さい立方体（奥行き50の壁用の箱は休止しにくいので使わない）。
	static FColliderId Cube(FWorld& World, FBodyId Body, f32 Half, uint32 Category)
	{
		FColliderDescription3D Description;
		Description.Shape = FOBB{At(0), {Half, Half, Half}};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static void Move(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, FQuaternion{});
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

// 左面x=5の十分に高い壁（中心(6,0)、半幅(1,50)）。
template <typename T> typename T::FColliderId Wall_Internal(typename T::FWorld& World, typename T::FBodyId Body)
{
	return T::Box(World, Body, T::At(6), 1, 50);
}

// 空World・非交差・移動なし・マスク0。
template <typename T> void Basics_Internal()
{
	typename T::FWorld World;
	const auto Reach = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff);
	PHYSICS_REQUIRE(Reach.Stop == EWorldSlideStop::ReachedDesiredEnd && Reach.EndCenter == T::At(10, 4) &&
	                !Reach.FirstHit && !Reach.SlideHit);
	const auto Still = ComputeSlideMove(World, T::Probe(T::At(1, 1), 0.5f), T::At(1, 1), Backoff);
	PHYSICS_REQUIRE(Still.Stop == EWorldSlideStop::NoMovement && Still.EndCenter == T::At(1, 1) && !Still.FirstHit);
	const auto Body = T::Body(World, T::At(0));
	const auto Ball = T::Ball(World, Body, T::At(0), 1);
	// 移動なしでも開始時の重なりは初期接触（NoMovementにしない）。
	const auto Overlap = ComputeSlideMove(World, T::Probe(T::At(0.5f), 0.5f), T::At(0.5f), Backoff);
	PHYSICS_REQUIRE(Overlap.Stop == EWorldSlideStop::InitialContact && Overlap.EndCenter == T::At(0.5f) &&
	                Overlap.FirstHit && Overlap.FirstHit->Collider == Ball && Overlap.FirstHit->bInitialContact &&
	                !Overlap.SlideHit);
	// マスク0は正常な非交差。
	const auto None = ComputeSlideMove(World, T::Probe(T::At(0.5f), 0.5f), T::At(10), Backoff, {}, Mask_Internal(0u));
	PHYSICS_REQUIRE(None.Stop == EWorldSlideStop::ReachedDesiredEnd && None.EndCenter == T::At(10) && !None.FirstHit);
}

// 垂直壁で停止、斜めに入って1回滑る、滑り経路の薄い板で停止（指令書の解析配置）。
template <typename T> void Walls_Internal()
{
	typename T::FWorld World;
	const auto Body = T::Body(World, T::At(0));
	const auto Wall = Wall_Internal<T>(World, Body);
	// 正面: 中心x=4.5（割合0.45）で接し、経路に沿って0.01戻す。残りは全部内向きなので滑らない。
	const auto Head = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10), Backoff);
	PHYSICS_REQUIRE(Head.Stop == EWorldSlideStop::Blocked && Head.FirstHit && Head.FirstHit->Collider == Wall &&
	                Near_Internal(Head.FirstHit->Fraction, 0.45, 1e-12) &&
	                Near_Internal(T::X(Head.EndCenter), 4.49, CenterTolerance) && T::Y(Head.EndCenter) == 0 &&
	                !Head.SlideHit);
	// 斜め: (0,0)→(10,4)、L=√116。後退後の中心は(4.5-10*0.01/L, 1.8-4*0.01/L)、滑り先のyは4。
	const f64 Length = Sqrt(116.0);
	const f64 AfterX = 4.5 - 10 * Backoff / Length;
	const auto Slide = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff);
	PHYSICS_REQUIRE(Slide.Stop == EWorldSlideStop::SlideCompleted && Slide.FirstHit &&
	                Slide.FirstHit->Collider == Wall && Near_Internal(Slide.FirstHit->Fraction, 0.45, 1e-12) &&
	                Near_Internal(T::X(Slide.EndCenter), AfterX, CenterTolerance) && T::Y(Slide.EndCenter) == 4 &&
	                !Slide.SlideHit);
	// 滑り経路の途中に厚さ0の板（下面y=3、x∈[3.1,4.9]）。中心y=2.5で接し、滑り方向へ0.01戻したy=2.49で止まる。
	// 滑り先(x,4)は板から1離れていて空いているが、途中の板を飛び越さない。
	const auto Plate = T::Box(World, Body, T::At(4, 3), 0.9f, 0);
	const auto Stopped = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff);
	const f64 AfterY = 1.8 - 4 * Backoff / Length;
	PHYSICS_REQUIRE(Stopped.Stop == EWorldSlideStop::Blocked && Stopped.FirstHit &&
	                Stopped.FirstHit->Collider == Wall && Stopped.SlideHit && Stopped.SlideHit->Collider == Plate &&
	                !Stopped.SlideHit->bInitialContact &&
	                Near_Internal(Stopped.SlideHit->Fraction, (2.5 - AfterY) / (4 - AfterY), CenterTolerance) &&
	                Near_Internal(T::X(Stopped.EndCenter), AfterX, CenterTolerance) &&
	                Near_Internal(T::Y(Stopped.EndCenter), 2.49, CenterTolerance));
	PHYSICS_REQUIRE(Stopped.SlideHit->Normal && Stopped.SlideHit->Normal->Y == -1);
	PHYSICS_REQUIRE(World.OverlapAll(T::Probe(T::At(static_cast<f32>(AfterX), 4), 0.5f)).Size() == 0);
}

// 開始時の接触・重なり（離れる向きでも）、終点接触、後退距離が接触までより長い、接線に再接触。
template <typename T> void Boundaries_Internal()
{
	typename T::FWorld World;
	const auto Body = T::Body(World, T::At(0));
	const auto Wall = Wall_Internal<T>(World, Body);
	const auto Inside = ComputeSlideMove(World, T::Probe(T::At(4.8f), 0.5f), T::At(10, 4), Backoff);
	PHYSICS_REQUIRE(Inside.Stop == EWorldSlideStop::InitialContact && Inside.EndCenter == T::At(4.8f) &&
	                Inside.FirstHit && Inside.FirstHit->bInitialContact && !Inside.SlideHit);
	const auto Away = ComputeSlideMove(World, T::Probe(T::At(4.5f), 0.5f), T::At(0, 3), Backoff);
	PHYSICS_REQUIRE(Away.Stop == EWorldSlideStop::InitialContact && Away.EndCenter == T::At(4.5f) &&
	                Away.FirstHit->Collider == Wall && !Away.FirstHit->Normal);
	// 終点でちょうど接する: 割合1、0.01戻して止まる（残りは内向きだけ）。
	const auto Touch = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(4.5f), Backoff);
	PHYSICS_REQUIRE(Touch.Stop == EWorldSlideStop::Blocked && Touch.FirstHit->Fraction == 1 &&
	                Near_Internal(T::X(Touch.EndCenter), 4.49, CenterTolerance));
	// 後退距離1は接触まで（0.5）より長い: 割合0へ切り詰め、開始中心そのもの（再検査なしで保持）。
	const auto Long = ComputeSlideMove(World, T::Probe(T::At(4), 0.5f), T::At(10), 1.0);
	PHYSICS_REQUIRE(Long.Stop == EWorldSlideStop::Blocked && Long.EndCenter == T::At(4) && Long.FirstHit);
	// 接線: 対象(5,101.5)半径1、y=100を右へ。x=5で接し法線(0,-1)。残りは全部接線方向なので(10,100)へ滑るが、
	// 0.01手前(4.99)から同じ円に再び接するため、その場（割合0へ切り詰めた手前）で止まる。ヒットは消さない。
	const auto Round = T::Ball(World, Body, T::At(5, 101.5f), 1);
	const auto Tangent = ComputeSlideMove(World, T::Probe(T::At(0, 100), 0.5f), T::At(10, 100), Backoff);
	PHYSICS_REQUIRE(Tangent.Stop == EWorldSlideStop::Blocked && Tangent.FirstHit &&
	                Tangent.FirstHit->Collider == Round && Tangent.FirstHit->Normal &&
	                Tangent.FirstHit->Normal->Y == -1 && Tangent.SlideHit && Tangent.SlideHit->Collider == Round &&
	                Near_Internal(T::X(Tangent.EndCenter), 4.99, CenterTolerance) && T::Y(Tangent.EndCenter) == 100);
}

// 箱の角（2D）／辺（3D、Z方向の辺）の斜めの法線で滑る。空の法線は非交差として扱わない。
template <typename T> void Normals_Internal()
{
	typename T::FWorld World;
	const auto Body = T::Body(World, T::At(0));
	// 左下の角(5,0.5)（中心(6,1.5)、半幅1）、半径1でX軸上を右へ。中心x=5-√3/2で接し、法線(-√3/2,-1/2)。
	// 後退後A=(5-√3/2-0.01, 0)、残りR=(10-Ax, 0)。内向き成分を除くと滑り=(R/4, -√3R/4)。
	const auto Corner = T::Box(World, Body, T::At(6, 1.5f), 1, 1);
	const f64 Ax = 5 - Sqrt(3.0) / 2 - Backoff;
	const f64 Rest = 10 - Ax;
	const auto Slide = ComputeSlideMove(World, T::Probe(T::At(0), 1), T::At(10), Backoff);
	PHYSICS_REQUIRE(Slide.Stop == EWorldSlideStop::SlideCompleted && Slide.FirstHit &&
	                Slide.FirstHit->Collider == Corner && Slide.FirstHit->Normal &&
	                Near_Internal(T::X(Slide.EndCenter), Ax + Rest / 4, CenterTolerance) &&
	                Near_Internal(T::Y(Slide.EndCenter), -Sqrt(3.0) / 4 * Rest, CenterTolerance) && !Slide.SlideHit);
	// 法線が空（半径1e-6で±1e7の長い移動。ヒットは成立）: 接触の手前で止め、終点へは進めない。
	const auto Tiny = T::Ball(World, Body, T::At(0, 1e-6f), 1e-6f);
	const auto Lost = ComputeSlideMove(World, T::Probe(T::At(-1e7f), 1e-6f), T::At(1e7f), Backoff);
	PHYSICS_REQUIRE(Lost.FirstHit && Lost.FirstHit->Collider == Tiny && !Lost.FirstHit->bInitialContact &&
	                !Lost.FirstHit->Normal);
	PHYSICS_REQUIRE(Lost.Stop == EWorldSlideStop::MissingNormal && !Lost.SlideHit && T::X(Lost.EndCenter) < -0.009 &&
	                T::X(Lost.EndCenter) > -0.011);
}

// 3DのXY面外: (0,0,0)→(10,0,4)。壁で後退した後、Z方向へ4まで滑る。
void OutOfPlane3D_Internal()
{
	FPhysicsWorld3D World;
	const auto Body = F3D::Body(World, F3D::At(0));
	Wall_Internal<F3D>(World, Body);
	const f64 Length = Sqrt(116.0);
	const auto Slide = ComputeSlideMove(World, FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 4}, Backoff);
	PHYSICS_REQUIRE(Slide.Stop == EWorldSlideStop::SlideCompleted && Slide.FirstHit &&
	                Near_Internal(Slide.EndCenter.X, 4.5 - 10 * Backoff / Length, CenterTolerance) &&
	                Slide.EndCenter.Y == 0 && Slide.EndCenter.Z == 4);
}

// 入力検査、丸めた候補の再検査、進めない滑り、極小移動、滑り終点の表現不能。
template <typename T> void Numeric_Internal()
{
	typename T::FWorld World;
	const f32 Max = TNumericLimits<f32>::Max();
	const f32 Nan = TNumericLimits<f32>::QuietNaN();
	// 定数式の桁あふれ警告を避け、実行時の乗算で無限大を作る。
	f32 Scale = 2;
	const f32 Inf = Max * Scale;
	const f64 Invalid[] = {0, -0.01, f64(Nan), f64(Inf)};
	for (const f64 Distance : Invalid)
	{
		PHYSICS_REQUIRE(Throws_Internal(
		    [&]
		    {
			    (void)ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(1), Distance);
		    }));
	}
	const f32 Radii[] = {0, -1, Nan, Inf};
	for (const f32 Radius : Radii)
	{
		PHYSICS_REQUIRE(Throws_Internal(
		    [&]
		    {
			    (void)ComputeSlideMove(World, T::Probe(T::At(0), Radius), T::At(1), Backoff);
		    }));
	}
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(Nan), Backoff);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)ComputeSlideMove(World, T::Probe(T::At(-Max), 0.5f), T::At(Max), Backoff, {}, Mask_Internal(0u));
	    }));
	// 2^20付近（f32の間隔0.125）。接触中心2^20+4.5から0.001戻した候補は2^20+4.5へ丸められ、
	// 再検査で接触するため採用せず、開始中心で止まる。0.2戻すと2^20+4.25へ丸められ、再検査を通る。
	const f32 Base = 1048576;
	const auto Body = T::Body(World, T::At(0));
	T::Box(World, Body, T::At(Base + 6), 1, 50);
	const auto Rounded = ComputeSlideMove(World, T::Probe(T::At(Base), 0.5f), T::At(Base + 10), 0.001);
	PHYSICS_REQUIRE(Rounded.Stop == EWorldSlideStop::PrecisionLimit && Rounded.EndCenter == T::At(Base) &&
	                Rounded.FirstHit && Near_Internal(Rounded.FirstHit->Fraction, 0.45, 1e-12));
	const auto Coarse = ComputeSlideMove(World, T::Probe(T::At(Base), 0.5f), T::At(Base + 10), 0.2);
	PHYSICS_REQUIRE(Coarse.Stop == EWorldSlideStop::Blocked && Coarse.EndCenter == T::At(Base + 4.25f));
	// 1e6付近（間隔0.0625）で1e-6radだけ傾いた壁: 滑りは(0,-5.75e-6)程度で、f32では同じ中心へ丸められる。
	const f32 Far = 1000000;
	T::TiltedBox(World, Body, T::At(Far + 6, Far), 1, 3, 1e-6f);
	const auto Stuck = ComputeSlideMove(World, T::Probe(T::At(Far, Far), 0.5f), T::At(Far + 10, Far), 0.25);
	PHYSICS_REQUIRE(Stuck.Stop == EWorldSlideStop::PrecisionLimit && Stuck.FirstHit && Stuck.FirstHit->Normal &&
	                Stuck.EndCenter == T::At(Far + 4.25f, Far) && !Stuck.SlideHit);
	// 極小移動。
	const auto Small = ComputeSlideMove(World, T::Probe(T::At(0, -100), 0.5f), T::At(1e-6f, -100), Backoff);
	PHYSICS_REQUIRE(Small.Stop == EWorldSlideStop::ReachedDesiredEnd && Small.EndCenter == T::At(1e-6f, -100));
	// 滑り終点がf32で表現できない: 最初の移動（SweepClosest）は法線付きで成功するが、滑り終点のyが-3.55e38になる。
	// 例外で通知し、呼出し側の以前の結果は変えない。
	const auto Giant = T::Body(World, T::At(2e38f, -2e38f));
	T::Ball(World, Giant, T::At(0), 1e38f);
	const auto Probe = T::Probe(T::At(0, -3e38f), 1e37f);
	const auto Direct = World.SweepClosest(Probe, T::At(3e38f, -3e38f));
	PHYSICS_REQUIRE(Direct && !Direct->bInitialContact && Direct->Normal);
	auto Previous = Coarse;
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    Previous = ComputeSlideMove(World, Probe, T::At(3e38f, -3e38f), 1e36);
	    }));
	PHYSICS_REQUIRE(Previous.Stop == EWorldSlideStop::Blocked && Previous.EndCenter == T::At(Base + 4.25f));
}

// 自己除外（複数Collider）とFilterをすべての問い合わせで使う。同割合の順序、無効ID、即時反映。
template <typename T> void Registration_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World, T::At(0));
	const auto Wall = Wall_Internal<T>(World, Level);
	const auto Self = T::Body(World, T::At(0));
	T::Ball(World, Self, T::At(0), 0.5f, CharacterCategory);
	T::Ball(World, Self, T::At(0.3f), 0.5f, CharacterCategory);
	// 滑り経路（x≈4.499を上へ）にだけある、自己Bodyの別Colliderと、対象外カテゴリのCollider。
	T::Ball(World, Self, T::At(4.5f, 3.2f), 0.2f, CharacterCategory);
	T::Ball(World, Level, T::At(4.5f, 3.6f), 0.2f, PickupCategory);
	const FWorldQueryFilter Mask = Mask_Internal(ObstacleCategory | CharacterCategory);
	const auto Excluded = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff, Self, Mask);
	PHYSICS_REQUIRE(Excluded.Stop == EWorldSlideStop::SlideCompleted && Excluded.FirstHit->Collider == Wall &&
	                T::Y(Excluded.EndCenter) == 4 && !Excluded.SlideHit);
	const auto Included = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff, {}, Mask);
	PHYSICS_REQUIRE(Included.Stop == EWorldSlideStop::InitialContact && Included.EndCenter == T::At(0));
	// 同じ割合の二つの壁は、先のスロットのColliderを返す。
	const auto Twin = Wall_Internal<T>(World, T::Body(World, T::At(0)));
	const auto Tie = ComputeSlideMove(World, T::Probe(T::At(0, -20), 0.5f), T::At(10, -20), Backoff, Self, Mask);
	PHYSICS_REQUIRE(Tie.FirstHit && Tie.FirstHit->Collider == Wall);
	// カテゴリ0・マスク0・移動・Attach・Detach・DestroyをStepなしで反映する。
	World.SetColliderQueryCategory(Wall, 0u);
	World.SetColliderQueryCategory(Twin, 0u);
	const auto Open = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff, Self, Mask);
	PHYSICS_REQUIRE(Open.Stop == EWorldSlideStop::ReachedDesiredEnd && Open.EndCenter == T::At(10, 4));
	World.SetColliderQueryCategory(Wall, ObstacleCategory);
	PHYSICS_REQUIRE(
	    ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff, Self, Mask_Internal(0u)).Stop ==
	    EWorldSlideStop::ReachedDesiredEnd);
	T::Move(World, Level, T::At(20));
	PHYSICS_REQUIRE(ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff, Self, Mask).Stop ==
	                EWorldSlideStop::ReachedDesiredEnd);
	T::Move(World, Level, T::At(0));
	// 経路上（y=0.4x）の(3,1.2)へ別Bodyの円を追加する。
	const auto Blocker = T::Ball(World, T::Body(World, T::At(0)), T::At(3, 1.2f), 0.5f);
	const auto Attached = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff, Self, Mask);
	PHYSICS_REQUIRE(Attached.FirstHit && Attached.FirstHit->Collider == Blocker);
	PHYSICS_REQUIRE(World.DetachCollider(Blocker));
	const auto Detached = ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff, Self, Mask);
	PHYSICS_REQUIRE(Detached.FirstHit && Detached.FirstHit->Collider == Wall);
	PHYSICS_REQUIRE(World.DestroyBody(Level));
	PHYSICS_REQUIRE(ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff, Self, Mask).Stop ==
	                EWorldSlideStop::ReachedDesiredEnd);
	// 削除済み・別WorldのBodyを除外IDにすると、マスク0でも例外。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(1), Backoff, Level, Mask_Internal(0u));
	    }));
	typename T::FWorld Other;
	const auto Foreign = T::Body(Other, T::At(0));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)ComputeSlideMove(World, T::Probe(T::At(0), 0.5f), T::At(1), Backoff, Foreign, Mask_Internal(0u));
	    }));
}

// World状態（Step前・引数不正Step・途中失敗Step・回復）、問い合わせあり／なしの数値経過、休止。
template <typename T> void WorldState_Internal()
{
	typename T::FWorld Queried;
	typename T::FWorld Control;
	T::NoGravity(Queried);
	T::NoGravity(Control);
	const auto A = T::Body(Queried, T::At(6), EBodyType::Dynamic);
	const auto B = T::Body(Control, T::At(6), EBodyType::Dynamic);
	T::Box(Queried, A, T::At(0), 1, 3);
	T::Box(Control, B, T::At(0), 1, 3);
	T::Push(Queried, A);
	T::Push(Control, B);
	const typename T::FWorld& Read = Queried;
	PHYSICS_REQUIRE(ComputeSlideMove(Read, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff).FirstHit);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		(void)ComputeSlideMove(Read, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff);
		(void)ComputeSlideMove(Read, T::Probe(T::At(0), 0.5f), T::At(10), Backoff);
		Queried.Step(.01);
		Control.Step(.01);
		PHYSICS_REQUIRE(T::SameState(Queried, A, Control, B));
	}
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 20);
	// Stepの引数検査だけの失敗では禁止しない。途中失敗したStepの後は移動0・マスク0でも拒否し、正常Stepで回復する。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    Queried.Step(0);
	    }));
	PHYSICS_REQUIRE(ComputeSlideMove(Read, T::Probe(T::At(0), 0.5f), T::At(10, 4), Backoff).FirstHit);
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings BrokenJobs;
	BrokenJobs.JobSystem = &Jobs;
	Queried.SetExecutionSettings(BrokenJobs);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    Queried.Step(.25, 1);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)ComputeSlideMove(Read, T::Probe(T::At(-50), 0.5f), T::At(-50), Backoff, {}, Mask_Internal(0u));
	    }));
	Queried.SetExecutionSettings({});
	Queried.Step(.01);
	PHYSICS_REQUIRE(ComputeSlideMove(Read, T::Probe(T::At(-50), 0.5f), T::At(-50), Backoff).Stop ==
	                EWorldSlideStop::NoMovement);
	// 休止中のBodyも対象で、問い合わせで起こさない。
	typename T::FWorld Resting;
	T::Box(Resting, T::Body(Resting, T::At(0, -1)), T::At(0), 5, 1);
	const auto Sleeper = T::Body(Resting, T::At(0, 0.6f), EBodyType::Dynamic);
	const auto Resident = T::Cube(Resting, Sleeper, 0.5f, CharacterCategory);
	for (int32 Index = 0; Index < 600; ++Index)
	{
		Resting.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Resting.IsSleeping(Sleeper));
	const auto Position = Resting.GetPosition(Sleeper);
	for (int32 Index = 0; Index < 50; ++Index)
	{
		const auto Move = ComputeSlideMove(Resting, T::Probe(T::At(-5, 0.6f), 0.25f), T::At(5, 2), Backoff, {},
		                                   Mask_Internal(CharacterCategory));
		PHYSICS_REQUIRE(Move.FirstHit && Move.FirstHit->Collider == Resident && !Move.FirstHit->bInitialContact);
	}
	PHYSICS_REQUIRE(Resting.IsSleeping(Sleeper) && Resting.GetPosition(Sleeper) == Position);
	PHYSICS_REQUIRE(Resting.CaptureSnapshot().StepIndex == 600);
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D slide basics", &Basics_Internal<F2D>},
    {"3D slide basics", &Basics_Internal<F3D>},
    {"2D slide walls stop slide and second stop", &Walls_Internal<F2D>},
    {"3D slide walls stop slide and second stop", &Walls_Internal<F3D>},
    {"2D slide boundaries", &Boundaries_Internal<F2D>},
    {"3D slide boundaries", &Boundaries_Internal<F3D>},
    {"2D slide corner normal and missing normal", &Normals_Internal<F2D>},
    {"3D slide edge normal and missing normal", &Normals_Internal<F3D>},
    {"3D slide out of plane", &OutOfPlane3D_Internal},
    {"2D slide numeric and precision", &Numeric_Internal<F2D>},
    {"3D slide numeric and precision", &Numeric_Internal<F3D>},
    {"2D slide registration filters and immediate state", &Registration_Internal<F2D>},
    {"3D slide registration filters and immediate state", &Registration_Internal<F3D>},
    {"2D slide world state and read-only", &WorldState_Internal<F2D>},
    {"3D slide world state and read-only", &WorldState_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetWorldSlideCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
