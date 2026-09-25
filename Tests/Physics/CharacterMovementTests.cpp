// SPDX-License-Identifier: NOASSERTION
// キャラクター移動のPhysics部品: 接触と初期重なり、反復滑り、接地、段差・重力・ジャンプ。実World2D／3Dで実行する。
// 静的な配置の期待値は手計算の解析値で、製品の関数から作らない。複雑な経路は、障害物を越えない・制約を破らない・
// 予算内で終わることを確認する。
// 許容差の根拠: 公開座標はf32。座標10前後の半ulpは約4.8e-7で、1回の移動の比較は1e-6、60回の固定更新を
// 積み上げた位置は各回の丸め（2.4e-7程度）の和に余裕を持たせて1e-4とする。
#include "TestCases.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#include "Toolbox/JobSystem.h"
#include "Toolbox/ShapeContactQuery2D.h"
#include "Toolbox/ShapeContactQuery3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr uint32 ObstacleCategory = 1u << 0;
constexpr uint32 CharacterCategory = 1u << 1;
constexpr f64 Tolerance = 1e-6;
constexpr f64 StepSeconds = 1.0 / 60.0;
constexpr f32 SixthPi = 0.523598775598f;
constexpr f32 ThirdPi = 1.0471975512f;

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

bool Near_Internal(f64 Value, f64 Expected, f64 Limit)
{
	return Abs(Value - Expected) <= Limit;
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
	using FSettings = FCharacterMoveSettings2D;
	using FState = FCharacterState2D;
	using FInput = FCharacterMoveInput2D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static f64 X(FVector Value)
	{
		return Value.X;
	}
	static f64 Y(FVector Value)
	{
		return Value.Y;
	}
	static FBodyId Body(FWorld& World, FVector Position = {})
	{
		FBodyDescription2D Description;
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
	// Z軸回りの角度付きの箱（3Dは奥行きの半幅50）。
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, f32 Angle = 0,
	                       uint32 Category = ObstacleCategory)
	{
		FColliderDescription2D Description;
		Description.Shape = FOrientedBox2D{Center, {HalfX, HalfY}, Angle};
		Description.QueryCategory = Category;
		return World.AttachCollider(Body, Description);
	}
	static void Move(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 0);
	}
};

// 3D Worldの型と登録操作（Z=0の平面に2Dと同じ配置を作る）。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FSettings = FCharacterMoveSettings3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static f64 X(FVector Value)
	{
		return Value.X;
	}
	static f64 Y(FVector Value)
	{
		return Value.Y;
	}
	static FBodyId Body(FWorld& World, FVector Position = {})
	{
		FBodyDescription3D Description;
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
	static FColliderId Box(FWorld& World, FBodyId Body, FVector Center, f32 HalfX, f32 HalfY, f32 Angle = 0,
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
	static void Move(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, FQuaternion{});
	}
};

// 1回の固定更新を進め、結果を返す（状態を更新する）。
template <typename T>
auto Step_Internal(const typename T::FWorld& World, const typename T::FSettings& Settings, typename T::FState& State,
                   typename T::FInput Input)
{
	const auto Result = StepCharacter(World, Settings, State, Input, StepSeconds);
	State = Result.State;
	return Result;
}
template <typename T> typename T::FInput Walk_Internal(f32 X, bool bJump = false)
{
	typename T::FInput Input;
	Input.Move = T::At(X);
	Input.bJump = bJump;
	return Input;
}
template <typename T> typename T::FState StateAt_Internal(typename T::FVector Center)
{
	typename T::FState State;
	State.Center = Center;
	return State;
}
// 上面y=0の床（中心(CenterX,-1)、半幅(HalfX,1)）。
template <typename T>
typename T::FColliderId Floor_Internal(typename T::FWorld& World, typename T::FBodyId Body, f32 CenterX = 0,
                                       f32 HalfX = 50)
{
	return T::Box(World, Body, T::At(CenterX, -1), HalfX, 1);
}

