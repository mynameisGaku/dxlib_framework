// SPDX-License-Identifier: NOASSERTION
// 2Dの有限線分問い合わせ。数学の基準値は解析解から求め、3D版との一致を根拠にしない。
// 許容誤差: 角度0の円・矩形はf64の二次式・区間計算が厳密に近いため1e-12。
// f32の角度（π/2等）を含む場合は、cos(f32(π/2))≈-4.4e-8による局所座標の誤差を見込んで1e-6とする。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Toolbox/JobSystem.h"
#include "Toolbox/SegmentIntersection2D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// f32で表した直角。
constexpr f32 HalfPi = 1.57079632679f;
// f32で表した45度。
constexpr f32 QuarterPi = 0.785398163397f;
// f32で表した30度。
constexpr f32 SixthPi = 0.523598775598f;
// 30度回転した半幅(2,0.5)の矩形を、y=1の水平線分(-5→5)が通るときの割合。
// 局所v=-x/2+√3/2∈[-0.5,0.5]からx∈[√3-1,√3+1]、局所u=√3x/2+1/2≤2からx≤√3。入口x=√3-1。
// 回転の向きを逆にすると入口はx=-√3となり、割合は(5-√3)/10へ変わる。
const f64 TiltedFraction = (4 + Sqrt(3.0)) / 10;

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

// 値が期待値の許容範囲内か調べる。
bool Near_Internal(const TOptional<f64>& Value, f64 Expected, f64 Tolerance)
{
	return Value && Abs(*Value - Expected) <= Tolerance;
}

// 指定Bodyへローカル円を取り付ける。
FColliderId2D Circle_Internal(FPhysicsWorld2D& World, FBodyId2D Body, FVector2 Center = {}, f32 Radius = 1)
{
	// 取り付ける円の設定。
	FColliderDescription2D Description;
	Description.Shape = FCircle2D{Center, Radius};
	return World.AttachCollider(Body, Description);
}

// 指定Bodyへローカル回転矩形を取り付ける。
FColliderId2D Box_Internal(FPhysicsWorld2D& World, FBodyId2D Body, FVector2 Center, FVector2 HalfExtents, f32 Angle = 0)
{
	// 取り付ける矩形の設定。
	FColliderDescription2D Description;
	Description.Shape = FOrientedBox2D{Center, HalfExtents, Angle};
	return World.AttachCollider(Body, Description);
}

// 指定位置・種別・角度のBodyを作る。
FBodyId2D Body_Internal(FPhysicsWorld2D& World, FVector2 Position, EBodyType Type = EBodyType::Dynamic, f32 Angle = 0)
{
	// 作成するBodyの設定。
	FBodyDescription2D Description;
	Description.Type = Type;
	Description.Position = Position;
	Description.Angle = Angle;
	return World.CreateBody(Description);
}

