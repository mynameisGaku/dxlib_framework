// SPDX-License-Identifier: NOASSERTION
// 距離拘束の所有と寿命（J1）。2D／3Dで対になる。
// 確認:
// 生成と破棄、IsAlive、別WorldのID、世代違い、同じBodyの拒否、Dynamicを含まない組の拒否、
// LengthとAnchorの検査、Bodyを破棄するとJointも失効する、JointスロットとBodyスロットの再利用、
// 古いJoint IDで新しいJointへつながらない、Colliderを外してもJointは残る、Joint破棄後もBodyは生きる。
// Distance Jointの拘束（距離の維持・回転・Warm Start・Island・並列）はJ2以降で確かめる。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// Toolboxの例外が投げられたかを見る。
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

// 平面側のWorld操作。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FJointId = FJointId2D;
	using FVector = FVector2;
	using FDescription = FBodyDescription2D;
	using FJoint = FDistanceJointDescription2D;
	using FState = FDistanceJointState2D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y};
	}
	static FBodyId Dynamic(FWorld& World, FVector Position)
	{
		FDescription Description;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FBodyId Static(FWorld& World, FVector Position)
	{
		FDescription Description;
		Description.Type = EBodyType::Static;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FBodyId Kinematic(FWorld& World, FVector Position)
	{
		FDescription Description;
		Description.Type = EBodyType::Kinematic;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static void AttachBox(FWorld& World, FBodyId Body, f32 Radius)
	{
		FColliderDescription2D Collider;
		Collider.Shape = FCircle2D{{0, 0}, Radius};
		World.AttachCollider(Body, Collider);
	}
	// 姿勢を変えず、角度だけ回す（Anchorの追従を確かめるため）。
	static void Turn(FWorld& World, FBodyId Body, f32 Angle)
	{
		World.SetBodyTransform(Body, World.GetPosition(Body), Angle);
	}
};

// 空間側のWorld操作。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FJointId = FJointId3D;
	using FVector = FVector3;
	using FDescription = FBodyDescription3D;
	using FJoint = FDistanceJointDescription3D;
	using FState = FDistanceJointState3D;
	static FVector At(f32 X, f32 Y, f32 Z = 0)
	{
		return {X, Y, Z};
	}
	static FBodyId Dynamic(FWorld& World, FVector Position)
	{
		FDescription Description;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FBodyId Static(FWorld& World, FVector Position)
	{
		FDescription Description;
		Description.Type = EBodyType::Static;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FBodyId Kinematic(FWorld& World, FVector Position)
	{
		FDescription Description;
		Description.Type = EBodyType::Kinematic;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static void AttachBox(FWorld& World, FBodyId Body, f32 Radius)
	{
		FColliderDescription3D Collider;
		Collider.Shape = FSphere{{0, 0, 0}, Radius};
		World.AttachCollider(Body, Collider);
	}
	// 姿勢を変えず、Z軸まわりだけ回す（Anchorの追従を確かめるため）。
	static void Turn(FWorld& World, FBodyId Body, f32 Angle)
	{
		const f64 Half = f64(Angle) * 0.5;
		World.SetBodyTransform(Body, World.GetPosition(Body),
		                       FQuaternion{0, 0, static_cast<f32>(Sin(Half)), static_cast<f32>(Cos(Half))});
	}
};

// 生成と破棄、IsAlive、状態の読み取り。
template <typename T> void CreateAndDestroy_Internal()
{
	typename T::FWorld World;
	const auto Anchor = T::Static(World, T::At(0, 0));
	const auto Weight = T::Dynamic(World, T::At(0, -2));
	typename T::FJoint Joint;
	Joint.Length = 2;
	const auto Id = World.CreateDistanceJoint(Anchor, Weight, Joint);
	PHYSICS_REQUIRE(World.IsJointAlive(Id));
	// 生成直後のAnchor間距離はLengthと一致する。
	const typename T::FState State = World.GetDistanceJoint(Id);
	PHYSICS_REQUIRE(Abs(State.CurrentLength - 2.0) < 1e-5 && Abs(State.Error) < 1e-5);
	PHYSICS_REQUIRE(World.DestroyJoint(Id));
	PHYSICS_REQUIRE(!World.IsJointAlive(Id));
	// 二度目の破棄はfalse。
	PHYSICS_REQUIRE(!World.DestroyJoint(Id));
	// 破棄後もBodyは生きる。
	PHYSICS_REQUIRE(World.IsAlive(Anchor) && World.IsAlive(Weight));
}

// 別WorldのIDと、期限切れの世代は見つからない。
template <typename T> void ForeignAndStale_Internal()
{
	typename T::FWorld World;
	typename T::FWorld Other;
	const auto Anchor = T::Static(World, T::At(0, 0));
	const auto Weight = T::Dynamic(World, T::At(0, -2));
	const auto OtherAnchor = T::Static(Other, T::At(0, 0));
	const auto OtherWeight = T::Dynamic(Other, T::At(0, -2));
	typename T::FJoint Joint;
	Joint.Length = 2;
	const auto Id = World.CreateDistanceJoint(Anchor, Weight, Joint);
	const auto Foreign = Other.CreateDistanceJoint(OtherAnchor, OtherWeight, Joint);
	// 別WorldのBodyは見つからない。
	PHYSICS_REQUIRE(!World.IsJointAlive(Foreign) && !Other.IsJointAlive(Id));
	// 世代をひとつ進めただけのIDは見つからない。
	typename T::FJointId Stale = Id;
	Stale.Generation += 1;
	PHYSICS_REQUIRE(!World.IsJointAlive(Stale));
	// スロット番号が範囲外のIDも見つからない。
	typename T::FJointId Outside = Id;
	Outside.Index = 4096;
	PHYSICS_REQUIRE(!World.IsJointAlive(Outside));
	PHYSICS_REQUIRE(!World.DestroyJoint(Foreign) && !World.DestroyJoint(Stale));
}

// 生成条件の検証。同じBody、Dynamicを含まない組、不正な値を拒否する。
template <typename T> void Reject_Internal()
{
	typename T::FWorld World;
	const auto StaticA = T::Static(World, T::At(0, 0));
	const auto StaticB = T::Static(World, T::At(1, 0));
	const auto Kinematic = T::Kinematic(World, T::At(2, 0));
	const auto Dynamic = T::Dynamic(World, T::At(0, -2));
	typename T::FJoint Joint;
	Joint.Length = 1;
	// 同じBody同士は拒否する。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.CreateDistanceJoint(Dynamic, Dynamic, Joint);
	    }));
	// 両方とも動かせない組は拒否する。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.CreateDistanceJoint(StaticA, StaticB, Joint);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.CreateDistanceJoint(StaticA, Kinematic, Joint);
	    }));
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.CreateDistanceJoint(Kinematic, StaticA, Joint);
	    }));
	// 存在しないBodyのIDは拒否する。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.CreateDistanceJoint(Dynamic, typename T::FBodyId{}, Joint);
	    }));
	// Lengthが負、または非有限は拒否する。
	typename T::FJoint Negative = Joint;
	Negative.Length = -1;
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.CreateDistanceJoint(StaticA, Dynamic, Negative);
	    }));
	typename T::FJoint Infinite = Joint;
	Infinite.Length = NAN;
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.CreateDistanceJoint(StaticA, Dynamic, Infinite);
	    }));
	// 拒否ではJointが作られない。
	typename T::FJointId Probe = World.CreateDistanceJoint(StaticA, Dynamic, Joint);
	PHYSICS_REQUIRE(World.IsJointAlive(Probe));
}

