// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsDebugSnapshot3D.h"
namespace Dxf
{
namespace
{
bool ValidShape_Internal(const Toolbox::FSphere& Shape) noexcept
{
	return Shape.Center.IsValid() && Toolbox::IsFinite(Shape.Radius) && Shape.Radius > 0;
}
bool ValidShape_Internal(const Toolbox::FOBB& Shape) noexcept
{
	if (!Shape.Center.IsValid() || !Shape.HalfExtents.IsValid() ||
		Shape.HalfExtents.X <= 0 || Shape.HalfExtents.Y <= 0 || Shape.HalfExtents.Z <= 0)
	{
		return false;
	}
	for (Toolbox::size_t Axis = 0; Axis < 3; ++Axis)
	{
		if (!Shape.Axes[Axis].IsValid() || Toolbox::Abs(Toolbox::LengthSquared(Shape.Axes[Axis]) - 1) > 1e-4f)
		{
			return false;
		}
		for (Toolbox::size_t Other = Axis + 1; Other < 3; ++Other)
		{
			if (Toolbox::Abs(Toolbox::Dot(Shape.Axes[Axis], Shape.Axes[Other])) > 1e-4f)
			{
				return false;
			}
		}
	}
	return true;
}
}
bool IsValidPhysicsDebugSnapshot3D(const FPhysicsDebugSnapshot3D& Snapshot) noexcept
{
	if (!Toolbox::IsFinite(Snapshot.SimulationSeconds) || Snapshot.SimulationSeconds < 0 ||
		Snapshot.Items.Size() > 256 || Snapshot.SkippedCount > 256 ||
		Snapshot.Items.Size() + Snapshot.SkippedCount > 256 || (Snapshot.World == 0 && !Snapshot.Items.IsEmpty()))
	{
		return false;
	}
	for (const auto& Item : Snapshot.Items)
	{
		if (Item.BodyGeneration == 0 || Item.ColliderGeneration == 0 ||
			!Item.CenterOfMass.IsValid() || !Item.Velocity.IsValid() || !Item.AngularVelocity.IsValid() ||
			(Item.Motion != EDebugBodyMotion::Static && Item.Motion != EDebugBodyMotion::Kinematic &&
			Item.Motion != EDebugBodyMotion::Dynamic))
		{
			return false;
		}
		if (!Item.Shape.Visit([](const auto& Shape)
		{
			return ValidShape_Internal(Shape);
		}))
		{
			return false;
		}
	}
	return true;
}
}