// 円: 解析値、接線、端点、内部・境界始点、非交差、半径0の点。
void CircleMath_Internal()
{
	// 中心(0,0)・半径1の円。
	const FCircle2D Circle{{0, 0}, 1};
	// (-2,0)→(2,0)は(-1,0)で入る。距離1/全長4=0.25。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{-2, 0}, FVector2{2, 0}, Circle), 0.25, 1e-12));
	// 縦方向。(0,-10)→(0,10)と半径2の円は(0,-2)で入る。8/20=0.4。
	PHYSICS_REQUIRE(
	    Near_Internal(IntersectSegment(FVector2{0, -10}, FVector2{0, 10}, FCircle2D{{0, 0}, 2}), 0.4, 1e-12));
	// y=1の接線は(0,1)だけで接する。中央0.5。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{-2, 1}, FVector2{2, 1}, Circle), 0.5, 1e-12));
	// 終点が境界上なら1。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{-2, 0}, FVector2{-1, 0}, Circle), 1, 0));
	// 終点が手前なら非交差。
	PHYSICS_REQUIRE(!IntersectSegment(FVector2{-2, 0}, FVector2{-1.001f, 0}, Circle));
	// 内部・境界上の始点は0。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{0.5f, 0}, FVector2{3, 0}, Circle), 0, 0));
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{1, 0}, FVector2{3, 0}, Circle), 0, 0));
	// 接線より外側と、遠ざかる線分は非交差。
	PHYSICS_REQUIRE(!IntersectSegment(FVector2{-2, 1.001f}, FVector2{2, 1.001f}, Circle));
	PHYSICS_REQUIRE(!IntersectSegment(FVector2{2, 0}, FVector2{5, 0}, Circle));
	// 半径0の円は点として扱う。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{0, 0}, FVector2{2, 0}, FCircle2D{{1, 0}, 0}), 0.5, 1e-12));
	PHYSICS_REQUIRE(!IntersectSegment(FVector2{0, 0.001f}, FVector2{2, 0.001f}, FCircle2D{{1, 0}, 0}));
	// 不正形状・非有限入力は失敗。
	PHYSICS_REQUIRE(Throws_Internal(
	    []
	    {
		    (void)IntersectSegment(FVector2{0, 0}, FVector2{1, 0}, FCircle2D{{0, 0}, -1});
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    []
	    {
		    (void)IntersectSegment(FVector2{TNumericLimits<f32>::QuietNaN(), 0}, FVector2{1, 0}, FCircle2D{});
	    }));
}

// 回転矩形: 軸平行、回転、角・辺の接触、内部始点、厚さ0、非交差。
void BoxMath_Internal()
{
	// 中心(0,0)・半幅(2,1)の軸平行矩形。
	const FOrientedBox2D Box{{0, 0}, {2, 1}, 0};
	// x=-2で入る。3/10。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{-5, 0}, FVector2{5, 0}, Box), 0.3, 1e-12));
	// y=-1で入る。4/10。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{0, -5}, FVector2{0, 5}, Box), 0.4, 1e-12));
	// 上辺y=1に沿う線分は辺に接する。x=-2で入る。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{-5, 1}, FVector2{5, 1}, Box), 0.3, 1e-12));
	// 角(2,1)だけを通る斜めの線分。中央0.5。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{1, 2}, FVector2{3, 0}, Box), 0.5, 1e-12));
	// 内部始点は0、辺の外側・手前は非交差。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{0, 0}, FVector2{9, 9}, Box), 0, 0));
	PHYSICS_REQUIRE(!IntersectSegment(FVector2{-5, 1.001f}, FVector2{5, 1.001f}, Box));
	PHYSICS_REQUIRE(!IntersectSegment(FVector2{-5, 0}, FVector2{-2.001f, 0}, Box));
	// 90度回転すると、X方向の半幅は1になる。4/10。
	PHYSICS_REQUIRE(Near_Internal(
	    IntersectSegment(FVector2{-5, 0}, FVector2{5, 0}, FOrientedBox2D{{0, 0}, {2, 1}, HalfPi}), 0.4, 1e-6));
	// 45度回転した半幅(1,1)は、x=-√2に頂点を持つひし形。(5-√2)/10。
	PHYSICS_REQUIRE(
	    Near_Internal(IntersectSegment(FVector2{-5, 0}, FVector2{5, 0}, FOrientedBox2D{{0, 0}, {1, 1}, QuarterPi}),
	                  (5 - Sqrt(2.0)) / 10, 1e-6));
	// 軸に対して非対称な30度回転。回転方向の誤りを検出する。
	PHYSICS_REQUIRE(
	    Near_Internal(IntersectSegment(FVector2{-5, 1}, FVector2{5, 1}, FOrientedBox2D{{0, 0}, {2, 0.5f}, SixthPi}),
	                  TiltedFraction, 1e-6));
	// 回転を外接矩形で代用すると当たる位置(±√2の外接矩形の角付近)は、実際には当たらない。
	PHYSICS_REQUIRE(
	    !IntersectSegment(FVector2{-5, 1.3f}, FVector2{-1.3f, 1.3f}, FOrientedBox2D{{0, 0}, {1, 1}, QuarterPi}));
	// 半幅(2,0)の線分と、同じ直線上の線分・横切る線分。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{-5, 0}, FVector2{5, 0}, FOrientedBox2D{{0, 0}, {2, 0}, 0}),
	                              0.3, 1e-12));
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{0, -1}, FVector2{0, 1}, FOrientedBox2D{{0, 0}, {2, 0}, 0}),
	                              0.5, 1e-12));
	// 半幅(0,0)は点。
	PHYSICS_REQUIRE(Near_Internal(IntersectSegment(FVector2{-1, 0}, FVector2{1, 0}, FOrientedBox2D{{0, 0}, {0, 0}, 0}),
	                              0.5, 1e-12));
	// 中心から離れた位置でも中心差を先に取って計算する。
	PHYSICS_REQUIRE(Near_Internal(
	    IntersectSegment(FVector2{99995, 7}, FVector2{100005, 7}, FOrientedBox2D{{100000, 7}, {2, 1}, 0}), 0.3, 1e-12));
	// 不正形状・非有限入力は失敗。
	PHYSICS_REQUIRE(Throws_Internal(
	    []
	    {
		    (void)IntersectSegment(FVector2{0, 0}, FVector2{1, 0}, FOrientedBox2D{{0, 0}, {-1, 1}, 0});
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    []
	    {
		    (void)IntersectSegment(FVector2{0, 0}, FVector2{1, 0},
		                           FOrientedBox2D{{0, 0}, {1, 1}, TNumericLimits<f32>::QuietNaN()});
	    }));
}