// 形状の接触（Toolbox）: 解析値の距離・法線、同心と同距離の面での方向なし、実際の軸。
void ShapeContacts_Internal()
{
	const auto Gap = FindShapeContact(FCircle2D{{0, 0}, 0.5f}, FCircle2D{{2, 0}, 1});
	PHYSICS_REQUIRE(Gap.Separation == 0.5 && Gap.Normal && Gap.Normal->X == -1 && Gap.Normal->Y == 0);
	const auto Touch = FindShapeContact(FCircle2D{{1.5f, 0}, 0.5f}, FCircle2D{{0, 0}, 1});
	PHYSICS_REQUIRE(Touch.Separation == 0 && Touch.Normal && Touch.Normal->X == 1);
	const auto Shallow = FindShapeContact(FSphere{{0, 1.25f, 0}, 0.5f}, FSphere{{0, 0, 0}, 1});
	PHYSICS_REQUIRE(Shallow.Separation == -0.25 && Shallow.Normal && Shallow.Normal->Y == 1);
	const auto Concentric = FindShapeContact(FCircle2D{{3, 3}, 0.5f}, FCircle2D{{3, 3}, 1});
	PHYSICS_REQUIRE(Concentric.Separation == -1.5 && !Concentric.Normal);
	// 正方形[-1,1]^2: 外側の面・角、内部の最寄りの面、中心（四つの面が同じ距離）。
	const FOrientedBox2D Square{{0, 0}, {1, 1}, 0};
	const auto Face = FindShapeContact(FCircle2D{{3, 0}, 0.5f}, Square);
	PHYSICS_REQUIRE(Face.Separation == 1.5 && Face.Normal && Face.Normal->X == 1 && Face.Normal->Y == 0);
	const auto Corner = FindShapeContact(FCircle2D{{2, 2}, 0.5f}, Square);
	PHYSICS_REQUIRE(Near_Internal(Corner.Separation, Sqrt(2.0) - 0.5, 1e-12) && Corner.Normal &&
	                Near_Internal(Corner.Normal->X, Sqrt(0.5), 1e-7) &&
	                Near_Internal(Corner.Normal->Y, Sqrt(0.5), 1e-7));
	const auto Inside = FindShapeContact(FCircle2D{{0.5f, 0}, 0.5f}, Square);
	PHYSICS_REQUIRE(Inside.Separation == -1 && Inside.Normal && Inside.Normal->X == 1 && Inside.Normal->Y == 0);
	const auto Middle = FindShapeContact(FCircle2D{{0, 0}, 0.5f}, Square);
	PHYSICS_REQUIRE(Middle.Separation == -1.5 && !Middle.Normal);
	// +30度の矩形の内部で+Y'方向へ0.1ずれた点: 最寄りの+Y'面（距離0.4）の法線は(-sin30, cos30)。
	// 中心そのものは±Y'面が同じ距離なので、方向を一つに決められない。
	const auto Rotated =
	    FindShapeContact(FCircle2D{{-0.05f, 0.0866025f}, 0.25f}, FOrientedBox2D{{0, 0}, {2, 0.5f}, SixthPi});
	PHYSICS_REQUIRE(!FindShapeContact(FCircle2D{{0, 0}, 0.25f}, FOrientedBox2D{{0, 0}, {2, 0.5f}, SixthPi}).Normal);
	PHYSICS_REQUIRE(Near_Internal(Rotated.Separation, -0.65, 1e-6) && Rotated.Normal &&
	                Near_Internal(Rotated.Normal->X, -0.5, 1e-6) &&
	                Near_Internal(Rotated.Normal->Y, Sqrt(3.0) / 2, 1e-6));
	// せん断した格納軸（Y'=(ε,1,0)）の内部: +X面の外向き法線は(1,-ε,0)/√(1+ε²)、距離は0.1/√(1+ε²)。
	const f32 Epsilon = static_cast<f32>(5e-5);
	FOBB Sheared{{0, 0, 0}, {1, 1, 1}};
	Sheared.Axes = {FVector3{1, 0, 0}, FVector3{Epsilon, 1, 0}, FVector3{0, 0, 1}};
	const f64 Length = Sqrt(1 + f64(Epsilon) * f64(Epsilon));
	const auto Shear = FindShapeContact(FSphere{{0.9f, 0, 0}, 0.25f}, Sheared);
	PHYSICS_REQUIRE(Near_Internal(Shear.Separation, -(f64(1 - 0.9f) / Length + 0.25), 1e-9) && Shear.Normal &&
	                Near_Internal(Shear.Normal->X, 1 / Length, 1e-7) &&
	                Near_Internal(Shear.Normal->Y, -f64(Epsilon) / Length, 1e-7));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)FindShapeContact(FCircle2D{{0, 0}, -1}, Square);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    FOBB Broken = Sheared;
		    Broken.Axes[0] = {2, 0, 0};
		    (void)FindShapeContact(FSphere{{0, 0, 0}, 1}, Broken);
	    }));
}

// WorldのQueryContacts: 距離の上限、自己除外・カテゴリ、容量の超過、状態の拒否。開始時の接触を除くSweep。
template <typename T> void WorldQueries_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	const auto Floor = Floor_Internal<T>(World, Level);
	const auto Wall = T::Box(World, Level, T::At(6), 1, 50);
	const auto Self = T::Body(World);
	T::Ball(World, Self, T::At(0, 0.5f), 0.4f, CharacterCategory);
	// 床の上0.02の円: Margin 0.05なら床だけ（壁は4.5離れている）。
	const auto Near = World.QueryContacts({T::At(0, 0.52f), 0.5f}, 0.05, Self);
	PHYSICS_REQUIRE(Near.IsComplete() && Near.Count == 1 && Near.Items[0].Collider == Floor &&
	                Near_Internal(Near.Items[0].Separation, f64(0.52f) - 0.5, 1e-12) && Near.Items[0].Normal &&
	                T::Y(*Near.Items[0].Normal) == 1);
	PHYSICS_REQUIRE(World.QueryContacts({T::At(0, 0.52f), 0.5f}, 0.01, Self).Count == 0);
	const auto Both = World.QueryContacts({T::At(4.5f, 0.52f), 0.5f}, 0.05, Self);
	PHYSICS_REQUIRE(Both.Count == 2 && Both.Items[0].Collider == Floor && Both.Items[1].Collider == Wall &&
	                Both.Items[1].Separation == 0);
	// 自己除外しないと自分の円（重なり）も入る。カテゴリで除くこともできる。
	PHYSICS_REQUIRE(World.QueryContacts({T::At(0, 0.52f), 0.5f}, 0.05).Count == 2);
	PHYSICS_REQUIRE(World.QueryContacts({T::At(0, 0.52f), 0.5f}, 0.05, {}, Mask_Internal(ObstacleCategory)).Count == 1);
	// 容量（32件）を超える接触は、全件数で分かる。
	const auto Crowd = T::Body(World, T::At(0, 20));
	for (int32 Index = 0; Index < 40; ++Index)
	{
		T::Ball(World, Crowd, T::At(0), 0.25f, CharacterCategory);
	}
	const auto Many = World.QueryContacts({T::At(0, 20), 0.5f}, 0);
	PHYSICS_REQUIRE(Many.Count == 32 && Many.TotalFound == 40 && !Many.IsComplete() && !Many.Items[0].Normal);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.QueryContacts({T::At(0), 0.5f}, -1);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.QueryContacts({T::At(0), 0.5f}, TNumericLimits<f64>::QuietNaN());
	    }));
	// 開始時に床へ接している円を右へ動かす: SweepClosestは床の初期接触、除く方は壁に当たる。
	const auto Initial = World.SweepClosest({T::At(0, 0.5f), 0.5f}, T::At(10, 0.5f), Self);
	PHYSICS_REQUIRE(Initial && Initial->Collider == Floor && Initial->bInitialContact);
	const auto Skipped = World.SweepClosestIgnoringInitialContacts({T::At(0, 0.5f), 0.5f}, T::At(10, 0.5f), Self);
	PHYSICS_REQUIRE(Skipped && Skipped->Collider == Wall && !Skipped->bInitialContact &&
	                Near_Internal(Skipped->Fraction, 0.45, 1e-12));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.SweepClosestIgnoringInitialContacts({T::At(0), 0}, T::At(1));
	    }));
	// 途中で失敗したStepの後は拒否し、正常なStepで回復する。
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
		    (void)World.QueryContacts({T::At(0, 0.52f), 0.5f}, 0.05, {}, Mask_Internal(0u));
	    }));
	World.SetExecutionSettings({});
	World.Step(.01);
	PHYSICS_REQUIRE(World.QueryContacts({T::At(-30, 30), 0.5f}, 0.05).Count == 0);
}