// Bodyを破棄すると、そのBodyに接続されるJointは失効する。
template <typename T> void BodyDestroyCascades_Internal()
{
	typename T::FWorld World;
	const auto Anchor = T::Static(World, T::At(0, 0));
	const auto Weight = T::Dynamic(World, T::At(0, -2));
	typename T::FJoint Joint;
	Joint.Length = 2;
	// A側を破棄する。
	const auto OnA = World.CreateDistanceJoint(Weight, Anchor, Joint);
	PHYSICS_REQUIRE(World.DestroyBody(Weight));
	PHYSICS_REQUIRE(!World.IsJointAlive(OnA));
	// 両方を順に破棄しても残るJointはない。
	const auto Second = T::Dynamic(World, T::At(3, -2));
	const auto OnSecond = World.CreateDistanceJoint(Anchor, Second, Joint);
	PHYSICS_REQUIRE(World.DestroyBody(Anchor));
	PHYSICS_REQUIRE(!World.IsJointAlive(OnSecond));
	PHYSICS_REQUIRE(World.DestroyBody(Second));
	// ほかのBodyは生きている。
	const auto Free = T::Dynamic(World, T::At(5, 0));
	PHYSICS_REQUIRE(World.IsAlive(Free));
}

// BodyスロットとJointスロットの再利用で、古いIDが新しい登録へつながらない。
template <typename T> void SlotReuse_Internal()
{
	typename T::FWorld World;
	const auto Anchor = T::Static(World, T::At(0, 0));
	const auto Weight = T::Dynamic(World, T::At(0, -2));
	typename T::FJoint Joint;
	Joint.Length = 2;
	const auto First = World.CreateDistanceJoint(Anchor, Weight, Joint);
	PHYSICS_REQUIRE(World.DestroyJoint(First));
	// 同じスロットが再利用され、世代が進む。
	const auto Second = World.CreateDistanceJoint(Anchor, Weight, Joint);
	PHYSICS_REQUIRE(Second.Index == First.Index && Second.Generation != First.Generation);
	PHYSICS_REQUIRE(World.IsJointAlive(Second) && !World.IsJointAlive(First));
	// Bodyも同じように再利用される。
	PHYSICS_REQUIRE(World.DestroyBody(Weight));
	const auto Reborn = T::Dynamic(World, T::At(0, -4));
	PHYSICS_REQUIRE(Reborn.Index == Weight.Index && Reborn.Generation != Weight.Generation);
	PHYSICS_REQUIRE(World.IsAlive(Reborn) && !World.IsAlive(Weight));
	// 失効したJointのBodyが残っているため、古いIDでは状態を引けない。
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    (void)World.GetDistanceJoint(First);
	    }));
	// 新しい登録は取得できる。
	typename T::FJoint Longer = Joint;
	Longer.Length = 4;
	const auto Third = World.CreateDistanceJoint(Anchor, Reborn, Longer);
	PHYSICS_REQUIRE(Abs(World.GetDistanceJoint(Third).CurrentLength - 4.0) < 1e-5);
}

