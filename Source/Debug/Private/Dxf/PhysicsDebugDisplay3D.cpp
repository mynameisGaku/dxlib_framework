// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsDebugDisplay3D.h"
namespace Dxf
{
namespace
{
// カプセルの線の形状（両端の球の辺と、中心線に平行な四本の側線）。
TResult<FGeometry3D> BuildCapsuleGeometry_Internal(const Toolbox::FCapsule& Capsule)
{
	auto Start = BuildSphereGeometry3D(Toolbox::FSphere{Capsule.Start, Capsule.Radius}, 12);
	if (!Start)
	{
		return Start;
	}
	auto End = BuildSphereGeometry3D(Toolbox::FSphere{Capsule.End, Capsule.Radius}, 12);
	if (!End)
	{
		return End;
	}
	FGeometry3D Geometry = Toolbox::Move(Start).Value();
	const FGeometry3D EndGeometry = Toolbox::Move(End).Value();
	for (const FLine3D& Line : EndGeometry.Lines)
	{
		Geometry.Lines.PushBack(Line);
	}
	const Toolbox::FVector3 Axis = Capsule.End - Capsule.Start;
	const Toolbox::f32 Length = Toolbox::Length(Axis);
	if (Length > 0)
	{
		// 中心線に直交する二方向。
		const Toolbox::FVector3 Direction = Axis * (1.0f / Length);
		const Toolbox::FVector3 Helper =
		    Toolbox::Abs(Direction.Y) < 0.9f ? Toolbox::FVector3{0, 1, 0} : Toolbox::FVector3{1, 0, 0};
		const Toolbox::FVector3 SideA = Toolbox::Normalize(Toolbox::Cross(Direction, Helper)) * Capsule.Radius;
		const Toolbox::FVector3 SideB = Toolbox::Cross(Direction, SideA);
		const Toolbox::FVector3 Sides[4] = {SideA, -SideA, SideB, -SideB};
		for (const Toolbox::FVector3& Side : Sides)
		{
			Geometry.Lines.PushBack({Capsule.Start + Side, Capsule.End + Side});
		}
	}
	return TResult<FGeometry3D>::Success(Toolbox::Move(Geometry));
}
} // namespace
TResult<FGeometryCommand3D> BuildPhysicsDebugGeometry3D(const FPhysicsDebugItem3D& Item,
	const FPhysicsDebugDisplaySettings3D& Settings)
{
	if (!Toolbox::IsFinite(Settings.VelocitySeconds) || Settings.VelocitySeconds < 0 || Settings.VelocitySeconds > 10)
	{
		return TResult<FGeometryCommand3D>::Failure(EErrorCode::InvalidArgument, "Invalid velocity display scale");
	}
	FGeometryCommand3D Command;
	Command.Options.Layer = ERenderLayer3D::Overlay;
	Command.Options.Depth = Settings.bAlwaysVisible ? EDepthMode3D::Always : EDepthMode3D::TestOnly;
	Command.Options.Color = Item.Type == EBodyType::Static ? FColor{140, 140, 140, 255} :
		(Item.bSleeping ? FColor{70, 140, 255, 255} : FColor{80, 255, 130, 255});
	if (Settings.bColliders)
	{
		auto Built = Item.Shape.Visit([](const auto& Shape) -> TResult<FGeometry3D>
		{
			if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Shape)>, Toolbox::FSphere>)
			{
				return BuildSphereGeometry3D(Shape, 12);
			}
			else if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Shape)>, Toolbox::FCapsule>)
			{
				return BuildCapsuleGeometry_Internal(Shape);
			}
			else
			{
				return BuildBoxGeometry3D(Shape);
			}
		});
		if (!Built)
		{
			return TResult<FGeometryCommand3D>::Failure(Built.Error());
		}
		Command.Geometry = Toolbox::Move(Built).Value();
		Command.Geometry.Triangles.Clear();
	}
	if (Settings.bVelocities && Settings.VelocitySeconds != 0 && Toolbox::LengthSquared(Item.Velocity) > 1e-12f)
	{
		Command.Geometry.Lines.PushBack({Item.CenterOfMass, Item.CenterOfMass + Item.Velocity * Settings.VelocitySeconds});
	}
	if (Settings.bCenters)
	{
		for (Toolbox::size_t Axis = 0; Axis < 3; ++Axis)
		{
			const Toolbox::FVector3 Delta{Axis == 0 ? 0.05f : 0, Axis == 1 ? 0.05f : 0, Axis == 2 ? 0.05f : 0};
			Command.Geometry.Lines.PushBack({Item.CenterOfMass - Delta, Item.CenterOfMass + Delta});
		}
	}
	if (!IsValidGeometry3D(Command))
	{
		return TResult<FGeometryCommand3D>::Failure(EErrorCode::InvalidArgument, "Debug shape is not representable");
	}
	return TResult<FGeometryCommand3D>::Success(Toolbox::Move(Command));
}
TResult<void> SubmitPhysicsDebugSnapshot3D(const FPhysicsDebugSnapshot3D& Snapshot,
	const FPhysicsDebugDisplaySettings3D& Settings, FRender3DContext& Render)
{
	if (!IsValidPhysicsDebugSnapshot3D(Snapshot) || !Toolbox::IsFinite(Settings.VelocitySeconds) ||
		Settings.VelocitySeconds < 0 || Settings.VelocitySeconds > 10)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid physics debug snapshot");
	}
	if (!Settings.bColliders && !Settings.bVelocities && !Settings.bCenters)
	{
		return {};
	}
	return Render.SubmitGenerated(Snapshot.Items.Size(), [&](Toolbox::size_t Index, FGeometryCommand3D& Output)
	{
		auto Built = BuildPhysicsDebugGeometry3D(Snapshot.Items[Index], Settings);
		if (!Built)
		{
			return TResult<void>::Failure(Built.Error());
		}
		Output = Toolbox::Move(Built).Value();
		return TResult<void>{};
	});
}
}