// 初期重なりの解消: 浅い重なり、境界だけの接触、深すぎる、方向なし、挟み込み、補正先の別の障害物、自己除外。
template <typename T> void Recovery_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	Floor_Internal<T>(World, Level);
	typename T::FSettings Settings;
	// 床へ0.2めり込んだ円は、法線(0,1)方向へ接触余裕0.02の位置（y=0.52）まで押し出す。
	const auto Shallow = ResolveCharacterOverlap(World, T::At(0, 0.3f), Settings);
	PHYSICS_REQUIRE(Shallow.Status == ECharacterRecoveryStatus::Resolved && Shallow.Iterations == 1 &&
	                Near_Internal(T::Y(Shallow.Center), 0.52, Tolerance) && T::X(Shallow.Center) == 0 &&
	                Near_Internal(Shallow.Distance, 0.52 - f64(0.3f), Tolerance));
	const auto Touching = ResolveCharacterOverlap(World, T::At(0, 0.5f), Settings);
	PHYSICS_REQUIRE(Touching.Status == ECharacterRecoveryStatus::NoOverlap && Touching.Center == T::At(0, 0.5f));
	// 中心が床の内部で上面まで0.9: 解消に1.42必要で上限0.25を超える。中心は元のまま。
	const auto Deep = ResolveCharacterOverlap(World, T::At(0, -0.9f), Settings);
	PHYSICS_REQUIRE(Deep.Status == ECharacterRecoveryStatus::TooDeep && Deep.Center == T::At(0, -0.9f) &&
	                Deep.Distance == 0);
	// 同心の円: 方向を決められない。
	T::Ball(World, Level, T::At(20, 20), 1);
	const auto Same = ResolveCharacterOverlap(World, T::At(20, 20), Settings);
	PHYSICS_REQUIRE(Same.Status == ECharacterRecoveryStatus::Ambiguous && Same.Center == T::At(20, 20));
	// 左右から0.1ずつ挟まれる: 片方を押し出すともう片方へ深くめり込み、合計の上限を超える。
	T::Box(World, Level, T::At(-40.9f, 40), 0.5f, 5);
	T::Box(World, Level, T::At(-39.1f, 40), 0.5f, 5);
	const auto Pinched = ResolveCharacterOverlap(World, T::At(-40, 40), Settings);
	PHYSICS_REQUIRE(Pinched.Status != ECharacterRecoveryStatus::Resolved &&
	                Pinched.Status != ECharacterRecoveryStatus::NoOverlap && Pinched.Center == T::At(-40, 40));
	// 床から押し出す先（上面が1.02）に薄い板（y=0.85）がある: 補正の経路が塞がれるので解消しない。
	const auto Plate = T::Box(World, Level, T::At(-60, 0.85f), 3, 0);
	T::Box(World, Level, T::At(-60, -1), 5, 1);
	const auto Covered = ResolveCharacterOverlap(World, T::At(-60, 0.3f), Settings);
	PHYSICS_REQUIRE(Covered.Status == ECharacterRecoveryStatus::Blocked && Covered.Center == T::At(-60, 0.3f) &&
	                Covered.Collider && *Covered.Collider == Plate);
	// 自己Bodyとの重なりは、自己除外すれば扱わない。
	const auto Self = T::Body(World);
	T::Ball(World, Self, T::At(0, 5), 1, CharacterCategory);
	PHYSICS_REQUIRE(ResolveCharacterOverlap(World, T::At(0, 5), Settings).Status !=
	                ECharacterRecoveryStatus::NoOverlap);
	PHYSICS_REQUIRE(ResolveCharacterOverlap(World, T::At(0, 5), Settings, Self).Status ==
	                ECharacterRecoveryStatus::NoOverlap);
	PHYSICS_REQUIRE(ResolveCharacterOverlap(World, T::At(0, 5), Settings, {}, Mask_Internal(ObstacleCategory)).Status ==
	                ECharacterRecoveryStatus::NoOverlap);
	// 解消の反復0は解消しない（重なりがあれば上限として返す）。
	typename T::FSettings None = Settings;
	None.MaxRecoveryIterations = 0;
	PHYSICS_REQUIRE(ResolveCharacterOverlap(World, T::At(0, 0.3f), None).Status ==
	                ECharacterRecoveryStatus::IterationLimit);
}

