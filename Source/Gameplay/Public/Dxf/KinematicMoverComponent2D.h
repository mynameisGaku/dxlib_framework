#pragma once
#include "Dxf/KinematicMoverComponent.h"
#include "Dxf/RigidBody2D.h"
namespace Dxf
{
/**
 * Kinematicの物体のComponentが使う、2DのWorldの型と操作。向きはラジアン角。
 */
struct FKinematicMoverTraits2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FVector = Toolbox::FVector2;
	using FRotation = Toolbox::f32;
	using FAngular = Toolbox::f32;
	using FColliderDescription = FColliderDescription2D;
	static constexpr const char* Name = "2D";
	static FRotation IdentityRotation() noexcept
	{
		return 0;
	}
	static FWorld* GetWorld(const FFixedTickContext& Context) noexcept
	{
		return Context.Physics2D;
	}
	static TKinematicPose<FKinematicMoverTraits2D> GetPose(const FWorld& World, FBodyId Body)
	{
		TKinematicPose<FKinematicMoverTraits2D> Pose;
		Pose.Position = World.GetPosition(Body);
		Pose.Rotation = World.GetAngle(Body);
		return Pose;
	}
	static FBodyDescription2D BodyDescription(const TKinematicPose<FKinematicMoverTraits2D>& Pose)
	{
		FBodyDescription2D Description;
		Description.Type = EBodyType::Kinematic;
		Description.Position = Pose.Position;
		Description.Angle = Pose.Rotation;
		return Description;
	}
	// 角度の差を-π〜πへ折り返して、最短の回転の角速度にする。
	static FAngular AngularVelocity(FRotation From, FRotation To, Toolbox::f64 Seconds) noexcept
	{
		Toolbox::f64 Delta = Toolbox::f64(To) - Toolbox::f64(From);
		Delta = Delta - 6.283185307179586 * Toolbox::Floor((Delta + 3.141592653589793) / 6.283185307179586);
		return static_cast<FAngular>(Delta / Seconds);
	}
	static Toolbox::f64 AngularSpeed(FAngular Angular) noexcept
	{
		return Toolbox::Abs(Toolbox::f64(Angular));
	}
	static FAngular ScaleAngular(FAngular Angular, Toolbox::f64 Scale) noexcept
	{
		return static_cast<FAngular>(Angular * Scale);
	}
	static Toolbox::f64 Length(FVector Value) noexcept
	{
		return Toolbox::Sqrt(Toolbox::f64(Value.X) * Value.X + Toolbox::f64(Value.Y) * Value.Y);
	}
	static FRotation Interpolate(FRotation From, FRotation To, Toolbox::f64 Alpha) noexcept
	{
		return static_cast<FRotation>(From + AngularVelocity(From, To, 1.0) * Alpha);
	}
	static void SetMotion(FWorld& World, FBodyId Body, FVector Velocity, FAngular Angular)
	{
		World.SetVelocity(Body, Velocity);
		World.SetAngularVelocity(Body, Angular);
	}
	static void Teleport(FWorld& World, FBodyId Body, const TKinematicPose<FKinematicMoverTraits2D>& Pose)
	{
		World.SetBodyTransform(Body, Pose.Position, Pose.Rotation);
		World.SetVelocity(Body, {});
		World.SetAngularVelocity(Body, 0);
	}
};
/**
 * 2DのKinematicの物体の位置と向き。
 */
using FKinematicPose2D = TKinematicPose<FKinematicMoverTraits2D>;
/**
 * 2DのKinematicの物体の登録内容。
 */
using FKinematicMoverDescription2D = TKinematicMoverDescription<FKinematicMoverTraits2D>;
/**
 * 2DのKinematicの物体を固定更新で動かすComponent（TKinematicMoverComponentの2D版）。
 */
using DKinematicMover2DComponent = TKinematicMoverComponent<FKinematicMoverTraits2D>;
} // namespace Dxf