// Distance JointはBodyの拘束であり、Colliderとは独立である。
template <typename T> void IndependentOfCollider_Internal()
{
	typename T::FWorld World;
	const auto Anchor = T::Static(World, T::At(0, 0));
	const auto Weight = T::Dynamic(World, T::At(0, -2));
	T::AttachBox(World, Weight, 0.5f);
	typename T::FJoint Joint;
	Joint.Length = 2;
	const auto Id = World.CreateDistanceJoint(Anchor, Weight, Joint);
	const auto Snapshot = World.CaptureSnapshot();
	PHYSICS_REQUIRE(Snapshot.Colliders.Size() == 1);
	// Colliderを外してもJointは残る。
	PHYSICS_REQUIRE(World.DetachCollider(Snapshot.Colliders[0].Id));
	PHYSICS_REQUIRE(World.IsJointAlive(Id));
	// Colliderが0個でもJointは有効で、距離は読める。
	PHYSICS_REQUIRE(World.CaptureSnapshot().Colliders.IsEmpty());
	PHYSICS_REQUIRE(Abs(World.GetDistanceJoint(Id).CurrentLength - 2.0) < 1e-5);
	// BodyのColliderが残らなくてもJointは張り付く。
	PHYSICS_REQUIRE(World.DestroyJoint(Id));
	PHYSICS_REQUIRE(World.IsAlive(Weight));
}

