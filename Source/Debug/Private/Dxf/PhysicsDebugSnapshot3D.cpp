// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsDebugSnapshot3D.h"
#include "Toolbox/Log.h"
namespace Dxf
{
namespace
{
// 球の中心と半径が表示可能な値か調べる。
bool ValidShape_Internal(const Toolbox::FSphere& Shape) noexcept
{
	return Shape.Center.IsValid() && Toolbox::IsFinite(Shape.Radius) && Shape.Radius > 0;
}
// 箱の中心・半径・直交正規化された軸が表示可能な値か調べる。
bool ValidShape_Internal(const Toolbox::FOBB& Shape) noexcept
{
	if (!Shape.Center.IsValid() || !Shape.HalfExtents.IsValid() || Shape.HalfExtents.X <= 0 ||
	    Shape.HalfExtents.Y <= 0 || Shape.HalfExtents.Z <= 0)
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
// 運動区分が定義済みの値か調べる。
bool ValidType_Internal(EBodyType Type) noexcept
{
	return Type == EBodyType::Static || Type == EBodyType::Kinematic || Type == EBodyType::Dynamic;
}
// スロット昇順のBody配列から、世代まで一致するBodyを探す。見つからなければnullptr。
const FPhysicsSnapshot3D::FBody* FindBody_Internal(const FPhysicsSnapshot3D& Source, const FBodyId3D& Id) noexcept
{
	// 探索範囲の先頭と終端。
	Toolbox::size_t Low = 0;
	Toolbox::size_t High = Source.Bodies.Size();
	while (Low < High)
	{
		const Toolbox::size_t Middle = Low + (High - Low) / 2;
		if (Source.Bodies[Middle].Id.Index < Id.Index)
		{
			Low = Middle + 1;
		}
		else
		{
			High = Middle;
		}
	}
	if (Low == Source.Bodies.Size() || !(Source.Bodies[Low].Id == Id))
	{
		return nullptr;
	}
	return &Source.Bodies[Low];
}
} // namespace
bool IsValidPhysicsDebugSnapshot3D(const FPhysicsDebugSnapshot3D& Snapshot) noexcept
{
	if (!Toolbox::IsFinite(Snapshot.SimulationSeconds) || Snapshot.SimulationSeconds < 0 ||
	    !Toolbox::IsFinite(Snapshot.LastDeltaSeconds) || Snapshot.LastDeltaSeconds < 0 ||
	    Snapshot.Items.Size() > MaxPhysicsDebugColliders3D || Snapshot.BodyCount > MaxPhysicsDebugBodies3D ||
	    (Snapshot.World == 0 && (!Snapshot.Items.IsEmpty() || Snapshot.BodyCount != 0)))
	{
		return false;
	}
	for (const auto& Item : Snapshot.Items)
	{
		if (Item.Collider.Body.World != Snapshot.World || Item.Collider.Body.Generation == 0 ||
		    Item.Collider.Generation == 0 || !Item.CenterOfMass.IsValid() || !Item.Velocity.IsValid() ||
		    !Item.AngularVelocity.IsValid() || !ValidType_Internal(Item.Type))
		{
			return false;
		}
		if (!Item.Shape.Visit(
		        [](const auto& Shape)
		        {
			        return ValidShape_Internal(Shape);
		        }))
		{
			return false;
		}
	}
	return true;
}
TResult<FPhysicsDebugSnapshot3D> BuildPhysicsDebugSnapshot3D(const FPhysicsSnapshot3D& Source,
                                                             Toolbox::f64 SimulationSeconds)
{
	if (!Toolbox::IsFinite(SimulationSeconds) || SimulationSeconds < 0)
	{
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidArgument, "Invalid simulation seconds");
	}
	// 表示上限を超える場合は、一部だけを全件のように見せず失敗する。
	if (Source.Colliders.Size() > MaxPhysicsDebugColliders3D || Source.Bodies.Size() > MaxPhysicsDebugBodies3D)
	{
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidArgument,
		                                                 "Physics debug snapshot exceeds the display limit");
	}
	if (Source.World == 0)
	{
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidArgument, "Physics snapshot has no world");
	}
	FPhysicsDebugSnapshot3D Snapshot;
	Snapshot.World = Source.World;
	Snapshot.Step = Source.StepIndex;
	Snapshot.LastDeltaSeconds = Source.LastDeltaSeconds;
	Snapshot.LastSubSteps = Source.LastSubSteps;
	Snapshot.SimulationSeconds = SimulationSeconds;
	Snapshot.BodyCount = Source.Bodies.Size();
	Snapshot.Items.Reserve(Source.Colliders.Size());
	for (const auto& Collider : Source.Colliders)
	{
		// Colliderが指すBodyを、位置だけでなくWorldと世代まで一致させて取得する。
		const FPhysicsSnapshot3D::FBody* Body =
		    Collider.Id.Body.World == Source.World ? FindBody_Internal(Source, Collider.Id.Body) : nullptr;
		if (Body == nullptr)
		{
			return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidState,
			                                                 "Physics snapshot collider has no matching body");
		}
		FPhysicsDebugItem3D Item;
		Item.Collider = Collider.Id;
		Item.CenterOfMass = Body->Position;
		Item.Velocity = Body->Velocity;
		Item.AngularVelocity = Body->AngularVelocity;
		Item.Type = Body->Type;
		Item.bSleeping = Body->bSleeping;
		// 重心相対の中心と箱の軸へ、Body姿勢を一度だけ適用する。
		const Toolbox::FQuaternion Orientation = Body->Rotation;
		Collider.LocalShape.Visit(
		    [&](const auto& Local)
		    {
			    auto Transformed = Local;
			    Transformed.Center = Body->Position + Orientation.Rotate(Local.Center);
			    if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Local)>, Toolbox::FOBB>)
			    {
				    for (Toolbox::size_t Axis = 0; Axis < 3; ++Axis)
				    {
					    Transformed.Axes[Axis] = Orientation.Rotate(Local.Axes[Axis]);
				    }
			    }
			    Item.Shape = Toolbox::Move(Transformed);
		    });
		Snapshot.Items.PushBack(Toolbox::Move(Item));
	}
	if (!IsValidPhysicsDebugSnapshot3D(Snapshot))
	{
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidState, "Invalid captured physics state");
	}
	return TResult<FPhysicsDebugSnapshot3D>::Success(Toolbox::Move(Snapshot));
}
TResult<FPhysicsDebugSnapshot3D> CapturePhysicsDebugSnapshot3D(const FPhysicsWorld3D& World,
                                                               Toolbox::f64 SimulationSeconds)
{
	if (!Toolbox::IsFinite(SimulationSeconds) || SimulationSeconds < 0)
	{
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidArgument, "Invalid simulation seconds");
	}
	// 採取結果を受け取る一時領域。失敗時は呼出し側の既存値を変更しない。
	FPhysicsSnapshot3D Source;
	try
	{
		FPhysicsSnapshotLimits Limits;
		Limits.MaxBodies = MaxPhysicsDebugBodies3D;
		Limits.MaxColliders = MaxPhysicsDebugColliders3D;
		Source = World.CaptureSnapshot(Limits);
	}
	catch (const Toolbox::FException& Error)
	{
		// Worldが採取を拒否した理由（途中失敗したStep後・上限超過など）を記録する。
		DXF_LOG_WARNING("PhysicsDebug", "3D snapshot capture refused: %s", Error.What());
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidState, Error.What());
	}
	catch (...)
	{
		DXF_LOG_WARNING("PhysicsDebug", "3D snapshot capture failed with an unknown exception");
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidState, "Physics snapshot capture failed");
	}
	return BuildPhysicsDebugSnapshot3D(Source, SimulationSeconds);
}
} // namespace Dxf
