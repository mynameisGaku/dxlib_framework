// SPDX-License-Identifier: NOASSERTION
// 線分問い合わせの対象フィルター（Colliderの問い合わせカテゴリと呼出し単位のマスク）。
// 同じ契約を実FPhysicsWorld2D／FPhysicsWorld3Dの両方で実行する。幾何計算そのものは既存回帰で確認済み。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/JobSystem.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// ゲーム側で定義する想定のカテゴリ。フレームワークは固定分類を持たない。
constexpr uint32 WallCategory = 1u << 0;
constexpr uint32 CharacterCategory = 1u << 1;
constexpr uint32 PickupCategory = 1u << 2;
constexpr uint32 HighCategory = 0x80000000u;

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
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type = EBodyType::Static)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = Position;
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
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type = EBodyType::Static)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = Position;
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

// 既定カテゴリの登録では、従来の2/3引数と明示全ビット・自己除外の呼出しが同じ結果になる。
template <typename T> void Compatibility_Internal()
{
	typename T::FWorld World;
	const auto Self = T::Body(World, T::At(0));
	const auto SelfCollider = T::Ball(World, Self, T::At(0), 1);
	const auto Other = T::Body(World, T::At(5));
	FColliderDescription2D Default2D;
	FColliderDescription3D Default3D;
	PHYSICS_REQUIRE(Default2D.QueryCategory == 1u && Default3D.QueryCategory == 1u);
	const auto OtherCollider = T::Ball(World, Other, T::At(0), 1);
	const FWorldQueryFilter All;
	PHYSICS_REQUIRE(All.IncludeCategories == 0xffffffffu);
	const auto Plain = World.RaycastClosest(T::At(-3), T::At(10));
	const auto Empty = World.RaycastClosest(T::At(-3), T::At(10), {});
	const auto Filtered = World.RaycastClosest(T::At(-3), T::At(10), {}, All);
	PHYSICS_REQUIRE(Plain && Empty && Filtered && Plain->Collider == SelfCollider && Empty->Collider == SelfCollider);
	PHYSICS_REQUIRE(Filtered->Collider == SelfCollider && Filtered->Fraction == Plain->Fraction &&
	                Filtered->Position == Plain->Position);
	const auto Excluded = World.RaycastClosest(T::At(-3), T::At(10), Self);
	const auto ExcludedFiltered = World.RaycastClosest(T::At(-3), T::At(10), Self, All);
	PHYSICS_REQUIRE(Excluded && ExcludedFiltered && Excluded->Collider == OtherCollider &&
	                ExcludedFiltered->Collider == OtherCollider);
	PHYSICS_REQUIRE(ExcludedFiltered->Fraction == Excluded->Fraction);
	PHYSICS_REQUIRE(World.GetColliderQueryCategory(SelfCollider) == 1u);
}

// 手前の不一致を飛ばして奥の一致を返す。壁とキャラクターを対象にすると手前の壁で止まる。
template <typename T> void Selection_Internal()
{
	typename T::FWorld World;
	const auto Scene = T::Body(World, T::At(0));
	// x=3の拾得物（手前）、x=5の壁、x=8のキャラクター（奥）。
	const auto Pickup = T::Ball(World, Scene, T::At(3), 0.5f, PickupCategory);
	const auto Wall = T::Box(World, Scene, T::At(5), 0.5f, 2, WallCategory);
	const auto Player = T::Ball(World, Scene, T::At(8), 0.5f, CharacterCategory);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10))->Collider == Pickup);
	const auto Blocked = World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(WallCategory | CharacterCategory));
	PHYSICS_REQUIRE(Blocked && Blocked->Collider == Wall && Abs(Blocked->Fraction - 0.45) < 1e-12);
	// 壁を越えた先では、一致する候補の中の最短を返す。
	PHYSICS_REQUIRE(
	    World.RaycastClosest(T::At(6), T::At(10), {}, Mask_Internal(WallCategory | CharacterCategory))->Collider ==
	    Player);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(WallCategory))->Collider == Wall);
	const auto Only = World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(CharacterCategory));
	PHYSICS_REQUIRE(Only && Only->Collider == Player && Abs(Only->Fraction - 0.75) < 1e-12);
}