// Local AnchorはBodyの回転に従う（読み取りの段階で座標が変換される）。
template <typename T> void LocalAnchorFollowsRotation_Internal()
{
	typename T::FWorld World;
	const auto Anchor = T::Static(World, T::At(0, 0));
	const auto Weight = T::Dynamic(World, T::At(0, -2));
	typename T::FJoint Joint;
	// LengthはJ2で拘束として解くので、段階としては現在距離だけを見る。、報告される現在距離だけを見る。
	Joint.Length = 2;
	Joint.LocalAnchorA = T::At(1, 0);
	Joint.LocalAnchorB = T::At(1, 0);
	const auto Id = World.CreateDistanceJoint(Anchor, Weight, Joint);
	// 両Anchorとも+1なので、重心間距離2と同じ。
	PHYSICS_REQUIRE(Abs(World.GetDistanceJoint(Id).CurrentLength - 2.0) < 1e-4);
	// Weightを90度回すとB側のAnchorが(1,-2)から(0,-1)へ移り、Anchor間距離はsqrt(2)になる。
	// 回転を追従していない実装なら2のままになる。
	T::Turn(World, Weight, 1.5707963f);
	PHYSICS_REQUIRE(Abs(World.GetDistanceJoint(Id).CurrentLength - 1.4142136) < 1e-3);
}

void Create2D_Internal()
{
	CreateAndDestroy_Internal<F2D>();
}
void Create3D_Internal()
{
	CreateAndDestroy_Internal<F3D>();
}
void Foreign2D_Internal()
{
	ForeignAndStale_Internal<F2D>();
}
void Foreign3D_Internal()
{
	ForeignAndStale_Internal<F3D>();
}
void Reject2D_Internal()
{
	Reject_Internal<F2D>();
}
void Reject3D_Internal()
{
	Reject_Internal<F3D>();
}
void Cascade2D_Internal()
{
	BodyDestroyCascades_Internal<F2D>();
}
void Cascade3D_Internal()
{
	BodyDestroyCascades_Internal<F3D>();
}
void Reuse2D_Internal()
{
	SlotReuse_Internal<F2D>();
}
void Reuse3D_Internal()
{
	SlotReuse_Internal<F3D>();
}
void Collider2D_Internal()
{
	IndependentOfCollider_Internal<F2D>();
}
void Collider3D_Internal()
{
	IndependentOfCollider_Internal<F3D>();
}
void Anchor2D_Internal()
{
	LocalAnchorFollowsRotation_Internal<F2D>();
}
void Anchor3D_Internal()
{
	LocalAnchorFollowsRotation_Internal<F3D>();
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D distance joint creates and destroys", &Create2D_Internal},
    {"3D distance joint creates and destroys", &Create3D_Internal},
    {"2D distance joint rejects foreign and stale ids", &Foreign2D_Internal},
    {"3D distance joint rejects foreign and stale ids", &Foreign3D_Internal},
    {"2D distance joint rejects invalid bodies and lengths", &Reject2D_Internal},
    {"3D distance joint rejects invalid bodies and lengths", &Reject3D_Internal},
    {"2D distance joint dies with its body", &Cascade2D_Internal},
    {"3D distance joint dies with its body", &Cascade3D_Internal},
    {"2D distance joint separates reused body and joint slots", &Reuse2D_Internal},
    {"3D distance joint separates reused body and joint slots", &Reuse3D_Internal},
    {"2D distance joint outlives collider detach", &Collider2D_Internal},
    {"3D distance joint outlives collider detach", &Collider3D_Internal},
    {"2D distance joint local anchor follows body rotation", &Anchor2D_Internal},
    {"3D distance joint local anchor follows body rotation", &Anchor3D_Internal}};
} // namespace

const PhysicsTest::FCase* PhysicsTest::GetJointLifetimeCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