// 空World、各Body種別、最短、Body角度＋Collider角度、ローカル中心、1Body複数Collider。
void WorldGeometry_Internal()
{
	// このケースだけが所有する実World。
	FPhysicsWorld2D World;
	PHYSICS_REQUIRE(!World.RaycastClosest({-2, 0}, {2, 0}));
	// ColliderなしのBodyは対象にならない。
	const auto Empty = Body_Internal(World, {0, 0}, EBodyType::Static);
	PHYSICS_REQUIRE(!World.RaycastClosest({-2, 0}, {2, 0}));
	(void)Empty;
	// 遠い静的Bodyの円。
	const auto FarBody = Body_Internal(World, {7, 0}, EBodyType::Static);
	const auto Far = Circle_Internal(World, FarBody);
	// 手前の運動学Bodyの円。
	const auto NearBody = Body_Internal(World, {3, 0}, EBodyType::Kinematic);
	const auto Near = Circle_Internal(World, NearBody);
	// 最短は(2,0)。割合0.2。
	const auto Hit = World.RaycastClosest({0, 0}, {10, 0});
	PHYSICS_REQUIRE(Hit && Hit->Collider == Near && Abs(Hit->Fraction - 0.2) < 1e-12 &&
	                Hit->Position == FVector2(2, 0));
	PHYSICS_REQUIRE(Hit->Collider.Body == NearBody && World.IsColliderAlive(Hit->Collider));
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {10, 0}, NearBody)->Collider == Far);
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0}, {1, 0}));
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {2, 0})->Fraction == 1);
	PHYSICS_REQUIRE(World.RaycastClosest({3, 0}, {4, 0})->Fraction == 0);
	// Body(0,5)を90度回転し、ローカル中心(2,0)・半幅(2,0.5)の矩形を置く。ワールド中心(0,7)、X方向の半幅0.5。
	const auto BoxBody = Body_Internal(World, {0, 5}, EBodyType::Dynamic, HalfPi);
	const auto BoxId = Box_Internal(World, BoxBody, {2, 0}, {2, 0.5f});
	const auto BoxHit = World.RaycastClosest({-3, 7}, {3, 7});
	PHYSICS_REQUIRE(BoxHit && BoxHit->Collider == BoxId && Abs(BoxHit->Fraction - 2.5 / 6) < 1e-6);
	PHYSICS_REQUIRE(Abs(BoxHit->Position.X + 0.5f) < 1e-5f && Abs(BoxHit->Position.Y - 7) < 1e-5f);
	// 同じBodyへ追加したローカル円（ローカル中心(4,0)→ワールド(0,9)）。
	const auto Offset = Circle_Internal(World, BoxBody, {4, 0}, 0.25f);
	PHYSICS_REQUIRE(World.RaycastClosest({-3, 8.5f}, {3, 8.5f})->Collider == BoxId);
	PHYSICS_REQUIRE(World.RaycastClosest({0, 10}, {0, 9})->Collider == Offset);
	// Body45度＋Collider45度＝90度。二重に回転すると180度でX方向の半幅2になり、割合が1/6へ変わる。
	const auto Composed = Body_Internal(World, {20, 0}, EBodyType::Static, QuarterPi);
	const auto ComposedBox = Box_Internal(World, Composed, {0, 0}, {2, 0.5f}, QuarterPi);
	const auto ComposedHit = World.RaycastClosest({17, 0}, {23, 0});
	PHYSICS_REQUIRE(ComposedHit && ComposedHit->Collider == ComposedBox && Abs(ComposedHit->Fraction - 2.5 / 6) < 1e-6);
	// Body角度30度・ローカル角度0の非対称な例。Body角度の向きを逆に適用すると割合が変わる。
	const auto Tilted = Body_Internal(World, {0, 40}, EBodyType::Static, SixthPi);
	const auto TiltedBox = Box_Internal(World, Tilted, {0, 0}, {2, 0.5f});
	const auto TiltedHit = World.RaycastClosest({-5, 41}, {5, 41});
	PHYSICS_REQUIRE(TiltedHit && TiltedHit->Collider == TiltedBox && Abs(TiltedHit->Fraction - TiltedFraction) < 1e-6);
	// 90度回転したBodyのローカル中心(3,0)の円は、ワールド(0,-7)。(2-0.5)/4。
	const auto Turned = Body_Internal(World, {0, -10}, EBodyType::Static, HalfPi);
	const auto TurnedCircle = Circle_Internal(World, Turned, {3, 0}, 0.5f);
	const auto TurnedHit = World.RaycastClosest({-2, -7}, {2, -7});
	PHYSICS_REQUIRE(TurnedHit && TurnedHit->Collider == TurnedCircle && Abs(TurnedHit->Fraction - 0.375) < 1e-6);
}