// 1ビット・複数ビット所属・複数ビット検索・最上位ビット・マスク0・カテゴリ0・全ビット。
template <typename T> void Bits_Internal()
{
	typename T::FWorld World;
	const auto Scene = T::Body(World, T::At(0));
	const auto Hidden = T::Ball(World, Scene, T::At(2), 0.5f, 0u);
	const auto Multi = T::Ball(World, Scene, T::At(4), 0.5f, CharacterCategory | PickupCategory);
	const auto High = T::Ball(World, Scene, T::At(6), 0.5f, HighCategory);
	// カテゴリ0は全ビットのマスクや従来の入口でも対象にならない。
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10))->Collider == Multi);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(0xffffffffu))->Collider == Multi);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(CharacterCategory))->Collider == Multi);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(PickupCategory))->Collider == Multi);
	PHYSICS_REQUIRE(
	    World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(WallCategory | HighCategory))->Collider == High);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(HighCategory))->Collider == High);
	PHYSICS_REQUIRE(!World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(WallCategory)));
	PHYSICS_REQUIRE(!World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(0u)));
	PHYSICS_REQUIRE(World.GetColliderQueryCategory(Hidden) == 0u &&
	                World.GetColliderQueryCategory(High) == HighCategory);
}

// 同じBodyの複数Colliderは個別に判定し、自己Bodyの指定は一致するものもまとめて除外する。
template <typename T> void PerCollider_Internal()
{
	typename T::FWorld World;
	const auto Self = T::Body(World, T::At(0));
	const auto Near = T::Ball(World, Self, T::At(2), 0.5f, PickupCategory);
	const auto Far = T::Ball(World, Self, T::At(4), 0.5f, CharacterCategory);
	const auto Other = T::Body(World, T::At(7));
	const auto OtherCollider = T::Ball(World, Other, T::At(0), 0.5f, CharacterCategory);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(PickupCategory))->Collider == Near);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(CharacterCategory))->Collider == Far);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), Self, Mask_Internal(CharacterCategory))->Collider ==
	                OtherCollider);
	PHYSICS_REQUIRE(!World.RaycastClosest(T::At(0), T::At(10), Self, Mask_Internal(PickupCategory)));
}

// 変更はStepなしで次の問い合わせへ反映し、IDと世代・StepIndexを変えない。
template <typename T> void Change_Internal()
{
	typename T::FWorld World;
	const auto Scene = T::Body(World, T::At(0));
	const auto Wall = T::Box(World, Scene, T::At(3), 0.5f, 2, WallCategory);
	const auto Target = T::Ball(World, Scene, T::At(6), 0.5f, CharacterCategory);
	const FWorldQueryFilter Sight = Mask_Internal(WallCategory | CharacterCategory);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Sight)->Collider == Wall);
	// 壁を一時的に射線の対象から外す。
	World.SetColliderQueryCategory(Wall, 0u);
	PHYSICS_REQUIRE(World.GetColliderQueryCategory(Wall) == 0u);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Sight)->Collider == Target);
	PHYSICS_REQUIRE(World.IsColliderAlive(Wall) && World.CaptureSnapshot().StepIndex == 0);
	// 復元するとStepなしで対象へ戻る。IDは変わらない。
	World.SetColliderQueryCategory(Wall, WallCategory);
	const auto Restored = World.RaycastClosest(T::At(0), T::At(10), {}, Sight);
	PHYSICS_REQUIRE(Restored && Restored->Collider == Wall && World.CaptureSnapshot().StepIndex == 0);
	// 未設定のビットだけへ変えると、そのマスクでは対象外になる。
	World.SetColliderQueryCategory(Target, HighCategory);
	PHYSICS_REQUIRE(!World.RaycastClosest(T::At(4), T::At(10), {}, Sight));
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(4), T::At(10))->Collider == Target);
}