// 反復滑り: 壁で停止、斜めで滑る、内側の角、途中の薄い板、狭い通路、上限、法線なし、丸め、容量。
template <typename T> void MoveAndSlide_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	typename T::FSettings Settings;
	const auto Empty = MoveAndSlide(World, T::At(0), T::At(10, 4), Settings);
	PHYSICS_REQUIRE(Empty.Stop == ECharacterMoveStop::Completed && Empty.EndCenter == T::At(10, 4) &&
	                Empty.ContactCount == 0 && Empty.Queries == 2);
	PHYSICS_REQUIRE(MoveAndSlide(World, T::At(0), T::At(0), Settings).Stop == ECharacterMoveStop::NoMovement);
	// 左面x=5の壁。正面なら中心x=4.5の接触から法線方向へ0.02戻した4.48で止まる。
	const auto Wall = T::Box(World, Level, T::At(6), 1, 50);
	const auto Head = MoveAndSlide(World, T::At(0), T::At(10), Settings);
	PHYSICS_REQUIRE(Head.Stop == ECharacterMoveStop::Blocked && Near_Internal(T::X(Head.EndCenter), 4.48, Tolerance) &&
	                T::Y(Head.EndCenter) == 0 && Head.ContactCount == 1 && Head.Contacts[0].Collider == Wall &&
	                T::X(Head.Contacts[0].Normal) == -1);
	// 斜め(10,4): 割合0.45の接触から4.48へ、残り(5.5,2.2)の内向き成分を除いてy=4まで滑る。
	const auto Slide = MoveAndSlide(World, T::At(0), T::At(10, 4), Settings);
	PHYSICS_REQUIRE(Slide.Stop == ECharacterMoveStop::Slid && Near_Internal(T::X(Slide.EndCenter), 4.48, Tolerance) &&
	                Near_Internal(T::Y(Slide.EndCenter), 4, Tolerance) &&
	                Near_Internal(T::X(Slide.Applied), 4.48, Tolerance));
	// 反復1回では壁の手前で止まり、残りを返す（到達成功ではない）。接触余裕を含めて中心x=4.48で当たるので、
	// 割合は0.448、残りは(10,4)*0.552。
	typename T::FSettings Once = Settings;
	Once.MaxIterations = 1;
	const auto Short = MoveAndSlide(World, T::At(0), T::At(10, 4), Once);
	PHYSICS_REQUIRE(Short.Stop == ECharacterMoveStop::IterationLimit &&
	                Near_Internal(T::X(Short.EndCenter), 4.48, Tolerance) &&
	                Near_Internal(T::X(Short.Remaining), 5.52, Tolerance) &&
	                Near_Internal(T::Y(Short.Remaining), 2.208, Tolerance));
	typename T::FSettings Budget = Settings;
	Budget.MaxQueries = 2;
	PHYSICS_REQUIRE(MoveAndSlide(World, T::At(0), T::At(10, 4), Budget).Stop == ECharacterMoveStop::QueryLimit);
	// 下面y=3の天井を加えた内側の角: 天井に沿った後、壁で止まる（4.48, 2.48）。
	const auto Ceiling = T::Box(World, Level, T::At(0, 4), 50, 1);
	const auto Corner = MoveAndSlide(World, T::At(0), T::At(10, 10), Settings);
	PHYSICS_REQUIRE(Corner.Stop == ECharacterMoveStop::Blocked &&
	                Near_Internal(T::X(Corner.EndCenter), 4.48, Tolerance) &&
	                Near_Internal(T::Y(Corner.EndCenter), 2.48, Tolerance) && Corner.ContactCount == 2 &&
	                Corner.Contacts[0].Collider == Ceiling && Corner.Contacts[1].Collider == Wall);
	// 途中に厚さ0の板（y=-3、x∈[1,4]）: 終点(2,-6)は空いているが、板の上で止まる。
	T::Box(World, Level, T::At(2.5f, -3), 1.5f, 0);
	const auto Thin = MoveAndSlide(World, T::At(2, 0), T::At(0, -6), Settings);
	PHYSICS_REQUIRE(Thin.Stop == ECharacterMoveStop::Blocked && Near_Internal(T::Y(Thin.EndCenter), -2.48, Tolerance));
	// 狭い通路（上下の面が中心から0.52）: 接触余裕以内の面を制約にして、平行な移動はそのまま通る。
	T::Box(World, Level, T::At(0, 21.02f), 50, 0.5f);
	T::Box(World, Level, T::At(0, 18.98f), 50, 0.5f);
	const auto Corridor = MoveAndSlide(World, T::At(-20, 20), T::At(-5, 0), Settings);
	PHYSICS_REQUIRE(Corridor.Stop == ECharacterMoveStop::Completed && Corridor.EndCenter == T::At(-25, 20));
	// 同じ場所の二つの壁（同じ割合）: 先のスロットの壁を制約にし、どちらも越えない。
	const auto TwinA = T::Box(World, Level, T::At(-44, -20), 1, 5);
	T::Box(World, Level, T::At(-44, -20), 1, 5);
	const auto Twin = MoveAndSlide(World, T::At(-50, -20), T::At(10), Settings);
	PHYSICS_REQUIRE(Twin.Stop == ECharacterMoveStop::Blocked && Twin.Contacts[0].Collider == TwinA &&
	                Near_Internal(T::X(Twin.EndCenter), -45.52, 1e-5));
	// 法線を得られない（半径1e-6で遠くから）: 接触の手前で止め、理由を返す。
	typename T::FSettings Tiny = Settings;
	Tiny.Radius = 1e-6f;
	Tiny.SkinWidth = 1e-3;
	T::Ball(World, Level, T::At(0, 100.000001f), 1e-6f);
	const auto Lost = MoveAndSlide(World, T::At(-1e7f, 100), T::At(2e7f), Tiny);
	PHYSICS_REQUIRE(Lost.Stop == ECharacterMoveStop::MissingNormal && T::X(Lost.EndCenter) < 0);
	// 2^20付近（間隔0.125）で接触余裕0.001: 丸めた候補がすべて接触側になり、進めない。
	const f32 Base = 1048576;
	T::Box(World, Level, T::At(Base + 6, 200), 1, 5);
	const auto Rounded = MoveAndSlide(World, T::At(Base, 200), T::At(10), Tiny);
	PHYSICS_REQUIRE(Rounded.Stop != ECharacterMoveStop::Completed && T::X(Rounded.EndCenter) <= Base + 4.5 + 1e-9);
	// 開始時の接触が容量を超える: 全件を制約にできないので動かない。
	const auto Crowd = T::Body(World, T::At(0, 300));
	for (int32 Index = 0; Index < 40; ++Index)
	{
		T::Ball(World, Crowd, T::At(0), 0.25f);
	}
	const auto Full = MoveAndSlide(World, T::At(0, 300.76f), T::At(1), Settings);
	PHYSICS_REQUIRE(Full.Stop == ECharacterMoveStop::ContactLimit && Full.EndCenter == T::At(0, 300.76f));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    typename T::FSettings Bad = Settings;
		    Bad.SkinWidth = 0;
		    (void)MoveAndSlide(World, T::At(0), T::At(1), Bad);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)MoveAndSlide(World, T::At(0), T::At(TNumericLimits<f32>::QuietNaN()), Settings);
	    }));
}

