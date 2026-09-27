// SPDX-License-Identifier: NOASSERTION
// Kinematicの物体の運動（R4）を、実DPhysicsScene2D／3D・固定更新で確かめる。
// 確認:
// 経路の解析値（位置＝速さ×時刻、回転＝角速度×時刻）、物理Stepでの一度だけの積分、登録順に依存しない物理Stepの直前の姿勢と速度、
// 速度・角速度の上限、Teleport、一時停止、Stepの前の点の予測（PredictBodyPoint）とStep後の実際の位置の一致。
#include "Support/Test.h"
#include "Support/FakeBackend.h"
#include "Dxf/AssetService.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/KinematicMoverComponent2D.h"
#include "Dxf/KinematicMoverComponent3D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/SceneNavigator.h"
using namespace Dxf;
using namespace Dxf::Testing;
using namespace Toolbox;
namespace
{
constexpr f64 StepSeconds = 1.0 / 60.0;

struct FCase2D
{
	using FScene = DPhysicsScene2D;
	using FVector = Toolbox::FVector2;
	using FMover = DKinematicMover2DComponent;
	using FDescription = FKinematicMoverDescription2D;
	using FPose = FKinematicPose2D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y};
	}
	static f32 X(FVector Value)
	{
		return Value.X;
	}
	static FColliderDescription2D Plate()
	{
		FColliderDescription2D Description;
		Description.Shape = Toolbox::FOrientedBox2D{{0, 0}, {2, 0.25f}, 0};
		return Description;
	}
	// 回転の角度（2Dはそのまま、比較用）。
	static f64 Turn(f32 Rotation)
	{
		return Rotation;
	}
	static FPose Spin(f64 Angle)
	{
		FPose Pose;
		Pose.Rotation = static_cast<f32>(Angle);
		return Pose;
	}
	static FVector PointAfter(const FPhysicsWorld2D& World, FBodyId2D Body, FVector Local)
	{
		const f64 Angle = World.GetAngle(Body);
		const FVector Center = World.GetPosition(Body);
		return {static_cast<f32>(Center.X + Cos(Angle) * Local.X - Sin(Angle) * Local.Y),
		        static_cast<f32>(Center.Y + Sin(Angle) * Local.X + Cos(Angle) * Local.Y)};
	}
	static f64 Distance(FVector A, FVector B)
	{
		const f64 DX = f64(A.X) - B.X;
		const f64 DY = f64(A.Y) - B.Y;
		return Sqrt(DX * DX + DY * DY);
	}
};
struct FCase3D
{
	using FScene = DPhysicsScene3D;
	using FVector = Toolbox::FVector3;
	using FMover = DKinematicMover3DComponent;
	using FDescription = FKinematicMoverDescription3D;
	using FPose = FKinematicPose3D;
	static FVector At(f32 X, f32 Y = 0)
	{
		return {X, Y, 0};
	}
	static f32 X(FVector Value)
	{
		return Value.X;
	}
	static FColliderDescription3D Plate()
	{
		FColliderDescription3D Description;
		Description.Shape = Toolbox::FOBB{{0, 0, 0}, {2, 0.25f, 2}};
		return Description;
	}
	// Y軸回りの回転の角度（四元数から）。
	static f64 Turn(const Toolbox::FQuaternion& Rotation)
	{
		const f64 Sign = Rotation.W < 0 ? -1.0 : 1.0;
		return 2.0 * Atan2(Sign * Rotation.Y, Sign * Rotation.W);
	}
	static FPose Spin(f64 Angle)
	{
		FPose Pose;
		Pose.Rotation = Toolbox::FQuaternion::FromAxisAngle({0, 1, 0}, static_cast<f32>(Angle));
		return Pose;
	}
	static FVector PointAfter(const FPhysicsWorld3D& World, FBodyId3D Body, FVector Local)
	{
		return World.GetPosition(Body) + World.GetOrientation(Body).Rotate(Local);
	}
	static f64 Distance(FVector A, FVector B)
	{
		const f64 DX = f64(A.X) - B.X;
		const f64 DY = f64(A.Y) - B.Y;
		const f64 DZ = f64(A.Z) - B.Z;
		return Sqrt(DX * DX + DY * DY + DZ * DZ);
	}
};