// 別World・明示無効・削除済み・旧世代のIDを拒否し、失敗時に既存値を変えない。再登録へ値を持ち越さない。
template <typename T> void Identity_Internal()
{
	typename T::FWorld World;
	const auto Body = T::Body(World, T::At(0));
	const auto Keep = T::Ball(World, Body, T::At(5), 0.5f, CharacterCategory);
	const auto Removed = T::Ball(World, Body, T::At(3), 0.5f, HighCategory);
	typename T::FWorld Other;
	const auto Foreign = T::Ball(Other, T::Body(Other, T::At(0)), T::At(0), 1);
	PHYSICS_REQUIRE(Foreign.Index == Keep.Index);
	const typename T::FColliderId Invalid{};
	PHYSICS_REQUIRE(World.DetachCollider(Removed));
	// 拒否すべき別World・明示無効・削除済みのID。
	const typename T::FColliderId Rejected[] = {Foreign, Invalid, Removed};
	for (const auto& Id : Rejected)
	{
		PHYSICS_REQUIRE(Throws_Internal(
		    [&]
		    {
			    World.SetColliderQueryCategory(Id, WallCategory);
		    }));
		PHYSICS_REQUIRE(Throws_Internal(
		    [&]
		    {
			    (void)World.GetColliderQueryCategory(Id);
		    }));
	}
	PHYSICS_REQUIRE(World.GetColliderQueryCategory(Keep) == CharacterCategory);
	PHYSICS_REQUIRE(Other.GetColliderQueryCategory(Foreign) == 1u);
	// 空いたスロットの新しい登録は、新しいDescriptionの値だけを持つ。
	const auto Reused = T::Ball(World, Body, T::At(3), 0.5f);
	PHYSICS_REQUIRE(Reused.Index == Removed.Index && Reused.Generation != Removed.Generation);
	PHYSICS_REQUIRE(World.GetColliderQueryCategory(Reused) == 1u);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(WallCategory))->Collider == Reused);
	PHYSICS_REQUIRE(!World.RaycastClosest(T::At(0), T::At(10), {}, Mask_Internal(HighCategory)));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.SetColliderQueryCategory(Removed, WallCategory);
	    }));
	// Body削除後の旧世代Body・Collider IDも拒否する。
	PHYSICS_REQUIRE(World.DestroyBody(Body));
	const auto NewBody = T::Body(World, T::At(0));
	PHYSICS_REQUIRE(NewBody.Index == Body.Index && NewBody.Generation != Body.Generation);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.GetColliderQueryCategory(Keep);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest(T::At(0), T::At(10), Body, Mask_Internal(0u));
	    }));
}

// 空World・マスク0・全Colliderがカテゴリ0でも、線分・除外ID・Step状態の検証を省略しない。
template <typename T> void ValidationFirst_Internal()
{
	typename T::FWorld World;
	const f32 Nan = TNumericLimits<f32>::QuietNaN();
	const FWorldQueryFilter None = Mask_Internal(0u);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest(T::At(1), T::At(1), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest(T::At(Nan), T::At(1), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest(T::At(-TNumericLimits<f32>::Max()), T::At(TNumericLimits<f32>::Max()), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest(T::At(0), T::At(1), typename T::FBodyId{}, None);
	    }));
	T::NoGravity(World);
	const auto Body = T::Body(World, T::At(0), EBodyType::Dynamic);
	const auto Hidden = T::Ball(World, Body, T::At(0), 1, 0u);
	PHYSICS_REQUIRE(!World.RaycastClosest(T::At(-2), T::At(2)));
	PHYSICS_REQUIRE(!World.RaycastClosest(T::At(-2), T::At(2), {}, None));
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Jobs;
	World.SetExecutionSettings(Broken);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.Step(.25, 1);
	    }));
	// 途中失敗したStepの後は、マスク0・カテゴリ0でも問い合わせと設定操作を拒否する。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest(T::At(-2), T::At(2), {}, None);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.SetColliderQueryCategory(Hidden, WallCategory);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.GetColliderQueryCategory(Hidden);
	    }));
	World.SetExecutionSettings({});
	World.Step(.5, 5);
	PHYSICS_REQUIRE(World.GetColliderQueryCategory(Hidden) == 0u);
	World.SetColliderQueryCategory(Hidden, WallCategory);
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(-2), T::At(2))->Collider == Hidden);
}

// 対象外の変換不能形状は計算しない。対象へ変えれば失敗し、先行する割合0でも隠さない。
template <typename T> void Uncomputable_Internal()
{
	typename T::FWorld World;
	const f32 Max = TNumericLimits<f32>::Max();
	const auto First = T::Body(World, T::At(0));
	const auto Leading = T::Ball(World, First, T::At(0), 1, WallCategory);
	const auto Huge = T::Body(World, T::At(Max));
	const auto Broken = T::Ball(World, Huge, T::At(Max), 1, PickupCategory);
	const auto Hit = World.RaycastClosest(T::At(0), T::At(1), {}, Mask_Internal(WallCategory));
	PHYSICS_REQUIRE(Hit && Hit->Collider == Leading && Hit->Fraction == 0);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest(T::At(0), T::At(1), {}, Mask_Internal(WallCategory | PickupCategory));
	    }));
	World.SetColliderQueryCategory(Broken, WallCategory);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.RaycastClosest(T::At(0), T::At(1), {}, Mask_Internal(WallCategory));
	    }));
	PHYSICS_REQUIRE(World.RaycastClosest(T::At(0), T::At(1), Huge, Mask_Internal(WallCategory))->Collider == Leading);
}