// 3Dの稜線と三つの面: 二つの壁の稜線に沿って上へ滑り、床を加えた三面では止まる。
void Crease3D_Internal()
{
	FPhysicsWorld3D World;
	const auto Level = F3D::Body(World);
	FColliderDescription3D WallX;
	WallX.Shape = FOBB{{6, 0, 0}, {1, 50, 50}};
	const auto First = World.AttachCollider(Level, WallX);
	FColliderDescription3D WallZ;
	WallZ.Shape = FOBB{{0, 0, 6}, {50, 50, 1}};
	const auto Second = World.AttachCollider(Level, WallZ);
	FCharacterMoveSettings3D Settings;
	const auto Along = MoveAndSlide(World, FVector3{0, 0, 0}, FVector3{10, 2, 10}, Settings);
	PHYSICS_REQUIRE(Along.Stop == ECharacterMoveStop::Slid && Near_Internal(Along.EndCenter.X, 4.48, 1e-5) &&
	                Near_Internal(Along.EndCenter.Z, 4.48, 1e-5) && Along.EndCenter.Y > 1.9 &&
	                Along.EndCenter.Y <= 2.0 && Along.ContactCount == 2 && Along.Contacts[0].Collider == First &&
	                Along.Contacts[1].Collider == Second);
	FColliderDescription3D Floor;
	Floor.Shape = FOBB{{0, -6, 0}, {50, 1, 50}};
	World.AttachCollider(Level, Floor);
	const auto Boxed = MoveAndSlide(World, FVector3{0, 0, 0}, FVector3{10, -10, 10}, Settings);
	PHYSICS_REQUIRE(Boxed.Stop == ECharacterMoveStop::Blocked && Near_Internal(Boxed.EndCenter.X, 4.48, 1e-5) &&
	                Near_Internal(Boxed.EndCenter.Y, -4.48, 1e-5) && Near_Internal(Boxed.EndCenter.Z, 4.48, 1e-5));
}

