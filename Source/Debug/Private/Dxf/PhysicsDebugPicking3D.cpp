// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsDebugPicking3D.h"
#include "Toolbox/SegmentIntersection.h"
namespace Dxf
{
TResult<Toolbox::TOptional<FPhysicsDebugPick3D>> PickPhysicsDebugSnapshot3D(const FPhysicsDebugSnapshot3D& Snapshot, const FLine3D& Segment)
{
	using FResult = TResult<Toolbox::TOptional<FPhysicsDebugPick3D>>;
	if (!Segment.Start.IsValid() || !Segment.End.IsValid() || !IsValidPhysicsDebugSnapshot3D(Snapshot))
	{
		return FResult::Failure(EErrorCode::InvalidArgument, "Invalid physics picking snapshot or segment");
	}
	// 配列を並べ替えず、スロット重複と同じBodyの世代不一致を検査する。
	Toolbox::size_t Bodies = 0;
	for (Toolbox::size_t Index = 0; Index < Snapshot.Items.Size(); ++Index)
	{
		const auto& Id = Snapshot.Items[Index].Collider;
		bool FirstBody = true;
		for (Toolbox::size_t Previous = 0; Previous < Index; ++Previous)
		{
			const auto& Other = Snapshot.Items[Previous].Collider;
			if (Id.Index == Other.Index || (Id.Body.Index == Other.Body.Index && Id.Body != Other.Body))
			{
				return FResult::Failure(EErrorCode::InvalidArgument, "Inconsistent physics picking identities");
			}
			FirstBody = FirstBody && Id.Body != Other.Body;
		}
		Bodies += FirstBody ? 1 : 0;
	}
	if (Bodies > Snapshot.BodyCount)
	{
		return FResult::Failure(EErrorCode::InvalidArgument, "Physics picking body count mismatch");
	}
	// 最短候補を値だけで保持する。最短が0でも後続の計算不能を見落とさない。
	Toolbox::TOptional<FPhysicsDebugPick3D> Best;
	try
	{
		for (const auto& Item : Snapshot.Items)
		{
			const auto Hit = Item.Shape.Visit([&](const auto& Shape)
			                                  {
				                                  return Toolbox::IntersectSegment(Segment.Start, Segment.End, Shape);
			                                  });
			if (!Hit)
			{
				continue;
			}
			if (!Toolbox::IsFinite(*Hit) || *Hit < 0 || *Hit > 1)
			{
				return FResult::Failure(EErrorCode::InvalidArgument, "Invalid physics picking fraction");
			}
			if (Best && Best->Fraction <= *Hit)
			{
				continue;
			}
			FPhysicsDebugPick3D Pick;
			Pick.Collider = Item.Collider;
			Pick.Step = Snapshot.Step;
			Pick.Fraction = *Hit;
			// 差のf32オーバーフローを避け、倍精度の凸結合で交点を求める。
			Toolbox::f32 Coordinates[3]{};
			for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
			{
				const Toolbox::f64 Value = (1 - *Hit) * Segment.Start.Component(Axis) + *Hit * Segment.End.Component(Axis);
				if (!Toolbox::IsFinite(Value) || Toolbox::Abs(Value) > Toolbox::TNumericLimits<Toolbox::f32>::Max())
				{
					return FResult::Failure(EErrorCode::InvalidArgument, "Unrepresentable physics picking point");
				}
				Coordinates[Axis] = static_cast<Toolbox::f32>(Value);
			}
			Pick.Position = {Coordinates[0], Coordinates[1], Coordinates[2]};
			Best = Pick;
		}
	}
	catch (const Toolbox::FException& Error)
	{
		return FResult::Failure(EErrorCode::InvalidArgument, Error.What());
	}
	return FResult::Success(Best);
}
} // namespace Dxf