// 問い合わせ・カテゴリ設定の有無が違う2つのWorldへ同じ外力を与え、数値状態とStep数が一致する。
template <typename T> void ReadOnly_Internal()
{
	typename T::FWorld Queried;
	typename T::FWorld Control;
	T::NoGravity(Queried);
	T::NoGravity(Control);
	const auto A = T::Body(Queried, T::At(0), EBodyType::Dynamic);
	const auto B = T::Body(Control, T::At(0), EBodyType::Dynamic);
	const auto ColliderA = T::Box(Queried, A, T::At(0), 1, 0.5f, CharacterCategory);
	T::Box(Control, B, T::At(0), 1, 0.5f);
	T::Push(Queried, A);
	T::Push(Control, B);
	const typename T::FWorld& Read = Queried;
	for (int32 Index = 0; Index < 50; ++Index)
	{
		PHYSICS_REQUIRE(Read.RaycastClosest(T::At(-3), T::At(3), {}, Mask_Internal(CharacterCategory)));
		PHYSICS_REQUIRE(!Read.RaycastClosest(T::At(-3), T::At(3), A, Mask_Internal(CharacterCategory)));
	}
	Queried.SetColliderQueryCategory(ColliderA, 0u);
	Queried.SetColliderQueryCategory(ColliderA, PickupCategory);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		(void)Read.RaycastClosest(T::At(-20), T::At(20), {}, Mask_Internal(PickupCategory));
		Queried.Step(.01);
		Control.Step(.01);
		PHYSICS_REQUIRE(T::SameState(Queried, A, Control, B));
	}
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 20 && Control.CaptureSnapshot().StepIndex == 20);
}

// カテゴリ0でも接触・接地・休止は既定カテゴリと同じで、Snapshotにも残る。設定と問い合わせは休止を解かない。
template <typename T> void ContactIndependence_Internal()
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
		Colliders[Index] = T::Box(World, Bodies[Index], T::At(0), 0.5f, 0.5f, Index == 0 ? 0u : 1u);
		for (int32 StepIndex = 0; StepIndex < 600; ++StepIndex)
		{
			World.Step(1.0 / 120.0);
		}
	}
	PHYSICS_REQUIRE(T::SameState(Hidden, Bodies[0], Control, Bodies[1]));
	PHYSICS_REQUIRE(Hidden.IsSleeping(Bodies[0]));
	const auto Snapshot = Hidden.CaptureSnapshot();
	PHYSICS_REQUIRE(Snapshot.Colliders.Size() == 2);
	PHYSICS_REQUIRE(!Hidden.RaycastClosest(T::At(0, 3), T::At(0, -3)));
	const auto Position = Hidden.GetPosition(Bodies[0]);
	Hidden.SetColliderQueryCategory(Colliders[0], WallCategory);
	PHYSICS_REQUIRE(Hidden.RaycastClosest(T::At(0, 3), T::At(0, -3))->Collider == Colliders[0]);
	PHYSICS_REQUIRE(Hidden.IsSleeping(Bodies[0]) && Hidden.GetPosition(Bodies[0]) == Position);
	PHYSICS_REQUIRE(Hidden.CaptureSnapshot().StepIndex == 600);
}

// 2D／3Dの各ケースを同じ名前の規則で並べる。
const PhysicsTest::FCase Cases_Internal[] = {
    {"2D query filter keeps legacy calls", &Compatibility_Internal<F2D>},
    {"3D query filter keeps legacy calls", &Compatibility_Internal<F3D>},
    {"2D query filter selects before closest", &Selection_Internal<F2D>},
    {"3D query filter selects before closest", &Selection_Internal<F3D>},
    {"2D query filter bit sets", &Bits_Internal<F2D>},
    {"3D query filter bit sets", &Bits_Internal<F3D>},
    {"2D query filter per collider and self exclusion", &PerCollider_Internal<F2D>},
    {"3D query filter per collider and self exclusion", &PerCollider_Internal<F3D>},
    {"2D query category changes apply immediately", &Change_Internal<F2D>},
    {"3D query category changes apply immediately", &Change_Internal<F3D>},
    {"2D query category ids and slot reuse", &Identity_Internal<F2D>},
    {"3D query category ids and slot reuse", &Identity_Internal<F3D>},
    {"2D query filter validates before empty results", &ValidationFirst_Internal<F2D>},
    {"3D query filter validates before empty results", &ValidationFirst_Internal<F3D>},
    {"2D query filter skips excluded uncomputable shapes", &Uncomputable_Internal<F2D>},
    {"3D query filter skips excluded uncomputable shapes", &Uncomputable_Internal<F3D>},
    {"2D query filter preserves dynamics", &ReadOnly_Internal<F2D>},
    {"3D query filter preserves dynamics", &ReadOnly_Internal<F3D>},
    {"2D query category leaves contacts and snapshots", &ContactIndependence_Internal<F2D>},
    {"3D query category leaves contacts and snapshots", &ContactIndependence_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetWorldQueryFilterCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
