// SPDX-License-Identifier: NOASSERTION
#include "PhysicsJoint.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
namespace
{
// 同じWorld系列を同期とJobで実行し、現行の公開値だけを照合する。
template <typename TWorld, typename TBody, typename TJoint, typename TVector>
Toolbox::int32 CheckJoint(TVector Offset, Toolbox::int32 Code)
{
	Toolbox::FJobSystem Jobs(2);
	TWorld World;
	Dxf::FPhysicsExecutionSettings Execution;
	Execution.JobSystem = &Jobs;
	World.SetExecutionSettings(Execution);
	World.SetGravity({});
	TBody Anchor;
	Anchor.Type = Dxf::EBodyType::Static;
	// 接続のA側。
	const auto A = World.CreateBody(Anchor);
	TBody Weight;
	Weight.Position = Offset;
	// 接続のB側。
	const auto B = World.CreateBody(Weight);
	Weight.Position = Offset * 2;
	const auto C = World.CreateBody(Weight);
	TJoint Description;
	Description.Length = 2;
	// 最初の試行または接続。
	const auto First = World.CreateDistanceJoint(A, B, Description);
	const auto Other = World.CreateDistanceJoint(B, C, Description);
	for (Toolbox::int32 Step = 0; Step < 60; ++Step)
	{
		World.Step(1.0 / 60);
	}
	if (!World.IsJointAlive(First) || Toolbox::Abs(World.GetDistanceJoint(First).Error) > 0.001 || World.GetExecutionDiagnostics().IslandCount != 1)
	{
		return Code;
	}
	if (!World.DestroyJoint(First))
	{
		return Code + 1;
	}
	const auto Next = World.CreateDistanceJoint(A, B, Description);
	if (Next.Index != First.Index || Next.Generation == First.Generation || World.IsJointAlive(First))
	{
		return Code + 2;
	}
	World.DestroyBody(B);
	if (World.IsJointAlive(Next) || World.IsJointAlive(Other) || !World.IsAlive(A) || !World.IsAlive(C))
	{
		return Code + 3;
	}
	const auto NewBody = World.CreateBody(Weight);
	if (NewBody.Index != B.Index || NewBody.Generation == B.Generation || World.IsAlive(B))
	{
		return Code + 4;
	}
	bool bRejected = false;
	try
	{
		(void)World.CreateDistanceJoint(A, B, Description);
	}
	catch (const Toolbox::FException&)
	{
		bRejected = true;
	}
	if (!bRejected)
	{
		return Code + 5;
	}
	World.Step(1.0 / 60);
	return 0;
}
} // namespace
Toolbox::int32 RunPhysicsJoint()
{
	// 最初の試行または接続。
	const auto First = CheckJoint<Dxf::FPhysicsWorld2D, Dxf::FBodyDescription2D, Dxf::FDistanceJointDescription2D>(Toolbox::FVector2{0, 2}, 601);
	if (First != 0)
	{
		return First;
	}
	return CheckJoint<Dxf::FPhysicsWorld3D, Dxf::FBodyDescription3D, Dxf::FDistanceJointDescription3D>(Toolbox::FVector3{0, 2, 0}, 611);
}