// 同距離はスロット昇順、疎なスロット、Stepなしの姿勢・着脱・削除・再利用の即時反映。
void ImmediateAndIdentity_Internal()
{
	// このケースだけが所有する実World。
	FPhysicsWorld2D World;
	const auto Body = World.CreateBody({});
	// 同距離の先行・後続Collider。
	const auto First = Circle_Internal(World, Body, {3, 0});
	const auto Middle = Circle_Internal(World, Body, {0, 30});
	const auto Second = Circle_Internal(World, Body, {3, 0});
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {10, 0})->Collider == First);
	// 中間のスロットを空けても、残りの順序は変わらない。
	PHYSICS_REQUIRE(World.DetachCollider(Middle));
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {10, 0})->Collider == First);
	World.SetBodyTransform(Body, {0, 4}, 0);
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 0);
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0}, {10, 0}));
	World.SetBodyTransform(Body, {0, 0}, HalfPi);
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {0, 10})->Collider == First);
	PHYSICS_REQUIRE(World.DetachCollider(First));
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {0, 10})->Collider == Second);
	// 空いたスロットを再利用するCollider単独の新世代。
	const auto Reattached = Circle_Internal(World, Body, {3, 0});
	PHYSICS_REQUIRE(Reattached.Index == First.Index || Reattached.Index == Middle.Index);
	PHYSICS_REQUIRE(!World.IsColliderAlive(First) && !World.IsColliderAlive(Middle));
	// 同距離の二つのうち、スロット番号が小さい方。
	const auto Expected = Reattached.Index < Second.Index ? Reattached : Second;
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {0, 10})->Collider == Expected);
	// 自己Bodyの全Colliderを除外する。
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0}, {0, 10}, Body));
	// 保存した結果のIDは、Body削除後に失効する。
	const auto Saved = World.RaycastClosest({0, 0}, {0, 10});
	PHYSICS_REQUIRE(World.DestroyBody(Body));
	PHYSICS_REQUIRE(!World.IsColliderAlive(Saved->Collider));
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0}, {0, 10}));
	// 同じスロットを再利用する新世代Body。
	const auto NewBody = World.CreateBody({});
	PHYSICS_REQUIRE(NewBody.Index == Body.Index && NewBody.Generation != Body.Generation);
	const auto Fresh = Circle_Internal(World, NewBody);
	PHYSICS_REQUIRE(World.RaycastClosest({-5, 0}, {5, 0})->Collider == Fresh);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({0, 0}, {0, 10}, Body);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({0, 0}, {0, 10}, FBodyId2D{});
	    }));
	// 別WorldのIDは同じスロット番号でも拒否する。
	FPhysicsWorld2D Other;
	const auto Foreign = Other.CreateBody({});
	PHYSICS_REQUIRE(Foreign.Index == NewBody.Index);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({0, 0}, {0, 10}, Foreign);
	    }));
	// 未指定と明示的な除外なしは別。未指定は通常の問い合わせ。
	PHYSICS_REQUIRE(World.RaycastClosest({-5, 0}, {5, 0}, {})->Collider == Fresh);
}

