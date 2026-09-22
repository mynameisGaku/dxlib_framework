// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsDebugSnapshot2D.h"
#include "Toolbox/Log.h"
namespace Dxf
{
namespace
{
// 円の中心と半径が表示可能な値か調べる。
bool ValidShape_Internal(const Toolbox::FCircle2D& Shape) noexcept
{
	return Shape.Center.IsValid() && Toolbox::IsFinite(Shape.Radius) && Shape.Radius > 0;
}
// 矩形の中心・半径・角度が表示可能な値か調べる。
bool ValidShape_Internal(const Toolbox::FOrientedBox2D& Shape) noexcept
{
	return Shape.Center.IsValid() && Shape.HalfExtents.IsValid() && Shape.HalfExtents.X > 0 &&
	       Shape.HalfExtents.Y > 0 && Toolbox::IsFinite(Shape.Angle);
}
// 運動区分が定義済みの値か調べる。
bool ValidType_Internal(EBodyType Type) noexcept
{
	return Type == EBodyType::Static || Type == EBodyType::Kinematic || Type == EBodyType::Dynamic;
}
// スロット昇順のBody配列から、世代まで一致するBodyを探す。見つからなければnullptr。
const FPhysicsSnapshot2D::FBody* FindBody_Internal(const FPhysicsSnapshot2D& Source, const FBodyId2D& Id) noexcept
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
// 重心相対の点をBodyの位置と角度でワールド座標へ移す。Physicsの形状変換と同じ式を使う。
Toolbox::FVector2 ToWorld_Internal(const FPhysicsSnapshot2D::FBody& Body, Toolbox::FVector2 Local) noexcept
{
	// 回転角の余弦と正弦。
	const Toolbox::f64 Cosine = Toolbox::Cos(Toolbox::f64(Body.Rotation));
	const Toolbox::f64 Sine = Toolbox::Sin(Toolbox::f64(Body.Rotation));
	return {static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.X) + Cosine * Local.X - Sine * Local.Y),
	        static_cast<Toolbox::f32>(Toolbox::f64(Body.Position.Y) + Sine * Local.X + Cosine * Local.Y)};
}
} // namespace
bool IsValidPhysicsDebugSnapshot2D(const FPhysicsDebugSnapshot2D& Snapshot) noexcept
{
	if (!Toolbox::IsFinite(Snapshot.SimulationSeconds) || Snapshot.SimulationSeconds < 0 ||
	    !Toolbox::IsFinite(Snapshot.LastDeltaSeconds) || Snapshot.LastDeltaSeconds < 0 ||
	    Snapshot.Items.Size() > MaxPhysicsDebugColliders2D || Snapshot.BodyCount > MaxPhysicsDebugBodies2D ||
	    (Snapshot.World == 0 && (!Snapshot.Items.IsEmpty() || Snapshot.BodyCount != 0)))
	{
		return false;
	}
	for (const auto& Item : Snapshot.Items)
	{
		if (Item.Collider.Body.World != Snapshot.World || Item.Collider.Body.Generation == 0 ||
		    Item.Collider.Generation == 0 || !Item.CenterOfMass.IsValid() || !Item.Velocity.IsValid() ||
		    !Toolbox::IsFinite(Item.BodyAngle) || !Toolbox::IsFinite(Item.AngularVelocity) ||
		    !ValidType_Internal(Item.Type))
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
TResult<FPhysicsDebugSnapshot2D> BuildPhysicsDebugSnapshot2D(const FPhysicsSnapshot2D& Source,
                                                             Toolbox::f64 SimulationSeconds)
{
	if (!Toolbox::IsFinite(SimulationSeconds) || SimulationSeconds < 0)
	{
		return TResult<FPhysicsDebugSnapshot2D>::Failure(EErrorCode::InvalidArgument, "Invalid simulation seconds");
	}
	// 表示上限を超える場合は、一部だけを全件のように見せず失敗する。
	if (Source.Colliders.Size() > MaxPhysicsDebugColliders2D || Source.Bodies.Size() > MaxPhysicsDebugBodies2D)
	{
		return TResult<FPhysicsDebugSnapshot2D>::Failure(EErrorCode::InvalidArgument,
		                                                 "Physics debug snapshot exceeds the display limit");
	}
	if (Source.World == 0)
	{
		return TResult<FPhysicsDebugSnapshot2D>::Failure(EErrorCode::InvalidArgument, "Physics snapshot has no world");
	}
	FPhysicsDebugSnapshot2D Snapshot;
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
		const FPhysicsSnapshot2D::FBody* Body =
		    Collider.Id.Body.World == Source.World ? FindBody_Internal(Source, Collider.Id.Body) : nullptr;
		if (Body == nullptr)
		{
			return TResult<FPhysicsDebugSnapshot2D>::Failure(EErrorCode::InvalidState,
			                                                 "Physics snapshot collider has no matching body");
		}
		FPhysicsDebugItem2D Item;
		Item.Collider = Collider.Id;
		Item.CenterOfMass = Body->Position;
		Item.BodyAngle = Body->Rotation;
		Item.Velocity = Body->Velocity;
		Item.AngularVelocity = Body->AngularVelocity;
		Item.Type = Body->Type;
		Item.bSleeping = Body->bSleeping;
		// 中心はBody角で回転させ、矩形の角度はBody角を一度だけ加える。
		Collider.LocalShape.Visit(
		    [&](const auto& Local)
		    {
			    auto Transformed = Local;
			    Transformed.Center = ToWorld_Internal(*Body, Local.Center);
			    if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Local)>, Toolbox::FOrientedBox2D>)
			    {
				    Transformed.Angle = Body->Rotation + Local.Angle;
			    }
			    Item.Shape = Toolbox::Move(Transformed);
		    });
		Snapshot.Items.PushBack(Toolbox::Move(Item));
	}
	if (!IsValidPhysicsDebugSnapshot2D(Snapshot))
	{
		return TResult<FPhysicsDebugSnapshot2D>::Failure(EErrorCode::InvalidState, "Invalid captured physics state");
	}
	return TResult<FPhysicsDebugSnapshot2D>::Success(Toolbox::Move(Snapshot));
}
TResult<FPhysicsDebugSnapshot2D> CapturePhysicsDebugSnapshot2D(const FPhysicsWorld2D& World,
                                                               Toolbox::f64 SimulationSeconds)
{
	if (!Toolbox::IsFinite(SimulationSeconds) || SimulationSeconds < 0)
	{
		return TResult<FPhysicsDebugSnapshot2D>::Failure(EErrorCode::InvalidArgument, "Invalid simulation seconds");
	}
	// 採取結果を受け取る一時領域。失敗時は呼出し側の既存値を変更しない。
	FPhysicsSnapshot2D Source;
	try
	{
		FPhysicsSnapshotLimits Limits;
		Limits.MaxBodies = MaxPhysicsDebugBodies2D;
		Limits.MaxColliders = MaxPhysicsDebugColliders2D;
		Source = World.CaptureSnapshot(Limits);
	}
	catch (const Toolbox::FException& Error)
	{
		// Worldが採取を拒否した理由（途中失敗したStep後・上限超過など）を記録する。
		DXF_LOG_WARNING("PhysicsDebug", "2D snapshot capture refused: %s", Error.What());
		return TResult<FPhysicsDebugSnapshot2D>::Failure(EErrorCode::InvalidState, Error.What());
	}
	catch (...)
	{
		DXF_LOG_WARNING("PhysicsDebug", "2D snapshot capture failed with an unknown exception");
		return TResult<FPhysicsDebugSnapshot2D>::Failure(EErrorCode::InvalidState, "Physics snapshot capture failed");
	}
	return BuildPhysicsDebugSnapshot2D(Source, SimulationSeconds);
}
} // namespace Dxf
