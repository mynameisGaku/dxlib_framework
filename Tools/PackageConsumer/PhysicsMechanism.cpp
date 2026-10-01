// SPDX-License-Identifier: NOASSERTION
#include "PhysicsMechanism.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include <stdio.h>
using namespace Dxf;
Toolbox::int32 RunPhysicsMechanisms()
{
	{
		FPhysicsWorld2D World;
		World.SetGravity({});
		FBodyDescription2D Support;
		Support.Type = EBodyType::Static;
		const auto A = World.CreateBody(Support);
		FBodyDescription2D Load;
		Load.Position.Y = 1;
		const auto B = World.CreateBody(Load);
		auto Description = World.MakeRevoluteJointDescription(A, B, World.GetPosition(B));
		const auto Joint = World.CreateRevoluteJoint(A, B, Description);
		World.SetRevoluteJointDrive(Joint, {true, 0.5, 10});
		World.SetRevoluteJointLimits(Joint, {true, -0.8, 0.8});
		for (Toolbox::int32 Step = 0; Step < 60; ++Step)
		{
			World.Step(1.0 / 60);
		}
		const auto State = World.GetRevoluteJoint(Joint);
		if (State.AnchorError > 0.001 || World.GetJointKind(Joint) != EJointKind::Revolute)
		{
			return 701;
		}
		World.SetRevoluteJointDrive(Joint, {true, -0.5, 5});
		World.SetRevoluteJointLimits(Joint, {false, -0.8, 0.8});
		World.Step(1.0 / 60);
		World.DestroyBody(B);
		if (World.IsJointAlive(Joint) || !World.IsAlive(A))
		{
			return 702;
		}
		printf("EXTERNAL_PHYSICS_MECHANISM Revolute-2D passed\n");
	}
	{
		FPhysicsWorld2D World;
		World.SetGravity({});
		FBodyDescription2D Support;
		Support.Type = EBodyType::Static;
		const auto A = World.CreateBody(Support);
		FBodyDescription2D Load;
		Load.Position.Y = 1;
		const auto B = World.CreateBody(Load);
		auto Description = World.MakeFixedJointDescription(A, B, World.GetPosition(B));
		const auto Joint = World.CreateFixedJoint(A, B, Description);
		for (Toolbox::int32 Step = 0; Step < 60; ++Step)
		{
			World.Step(1.0 / 60);
		}
		const auto State = World.GetFixedJoint(Joint);
		if (State.AnchorError > 0.001 || World.GetJointKind(Joint) != EJointKind::Fixed)
		{
			return 701;
		}
		World.DestroyBody(B);
		if (World.IsJointAlive(Joint) || !World.IsAlive(A))
		{
			return 702;
		}
		printf("EXTERNAL_PHYSICS_MECHANISM Fixed-2D passed\n");
	}
	{
		FPhysicsWorld2D World;
		World.SetGravity({});
		FBodyDescription2D Support;
		Support.Type = EBodyType::Static;
		const auto A = World.CreateBody(Support);
		FBodyDescription2D Load;
		Load.Position.Y = 1;
		const auto B = World.CreateBody(Load);
		auto Description = World.MakePrismaticJointDescription(A, B, World.GetPosition(B));
		const auto Joint = World.CreatePrismaticJoint(A, B, Description);
		World.SetPrismaticJointDrive(Joint, {true, 0.5, 10});
		World.SetPrismaticJointLimits(Joint, {true, -0.8, 0.8});
		for (Toolbox::int32 Step = 0; Step < 60; ++Step)
		{
			World.Step(1.0 / 60);
		}
		const auto State = World.GetPrismaticJoint(Joint);
		if (State.AnchorError > 0.001 || World.GetJointKind(Joint) != EJointKind::Prismatic)
		{
			return 701;
		}
		World.SetPrismaticJointDrive(Joint, {true, -0.5, 5});
		World.SetPrismaticJointLimits(Joint, {false, -0.8, 0.8});
		World.Step(1.0 / 60);
		World.DestroyBody(B);
		if (World.IsJointAlive(Joint) || !World.IsAlive(A))
		{
			return 702;
		}
		printf("EXTERNAL_PHYSICS_MECHANISM Prismatic-2D passed\n");
	}
	{
		FPhysicsWorld3D World;
		World.SetGravity({});
		FBodyDescription3D Support;
		Support.Type = EBodyType::Static;
		const auto A = World.CreateBody(Support);
		FBodyDescription3D Load;
		Load.Position.Y = 1;
		const auto B = World.CreateBody(Load);
		auto Description = World.MakeRevoluteJointDescription(A, B, World.GetPosition(B));
		const auto Joint = World.CreateRevoluteJoint(A, B, Description);
		World.SetRevoluteJointDrive(Joint, {true, 0.5, 10});
		World.SetRevoluteJointLimits(Joint, {true, -0.8, 0.8});
		for (Toolbox::int32 Step = 0; Step < 60; ++Step)
		{
			World.Step(1.0 / 60);
		}
		const auto State = World.GetRevoluteJoint(Joint);
		if (State.AnchorError > 0.001 || World.GetJointKind(Joint) != EJointKind::Revolute)
		{
			return 701;
		}
		World.SetRevoluteJointDrive(Joint, {true, -0.5, 5});
		World.SetRevoluteJointLimits(Joint, {false, -0.8, 0.8});
		World.Step(1.0 / 60);
		World.DestroyBody(B);
		if (World.IsJointAlive(Joint) || !World.IsAlive(A))
		{
			return 702;
		}
		printf("EXTERNAL_PHYSICS_MECHANISM Revolute-3D passed\n");
	}
	{
		FPhysicsWorld3D World;
		World.SetGravity({});
		FBodyDescription3D Support;
		Support.Type = EBodyType::Static;
		const auto A = World.CreateBody(Support);
		FBodyDescription3D Load;
		Load.Position.Y = 1;
		const auto B = World.CreateBody(Load);
		auto Description = World.MakeFixedJointDescription(A, B, World.GetPosition(B));
		const auto Joint = World.CreateFixedJoint(A, B, Description);
		for (Toolbox::int32 Step = 0; Step < 60; ++Step)
		{
			World.Step(1.0 / 60);
		}
		const auto State = World.GetFixedJoint(Joint);
		if (State.AnchorError > 0.001 || World.GetJointKind(Joint) != EJointKind::Fixed)
		{
			return 701;
		}
		World.DestroyBody(B);
		if (World.IsJointAlive(Joint) || !World.IsAlive(A))
		{
			return 702;
		}
		printf("EXTERNAL_PHYSICS_MECHANISM Fixed-3D passed\n");
	}
	{
		FPhysicsWorld3D World;
		World.SetGravity({});
		FBodyDescription3D Support;
		Support.Type = EBodyType::Static;
		const auto A = World.CreateBody(Support);
		FBodyDescription3D Load;
		Load.Position.Y = 1;
		const auto B = World.CreateBody(Load);
		auto Description = World.MakePrismaticJointDescription(A, B, World.GetPosition(B));
		const auto Joint = World.CreatePrismaticJoint(A, B, Description);
		World.SetPrismaticJointDrive(Joint, {true, 0.5, 10});
		World.SetPrismaticJointLimits(Joint, {true, -0.8, 0.8});
		for (Toolbox::int32 Step = 0; Step < 60; ++Step)
		{
			World.Step(1.0 / 60);
		}
		const auto State = World.GetPrismaticJoint(Joint);
		if (State.AnchorError > 0.001 || World.GetJointKind(Joint) != EJointKind::Prismatic)
		{
			return 701;
		}
		World.SetPrismaticJointDrive(Joint, {true, -0.5, 5});
		World.SetPrismaticJointLimits(Joint, {false, -0.8, 0.8});
		World.Step(1.0 / 60);
		World.DestroyBody(B);
		if (World.IsJointAlive(Joint) || !World.IsAlive(A))
		{
			return 702;
		}
		printf("EXTERNAL_PHYSICS_MECHANISM Prismatic-3D passed\n");
	}
	return 0;
}