// Debug表示の件数上限より後ろのColliderも対象にする。
void Unlimited_Internal()
{
	FPhysicsWorld2D World;
	const auto Body = Body_Internal(World, {0, 0}, EBodyType::Static);
	for (int32 Index = 0; Index < 300; ++Index)
	{
		Circle_Internal(World, Body, {static_cast<f32>(Index), 50});
	}
	// 表示上限より後ろに登録する最短Collider。
	const auto Last = Circle_Internal(World, Body, {5, 0});
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {10, 0})->Collider == Last);
}

// 空Worldの不正線分、計算不能な変換、先行ヒット0の後続エラー。
void Invalid_Internal()
{
	FPhysicsWorld2D World;
	// f32で表現できる最大値。
	const f32 Max = TNumericLimits<f32>::Max();
	// 無限大と非数値。定数式の桁あふれ警告を避けるため、実行時の乗算で無限大を作る。
	f32 Scale = 2;
	const f32 Inf = Max * Scale;
	const f32 Nan = TNumericLimits<f32>::QuietNaN();
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({1, 1}, {1, 1});
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({Nan, 0}, {1, 0});
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({0, 0}, {0, Inf});
	    }));
	// 差がf32で表現できない線分。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({-Max, 0}, {Max, 0});
	    }));
	// 始点を内部に含む先行Collider（割合0）。
	const auto First = World.CreateBody({});
	Circle_Internal(World, First);
	// 変換時に桁あふれするBody。
	FBodyDescription2D Huge;
	Huge.Position = {Max, 0};
	const auto Body = World.CreateBody(Huge);
	Circle_Internal(World, Body, {Max, 0});
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({0, 0}, {1, 0});
	    }));
	// 除外したBodyの形状は計算しない。
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0}, {1, 0}, Body)->Fraction == 0);
}

// 途中失敗したStepの状態を拒否し、正常Stepで回復する。引数拒否だけでは禁止しない。
void FailedStep_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	Circle_Internal(World, World.CreateBody({}));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.Step(0);
	    }));
	PHYSICS_REQUIRE(World.RaycastClosest({-2, 0}, {2, 0}));
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	// Step内部を失敗させる実行設定。
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Jobs;
	World.SetExecutionSettings(Broken);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.Step(.25, 1);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest({-2, 0}, {2, 0});
	    }));
	World.SetExecutionSettings({});
	World.Step(.5, 5);
	PHYSICS_REQUIRE(World.RaycastClosest({-2, 0}, {2, 0}));
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 1);
}