// 接地: 平らな床（距離0.02と境界の接触0）、隙間、30度（歩ける）と60度（急）、壁・天井、削除・カテゴリ変更の即時反映。
template <typename T> void Ground_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	const auto Floor = Floor_Internal<T>(World, Level);
	typename T::FSettings Settings;
	const auto Standing = ProbeCharacterGround(World, T::At(0, 0.52f), Settings);
	PHYSICS_REQUIRE(Standing.State == ECharacterGroundState::Walkable && Standing.Collider &&
	                *Standing.Collider == Floor && Standing.Normal && T::Y(*Standing.Normal) == 1 &&
	                Near_Internal(Standing.Separation, 0.02, Tolerance));
	PHYSICS_REQUIRE(ProbeCharacterGround(World, T::At(0, 0.5f), Settings).State == ECharacterGroundState::Walkable);
	// 隙間0.05は接触余裕の2倍（0.04）を超える。
	PHYSICS_REQUIRE(ProbeCharacterGround(World, T::At(0, 0.55f), Settings).State == ECharacterGroundState::Airborne);
	// 30度の斜面は歩ける（cos30 >= cos45）、60度は急。
	T::Box(World, Level, T::At(20, 0), 5, 1, SixthPi);
	T::Box(World, Level, T::At(40, 0), 5, 1, ThirdPi);
	const f64 Lift = 1 + 0.52;
	const auto Gentle = ProbeCharacterGround(
	    World, T::At(20 - static_cast<f32>(Lift * Sin(f64(SixthPi))), static_cast<f32>(Lift * Cos(f64(SixthPi)))),
	    Settings);
	PHYSICS_REQUIRE(Gentle.State == ECharacterGroundState::Walkable && Gentle.Normal &&
	                Near_Internal(T::X(*Gentle.Normal), -0.5, 1e-5));
	const auto Steep = ProbeCharacterGround(
	    World, T::At(40 - static_cast<f32>(Lift * Sin(f64(ThirdPi))), static_cast<f32>(Lift * Cos(f64(ThirdPi)))),
	    Settings);
	PHYSICS_REQUIRE(Steep.State == ECharacterGroundState::Steep);
	// 壁（法線が水平）と天井（下向き）は支持面ではない。
	T::Box(World, Level, T::At(-20, 0), 1, 50);
	PHYSICS_REQUIRE(ProbeCharacterGround(World, T::At(-21.52f, 30), Settings).State == ECharacterGroundState::Airborne);
	T::Box(World, Level, T::At(0, 61), 50, 1);
	PHYSICS_REQUIRE(ProbeCharacterGround(World, T::At(0, 59.48f), Settings).State == ECharacterGroundState::Airborne);
	// カテゴリ0・削除はStepなしで反映する。
	World.SetColliderQueryCategory(Floor, 0u);
	PHYSICS_REQUIRE(ProbeCharacterGround(World, T::At(0, 0.52f), Settings).State == ECharacterGroundState::Airborne);
	World.SetColliderQueryCategory(Floor, ObstacleCategory);
	PHYSICS_REQUIRE(World.DetachCollider(Floor));
	PHYSICS_REQUIRE(ProbeCharacterGround(World, T::At(0, 0.52f), Settings).State == ECharacterGroundState::Airborne);
}

// 固定更新:
// 静止、歩行（解析値）、壁、ジャンプの頂点と着地、天井、坂、急坂、段差（成功・高すぎ・天井・無効）、崖、吸い付き。
template <typename T> void Steps_Internal()
{
	typename T::FWorld World;
	const auto Level = T::Body(World);
	Floor_Internal<T>(World, Level);
	typename T::FSettings Settings;
	auto State = StateAt_Internal<T>(T::At(0, 0.52f));
	const auto Idle = Step_Internal<T>(World, Settings, State, Walk_Internal<T>(0));
	PHYSICS_REQUIRE(State.Center == T::At(0, 0.52f) && State.Ground.State == ECharacterGroundState::Walkable &&
	                !Idle.bLanded && !Idle.bLeftGround && State.Velocity == T::At(0));
	// 右へ60回: 速度はi*40/60で5まで上がる。x = (40/60*28 + 53*5)/60 = 4.7277…
	for (int32 Index = 0; Index < 60; ++Index)
	{
		Step_Internal<T>(World, Settings, State, Walk_Internal<T>(1));
		PHYSICS_REQUIRE(State.Ground.State == ECharacterGroundState::Walkable);
	}
	PHYSICS_REQUIRE(Near_Internal(T::X(State.Center), (40.0 / 60.0 * 28 + 53 * 5) / 60, 1e-4) &&
	                Near_Internal(T::Y(State.Center), 0.52, 1e-6) && Near_Internal(T::X(State.Velocity), 5, 1e-9));
	// ジャンプ: 初速6、重力20。37回で変位の和が0に戻り、頂点は0.52+57/60。
	State = StateAt_Internal<T>(T::At(0, 0.52f));
	const auto Jump = Step_Internal<T>(World, Settings, State, Walk_Internal<T>(0, true));
	PHYSICS_REQUIRE(Jump.bJumped && Jump.bLeftGround && State.Ground.State == ECharacterGroundState::Airborne &&
	                Near_Internal(T::Y(State.Center), 0.62, 1e-6));
	f64 Highest = T::Y(State.Center);
	int32 LandedAt = -1;
	for (int32 Index = 1; Index < 60 && LandedAt < 0; ++Index)
	{
		// 空中の押しっぱなしのジャンプ要求は、着地まで何もしない。
		const auto Air = Step_Internal<T>(World, Settings, State, Walk_Internal<T>(0, true));
		PHYSICS_REQUIRE(!Air.bJumped);
		Highest = Max(Highest, T::Y(State.Center));
		if (Air.bLanded)
		{
			LandedAt = Index;
		}
	}
	PHYSICS_REQUIRE(LandedAt >= 36 && LandedAt <= 38 && Near_Internal(Highest, 0.52 + 57.0 / 60.0, 1e-4) &&
	                Near_Internal(T::Y(State.Center), 0.52, 1e-4) &&
	                State.Ground.State == ECharacterGroundState::Walkable);
	// 天井（下面y=1.2）: 頭が当たって上向きの速度を失い、落ちて着地する。
	typename T::FWorld Low;
	const auto LowLevel = T::Body(Low);
	Floor_Internal<T>(Low, LowLevel);
	T::Box(Low, LowLevel, T::At(0, 2.2f), 50, 1);
	State = StateAt_Internal<T>(T::At(0, 0.52f));
	bool bCeiling = false;
	bool bLandedAgain = false;
	for (int32 Index = 0; Index < 60; ++Index)
	{
		const auto Step = Step_Internal<T>(Low, Settings, State, Walk_Internal<T>(0, Index == 0));
		bCeiling = bCeiling || Step.bHitCeiling;
		bLandedAgain = bLandedAgain || Step.bLanded;
		PHYSICS_REQUIRE(T::Y(State.Center) <= 1.2 - 0.5 + 1e-6);
	}
	PHYSICS_REQUIRE(bCeiling && bLandedAgain && State.Ground.State == ECharacterGroundState::Walkable);
}

