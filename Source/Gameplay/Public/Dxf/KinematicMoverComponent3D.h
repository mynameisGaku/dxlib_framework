#pragma once
#include "Dxf/KinematicMoverComponent.h"
#include "Dxf/RigidBody3D.h"
namespace Dxf
{
/**
 * Kinematicの物体のComponentが使う、3DのWorldの型と操作。向きは単位四元数。
 */
struct FKinematicMoverTraits3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FVector = Toolbox::FVector3;
	using FRotation = Toolbox::FQuaternion;
	using FAngular = Toolbox::FVector3;
	using FColliderDescription = FColliderDescription3D;
	static constexpr const char* Name = "3D";
	static FRotation IdentityRotation() noexcept
	{
		return {};
	}
	static FWorld* GetWorld(const FFixedTickContext& Context) noexcept
	{
		return Context.Physics3D;
	}
	static TKinematicPose<FKinematicMoverTraits3D> GetPose(const FWorld& World, FBodyId Body)
	{
		TKinematicPose<FKinematicMoverTraits3D> Pose;
		Pose.Position = World.GetPosition(Body);
		Pose.Rotation = World.GetOrientation(Body);
		return Pose;
	}
	static FBodyDescription3D BodyDescription(const TKinematicPose<FKinematicMoverTraits3D>& Pose)
	{
		FBodyDescription3D Description;
		Description.Type = EBodyType::Kinematic;
		Description.Position = Pose.Position;
		Description.Orientation = Pose.Rotation;
		return Description;
	}
	// 差の四元数（To×From⁻¹）を符号同値のうちW≥0の側にして、最短の回転の角速度（軸×角度／秒）にする。
	static FAngular AngularVelocity(FRotation From, FRotation To, Toolbox::f64 Seconds) noexcept
	{
		Toolbox::FQuaternion Delta = To * From.Conjugate();
		Toolbox::f64 X = Delta.X;
		Toolbox::f64 Y = Delta.Y;
		Toolbox::f64 Z = Delta.Z;
		Toolbox::f64 W = Delta.W;
		if (W < 0)
		{
			X = -X;
			Y = -Y;
			Z = -Z;
			W = -W;
		}
		const Toolbox::f64 Sine = Toolbox::Sqrt(X * X + Y * Y + Z * Z);
		if (!(Sine > 1e-12))
		{
			return {};
		}
		const Toolbox::f64 Angle = 2.0 * Toolbox::Atan2(Sine, W);
		const Toolbox::f64 Scale = Angle / (Sine * Seconds);
		return {static_cast<Toolbox::f32>(X * Scale), static_cast<Toolbox::f32>(Y * Scale),
		        static_cast<Toolbox::f32>(Z * Scale)};
	}
	static Toolbox::f64 AngularSpeed(FAngular Angular) noexcept
	{
		return Length(Angular);
	}
	static FAngular ScaleAngular(FAngular Angular, Toolbox::f64 Scale) noexcept
	{
		return Angular * static_cast<Toolbox::f32>(Scale);
	}
	static Toolbox::f64 Length(FVector Value) noexcept
	{
		return Toolbox::Sqrt(Toolbox::f64(Value.X) * Value.X + Toolbox::f64(Value.Y) * Value.Y +
		                     Toolbox::f64(Value.Z) * Value.Z);
	}
	static FRotation Interpolate(FRotation From, FRotation To, Toolbox::f64 Alpha) noexcept
	{
		return Toolbox::FQuaternion::Slerp(From, To, static_cast<Toolbox::f32>(Alpha));
	}
	static void SetMotion(FWorld& World, FBodyId Body, FVector Velocity, FAngular Angular)
	{
		World.SetVelocity(Body, Velocity);
		World.SetAngularVelocity(Body, Angular);
	}
	static void Teleport(FWorld& World, FBodyId Body, const TKinematicPose<FKinematicMoverTraits3D>& Pose)
	{
		World.SetBodyTransform(Body, Pose.Position, Pose.Rotation);
		World.SetVelocity(Body, {});
		World.SetAngularVelocity(Body, {});
	}
};
/**
 * 3DのKinematicの物体の位置と向き。
 */
using FKinematicPose3D = TKinematicPose<FKinematicMoverTraits3D>;
/**
 * 3DのKinematicの物体の登録内容。
 */
using FKinematicMoverDescription3D = TKinematicMoverDescription<FKinematicMoverTraits3D>;
/**
 * 3DのKinematicの物体を固定更新で動かすComponent（TKinematicMoverComponentの3D版）。
 */
using DKinematicMover3DComponent = TKinematicMoverComponent<FKinematicMoverTraits3D>;
} // namespace Dxf