struct FFixture
{
	FFakeBackend Backend;
	FAssetService Assets{Backend, Backend, Backend};
	FAudioPlayer Audio{Backend};
	FSceneNavigator Navigator{Assets, Audio};
	FInputStateTracker Tracker;
	FInitContext Init()
	{
		return {Assets};
	}
	FTickContext Tick(f64 DeltaSeconds, bool bPaused = false)
	{
		FFrameTime Time;
		Time.DeltaSeconds = DeltaSeconds;
		Time.UnscaledDeltaSeconds = DeltaSeconds;
		Time.bPaused = bPaused;
		return {Tracker.GetSnapshot(), Time};
	}
};

// Kinematicの物体のGameObject。
template <typename T> class TMoverObject : public DGameObject
{
public:
	explicit TMoverObject(typename T::FDescription Description) : m_Description(Toolbox::Move(Description))
	{
	}
	typename T::FMover& Mover()
	{
		auto Found = FindComponent<typename T::FMover>();
		REQUIRE(Found.Get() != nullptr);
		return *Found.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		auto Added = AddComponent<typename T::FMover>(m_Description);
		if (!Added)
		{
			return TResult<void>::Failure(Added.Error());
		}
		return {};
	}

private:
	typename T::FDescription m_Description;
};

// 物理Stepの直前（全ての固定更新の後）に、床の位置と速度を記録する観測者。
template <typename T> class TProbe : public DGameObject, private IPrePhysicsStep
{
public:
	TMoverObject<T>* Target = nullptr;
	Toolbox::TVector<typename T::FVector> Positions;
	Toolbox::TVector<typename T::FVector> Velocities;

protected:
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		Context.PrePhysicsStep->Enqueue(*this);
	}

private:
	void OnPrePhysicsStep_Internal(const FFixedTickContext& Context) override
	{
		const auto Body = Target->Mover().GetBodyId();
		REQUIRE(Body);
		if constexpr (sizeof(typename T::FVector) == sizeof(Toolbox::FVector2))
		{
			Positions.PushBack(Context.Physics2D->GetPosition(*Body));
			Velocities.PushBack(Context.Physics2D->GetVelocity(*Body));
		}
		else
		{
			Positions.PushBack(Context.Physics3D->GetPosition(*Body));
			Velocities.PushBack(Context.Physics3D->GetVelocity(*Body));
		}
	}
};

template <typename T> typename T::FDescription Description_Internal()
{
	typename T::FDescription Description;
	Description.Colliders.PushBack(T::Plate());
	return Description;
}

// 一定の速さの経路。固定更新kの物理Stepの後の位置は経路(k×dt)で、物理Stepで一度だけ動く（二重の移動なし）。
template <typename T> void FollowsPathOnce_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto Object = Scene.template Spawn<TMoverObject<T>>(Description_Internal<T>());
	REQUIRE(Object);
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	auto& Mover = Object.Value().Get()->Mover();
	Mover.SetPath(
	    [](f64 Seconds)
	    {
		    typename T::FPose Pose;
		    Pose.Position = T::At(static_cast<f32>(2 * Seconds));
		    return Pose;
	    });
	for (int32 Frame = 1; Frame <= 90; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds)));
		REQUIRE(Abs(T::X(Mover.GetPose().Position) - 2 * StepSeconds * Frame) < 1e-4);
		REQUIRE(!Mover.WasLimited());
	}
	// 1描画フレームに固定更新3回でも経路の時刻は3回分だけ進む。
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(3 * StepSeconds)));
	REQUIRE(Abs(T::X(Mover.GetPose().Position) - 2 * StepSeconds * 93) < 1e-4);
	REQUIRE(Abs(Mover.GetElapsedSeconds() - 93 * StepSeconds) < 1e-9);
	// 一時停止中は固定更新がなく、経路の時刻も位置も進まない。
	const auto Paused = Mover.GetPose().Position;
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds, true)));
	REQUIRE(Mover.GetPose().Position == Paused && Abs(Mover.GetElapsedSeconds() - 93 * StepSeconds) < 1e-9);
	Scene.Shutdown_Internal();
}