// 坂・段差・崖。
template <typename T> void Terrain_Internal()
{
	typename T::FSettings Settings;
	// 30度の上り坂（x=0から）。坂の上でも歩ける床に立ち、yが上がる。
	{
		typename T::FWorld World;
		const auto Level = T::Body(World);
		Floor_Internal<T>(World, Level, -10, 10);
		const f64 Cosine = Cos(f64(SixthPi));
		const f64 Sine = Sin(f64(SixthPi));
		T::Box(World, Level, T::At(static_cast<f32>(10 * Cosine + Sine), static_cast<f32>(10 * Sine - Cosine)), 10, 1,
		       SixthPi);
		auto State = StateAt_Internal<T>(T::At(-2, 0.52f));
		for (int32 Index = 0; Index < 120; ++Index)
		{
			Step_Internal<T>(World, Settings, State, Walk_Internal<T>(1));
			PHYSICS_REQUIRE(State.Ground.State == ECharacterGroundState::Walkable);
		}
		PHYSICS_REQUIRE(T::X(State.Center) > 3 && T::Y(State.Center) > 0.52 + 0.5 * Sine * T::X(State.Center));
		// 下りも接地を保つ（吸い付き）。
		for (int32 Index = 0; Index < 90; ++Index)
		{
			Step_Internal<T>(World, Settings, State, Walk_Internal<T>(-1));
			PHYSICS_REQUIRE(State.Ground.State == ECharacterGroundState::Walkable);
		}
	}
	// 60度の急坂は上らない。
	{
		typename T::FWorld World;
		const auto Level = T::Body(World);
		Floor_Internal<T>(World, Level);
		const f64 Cosine = Cos(f64(ThirdPi));
		const f64 Sine = Sin(f64(ThirdPi));
		T::Box(World, Level, T::At(static_cast<f32>(2 + 10 * Cosine + Sine), static_cast<f32>(10 * Sine - Cosine)), 10,
		       1, ThirdPi);
		auto State = StateAt_Internal<T>(T::At(0, 0.52f));
		for (int32 Index = 0; Index < 120; ++Index)
		{
			Step_Internal<T>(World, Settings, State, Walk_Internal<T>(1));
			PHYSICS_REQUIRE(T::Y(State.Center) < 0.52 + 0.05);
		}
		PHYSICS_REQUIRE(T::X(State.Center) < 2);
	}
	// 段差: 高さ0.2（上れる）、0.5（高すぎ）、0.2に低い天井、段差上りの無効化。
	const auto StepRun = [&](f32 Height, bool bCeiling, f64 StepHeight, bool bExpectUp)
	{
		typename T::FWorld World;
		const auto Level = T::Body(World);
		Floor_Internal<T>(World, Level);
		T::Box(World, Level, T::At(22, Height / 2), 20, Height / 2);
		if (bCeiling)
		{
			T::Box(World, Level, T::At(0, 2.1f), 50, 1);
		}
		typename T::FSettings Local = Settings;
		Local.StepHeight = StepHeight;
		auto State = StateAt_Internal<T>(T::At(0, 0.52f));
		bool bStepped = false;
		for (int32 Index = 0; Index < 90; ++Index)
		{
			const auto Step = Step_Internal<T>(World, Local, State, Walk_Internal<T>(1));
			bStepped = bStepped || Step.bSteppedUp;
		}
		if (bExpectUp)
		{
			PHYSICS_REQUIRE(bStepped && T::X(State.Center) > 2.5 &&
			                Near_Internal(T::Y(State.Center), Height + 0.52, 1e-4) &&
			                State.Ground.State == ECharacterGroundState::Walkable);
		}
		else
		{
			// 段差が中心（0.52）より低ければ角に、高ければ面に止められる。どちらも表面から接触余裕0.02の距離（0.52）で止まり、
			// 角ではx = 2 - √(0.52² - (0.52-高さ)²)。
			const f64 Rise = f64(0.52f) - Height;
			const f64 BlockedX = Height >= 0.52f ? 1.48 : 2 - Sqrt(0.52 * 0.52 - Rise * Rise);
			PHYSICS_REQUIRE(!bStepped && Near_Internal(T::X(State.Center), BlockedX, 1e-4) &&
			                Near_Internal(T::Y(State.Center), 0.52, 1e-4));
		}
	};
	StepRun(0.2f, false, 0.3, true);
	StepRun(0.5f, false, 0.3, false);
	StepRun(0.2f, true, 0.3, false);
	StepRun(0.2f, false, 0, false);
	// 空中では段差を上らない: 段差の手前で跳んで押し続けても、着地前にbSteppedUpにならない。
	{
		typename T::FWorld World;
		const auto Level = T::Body(World);
		Floor_Internal<T>(World, Level);
		T::Box(World, Level, T::At(4, 2), 2, 2);
		auto State = StateAt_Internal<T>(T::At(1.4f, 0.52f));
		Step_Internal<T>(World, Settings, State, Walk_Internal<T>(0, true));
		for (int32 Index = 0; Index < 20; ++Index)
		{
			const auto Step = Step_Internal<T>(World, Settings, State, Walk_Internal<T>(1));
			PHYSICS_REQUIRE(!Step.bSteppedUp && T::X(State.Center) <= 1.48 + 1e-6);
		}
	}
	// 崖: 床はx<2。右へ歩くと接地を失い、落ちる。
	{
		typename T::FWorld World;
		const auto Level = T::Body(World);
		Floor_Internal<T>(World, Level, -8, 10);
		auto State = StateAt_Internal<T>(T::At(0, 0.52f));
		bool bLeft = false;
		for (int32 Index = 0; Index < 90; ++Index)
		{
			const auto Step = Step_Internal<T>(World, Settings, State, Walk_Internal<T>(1));
			bLeft = bLeft || Step.bLeftGround;
		}
		PHYSICS_REQUIRE(bLeft && State.Ground.State == ECharacterGroundState::Airborne && T::Y(State.Center) < 0);
	}
}