// 問い合わせあり／なしのWorldへ同じ力・トルクを与え、位置・角度・速度・休止・Step数が一致する。
void ReadOnly_Internal()
{
	FPhysicsWorld2D Queried;
	FPhysicsWorld2D Control;
	Queried.SetGravity({0, 0});
	Control.SetGravity({0, 0});
	const auto A = Queried.CreateBody({});
	const auto B = Control.CreateBody({});
	Box_Internal(Queried, A, {0, 0}, {1, 0.5f});
	Box_Internal(Control, B, {0, 0}, {1, 0.5f});
	Queried.ApplyForce(A, {2, 3});
	Control.ApplyForce(B, {2, 3});
	Queried.ApplyTorque(A, 1.5f);
	Control.ApplyTorque(B, 1.5f);
	// const経由でのみ問い合わせる参照。
	const FPhysicsWorld2D& Read = Queried;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		PHYSICS_REQUIRE(Read.RaycastClosest({-3, 0}, {3, 0}));
	}
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 0);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		(void)Read.RaycastClosest({-20, 0}, {20, 0});
		Queried.Step(.01);
		Control.Step(.01);
		PHYSICS_REQUIRE(Queried.GetPosition(A) == Control.GetPosition(B));
		PHYSICS_REQUIRE(Queried.GetAngle(A) == Control.GetAngle(B));
		PHYSICS_REQUIRE(Queried.GetVelocity(A) == Control.GetVelocity(B));
		PHYSICS_REQUIRE(Queried.GetAngularVelocity(A) == Control.GetAngularVelocity(B));
		PHYSICS_REQUIRE(Queried.IsSleeping(A) == Control.IsSleeping(B));
	}
	// 蓄積した力が問い合わせで消えていれば、速度は0のまま。
	PHYSICS_REQUIRE(Queried.GetVelocity(A).X > 0 && Queried.GetAngularVelocity(A) > 0);
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 20);
}

// 接地して休止したBodyも選択でき、問い合わせ後も起床しない。
void Sleeping_Internal()
{
	FPhysicsWorld2D World;
	const auto Ground = Body_Internal(World, {0, -1}, EBodyType::Static);
	Box_Internal(World, Ground, {0, 0}, {5, 1});
	const auto Id = Body_Internal(World, {0, 0.5f});
	const auto Collider = Box_Internal(World, Id, {0, 0}, {0.5f, 0.5f});
	for (int32 Index = 0; Index < 600; ++Index)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(World.IsSleeping(Id));
	// 問い合わせ前の休止位置。
	const auto Position = World.GetPosition(Id);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		PHYSICS_REQUIRE(World.RaycastClosest({0, 3}, {0, -2})->Collider == Collider);
	}
	PHYSICS_REQUIRE(World.IsSleeping(Id) && World.GetPosition(Id) == Position);
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 600);
}

// 2D問い合わせの回帰一覧。
const PhysicsTest::FCase Cases_Internal[] = {
    {"2D segment circle analytic values", &CircleMath_Internal},
    {"2D segment rotated box analytic values", &BoxMath_Internal},
    {"2D world query geometry rotation and offsets", &WorldGeometry_Internal},
    {"2D world query immediate state and generations", &ImmediateAndIdentity_Internal},
    {"2D world query beyond debug limit", &Unlimited_Internal},
    {"2D world query invalid input and transforms", &Invalid_Internal},
    {"2D world query failed step recovery", &FailedStep_Internal},
    {"2D world query preserves dynamics", &ReadOnly_Internal},
    {"2D world query preserves sleeping", &Sleeping_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetWorldQuery2DCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