// 観測者（物理Stepの直前）が見る床の位置と速度は、床と観測者の生成順に依存しない。
template <typename T> void OrderIndependent_Internal()
{
	Toolbox::TVector<typename T::FVector> Records[2];
	for (int32 Order = 0; Order < 2; ++Order)
	{
		FFixture Fixture;
		typename T::FScene Scene;
		TProbe<T>* Probe = nullptr;
		TMoverObject<T>* Mover = nullptr;
		if (Order == 0)
		{
			Probe = Scene.template Spawn<TProbe<T>>().Value().Get();
			Mover = Scene.template Spawn<TMoverObject<T>>(Description_Internal<T>()).Value().Get();
		}
		else
		{
			Mover = Scene.template Spawn<TMoverObject<T>>(Description_Internal<T>()).Value().Get();
			Probe = Scene.template Spawn<TProbe<T>>().Value().Get();
		}
		Probe->Target = Mover;
		REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
		Mover->Mover().SetPath(
		    [](f64 Seconds)
		    {
			    typename T::FPose Pose;
			    Pose.Position = T::At(static_cast<f32>(Seconds * Seconds), static_cast<f32>(Seconds));
			    return Pose;
		    });
		for (int32 Frame = 0; Frame < 30; ++Frame)
		{
			REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds)));
		}
		for (Toolbox::size_t Index = 0; Index < Probe->Positions.Size(); ++Index)
		{
			Records[Order].PushBack(Probe->Positions[Index]);
			Records[Order].PushBack(Probe->Velocities[Index]);
		}
		Scene.Shutdown_Internal();
	}
	REQUIRE(Records[0].Size() == 60 && Records[0].Size() == Records[1].Size());
	for (Toolbox::size_t Index = 0; Index < Records[0].Size(); ++Index)
	{
		REQUIRE(Records[0][Index] == Records[1][Index]);
	}
	// 物理Stepの直前の速度は、その固定更新の後の経路へ向かう値（開始時の位置は一つ前の経路）。
	REQUIRE(Abs(T::X(Records[0][2]) - StepSeconds * StepSeconds) < 1e-6);
}

// 一定の角速度の回転。k回の後の角度は角速度×k×dt（3Dは四元数の最短回転で、符号同値を含む一周を越える回転）。
template <typename T> void Rotates_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto Object = Scene.template Spawn<TMoverObject<T>>(Description_Internal<T>());
	REQUIRE(Object);
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	auto& Mover = Object.Value().Get()->Mover();
	constexpr f64 Rate = 1.5;
	Mover.SetPath(
	    [](f64 Seconds)
	    {
		    return T::Spin(Rate * Seconds);
	    });
	for (int32 Frame = 1; Frame <= 300; ++Frame)
	{
		REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds)));
		f64 Error = T::Turn(Mover.GetPose().Rotation) - Rate * StepSeconds * Frame;
		Error -= 6.283185307179586 * Floor((Error + 3.141592653589793) / 6.283185307179586);
		REQUIRE(Abs(Error) < 1e-3);
	}
	Scene.Shutdown_Internal();
}