// 初期重なりからの復帰、深すぎる重なり、同じ入力列の再現、不正な設定。
template <typename T> void StepRecovery_Internal()
{
	typename T::FSettings Settings;
	typename T::FWorld World;
	const auto Level = T::Body(World);
	Floor_Internal<T>(World, Level);
	auto State = StateAt_Internal<T>(T::At(0, 0.3f));
	const auto Rescue = Step_Internal<T>(World, Settings, State, Walk_Internal<T>(0));
	PHYSICS_REQUIRE(Rescue.Recovery.Status == ECharacterRecoveryStatus::Resolved &&
	                Near_Internal(T::Y(State.Center), 0.52, 1e-6) &&
	                State.Ground.State == ECharacterGroundState::Walkable);
	auto Stuck = StateAt_Internal<T>(T::At(0, -0.9f));
	const auto Deep = Step_Internal<T>(World, Settings, Stuck, Walk_Internal<T>(1, true));
	PHYSICS_REQUIRE(Deep.Recovery.Status == ECharacterRecoveryStatus::TooDeep && Stuck.Center == T::At(0, -0.9f) &&
	                !Deep.bJumped);
	// 同じ入力列（歩き・ジャンプ・段差）なら、別のWorldでも同じ経過になる。
	const auto Run = [&]
	{
		typename T::FWorld Local;
		const auto LocalLevel = T::Body(Local);
		Floor_Internal<T>(Local, LocalLevel);
		T::Box(Local, LocalLevel, T::At(24, 0.1f), 20, 0.1f);
		auto Replay = StateAt_Internal<T>(T::At(0, 0.52f));
		for (int32 Index = 0; Index < 120; ++Index)
		{
			Step_Internal<T>(Local, Settings, Replay,
			                 Walk_Internal<T>(Index % 40 < 30 ? 1.0f : -0.5f, Index % 50 == 10));
		}
		return Replay;
	};
	const auto First = Run();
	const auto Second = Run();
	PHYSICS_REQUIRE(First.Center == Second.Center && First.Velocity == Second.Velocity &&
	                First.Ground.State == Second.Ground.State && T::X(First.Center) > 2);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    typename T::FSettings Bad = Settings;
		    Bad.MaxSlopeAngle = 2;
		    (void)StepCharacter(World, Bad, State, Walk_Internal<T>(0), StepSeconds);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)StepCharacter(World, Settings, State, Walk_Internal<T>(0), 0);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    typename T::FSettings Bad = Settings;
		    Bad.Up = T::At(0);
		    (void)StepCharacter(World, Bad, State, Walk_Internal<T>(0), StepSeconds);
	    }));
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"shape contacts analytic", &ShapeContacts_Internal},
    {"2D world contacts and initial-contact sweep", &WorldQueries_Internal<F2D>},
    {"3D world contacts and initial-contact sweep", &WorldQueries_Internal<F3D>},
    {"2D character overlap recovery", &Recovery_Internal<F2D>},
    {"3D character overlap recovery", &Recovery_Internal<F3D>},
    {"2D character move and slide", &MoveAndSlide_Internal<F2D>},
    {"3D character move and slide", &MoveAndSlide_Internal<F3D>},
    {"3D character crease and three planes", &Crease3D_Internal},
    {"2D character ground", &Ground_Internal<F2D>},
    {"3D character ground", &Ground_Internal<F3D>},
    {"2D character walk jump ceiling", &Steps_Internal<F2D>},
    {"3D character walk jump ceiling", &Steps_Internal<F3D>},
    {"2D character slopes steps cliff", &Terrain_Internal<F2D>},
    {"3D character slopes steps cliff", &Terrain_Internal<F3D>},
    {"2D character recovery determinism validation", &StepRecovery_Internal<F2D>},
    {"3D character recovery determinism validation", &StepRecovery_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetCharacterMovementCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