// 上限を超える目標には上限の速さで向かい、Teleportは速度0で置き直す。
template <typename T> void LimitsAndTeleport_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto Description = Description_Internal<T>();
	Description.MaxLinearSpeed = 6;
	auto Object = Scene.template Spawn<TMoverObject<T>>(Description);
	REQUIRE(Object);
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	auto& Mover = Object.Value().Get()->Mover();
	typename T::FPose Far;
	Far.Position = T::At(100);
	Mover.SetTarget(Far);
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds)));
	REQUIRE(Mover.WasLimited() && Abs(T::X(Mover.GetPose().Position) - 6 * StepSeconds) < 1e-5);
	// 一回の目標は消費され、次の固定更新では止まる。
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds)));
	REQUIRE(!Mover.WasLimited() && Abs(T::X(Mover.GetPose().Position) - 6 * StepSeconds) < 1e-5);
	typename T::FPose Jump;
	Jump.Position = T::At(50);
	Mover.Teleport(Jump);
	REQUIRE(Mover.GetPose().Position == T::At(50) && Mover.GetTeleportCount() == 1);
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds)));
	REQUIRE(Mover.GetPose().Position == T::At(50));
	bool bThrew = false;
	try
	{
		Description.MaxLinearSpeed = 0;
		typename T::FMover Invalid(Description);
	}
	catch (const FException&)
	{
		bThrew = true;
	}
	REQUIRE(bThrew);
	Scene.Shutdown_Internal();
}

// Stepの前に予測した床の点の位置は、Step後の実際の位置と一致する（並進と回転、Staticは同じ位置、Dynamicは拒否）。
template <typename T> void PredictsPoint_Internal()
{
	FFixture Fixture;
	typename T::FScene Scene;
	auto Object = Scene.template Spawn<TMoverObject<T>>(Description_Internal<T>());
	REQUIRE(Object);
	REQUIRE(Scene.Initialize_Internal(Fixture.Init()));
	auto& Mover = Object.Value().Get()->Mover();
	Mover.SetPath(
	    [](f64 Seconds)
	    {
		    auto Pose = T::Spin(2.0 * Seconds);
		    Pose.Position = T::At(static_cast<f32>(Seconds), static_cast<f32>(0.5 * Seconds));
		    return Pose;
	    });
	auto& World = Scene.GetPhysicsWorld();
	REQUIRE(Scene.Tick_Internal(Fixture.Tick(StepSeconds)));
	const auto Body = *Mover.GetBodyId();
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		// 速度・角速度を設定した状態（物理Stepの直前と同じ）で予測する。
		const auto Local = T::At(1.5f, 0.25f);
		const auto Point = T::PointAfter(World, Body, Local);
		const auto Predicted = World.PredictBodyPoint(Body, Point, StepSeconds);
		World.Step(StepSeconds);
		REQUIRE(T::Distance(Predicted, T::PointAfter(World, Body, Local)) < 1e-5);
	}
	REQUIRE(World.GetBodyType(Body) == EBodyType::Kinematic);
	// Dynamicは力・接触で変わるため予測しない。Staticは同じ位置。
	const auto Dynamic = World.CreateBody({});
	bool bThrew = false;
	try
	{
		(void)World.PredictBodyPoint(Dynamic, T::At(0), StepSeconds);
	}
	catch (const FException&)
	{
		bThrew = true;
	}
	REQUIRE(bThrew);
	Scene.Shutdown_Internal();
}
} // namespace

TEST("2D kinematic mover follows its path once per physics step")
{
	FollowsPathOnce_Internal<FCase2D>();
}
TEST("3D kinematic mover follows its path once per physics step")
{
	FollowsPathOnce_Internal<FCase3D>();
}
TEST("2D kinematic mover motion does not depend on spawn order")
{
	OrderIndependent_Internal<FCase2D>();
}
TEST("3D kinematic mover motion does not depend on spawn order")
{
	OrderIndependent_Internal<FCase3D>();
}
TEST("2D kinematic mover rotates along the shortest path")
{
	Rotates_Internal<FCase2D>();
}
TEST("3D kinematic mover rotates along the shortest path")
{
	Rotates_Internal<FCase3D>();
}
TEST("2D kinematic mover limits speed and teleports without carrying")
{
	LimitsAndTeleport_Internal<FCase2D>();
}
TEST("3D kinematic mover limits speed and teleports without carrying")
{
	LimitsAndTeleport_Internal<FCase3D>();
}
TEST("2D body point prediction matches the next step")
{
	PredictsPoint_Internal<FCase2D>();
}
TEST("3D body point prediction matches the next step")
{
	PredictsPoint_Internal<FCase3D>();
}
