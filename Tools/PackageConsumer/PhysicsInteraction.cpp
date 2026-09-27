// SPDX-License-Identifier: NOASSERTION
#include "PhysicsInteraction.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"

namespace
{
// 物理更新1回の秒数。予測と実際のStepへ同じ値を渡す。
constexpr Toolbox::f64 StepSeconds = 1.0 / 60.0;

// 上位層を参照せず、Sensorの遷移と動く床の読み取り専用候補を同じ手順で検証する。
template <typename TWorld, typename TBody, typename TCollider, typename TSettings, typename TState, typename TInput,
          typename TVector, typename TBall, typename TBox>
Toolbox::int32 CheckPhysicsInteraction(TVector Start, TVector Velocity, TBall BallShape, TBox FloorShape,
                                       Toolbox::int32 Code)
{
	// 同じ位置のKinematicとSensor。Sensorは運動へ応答せず、問い合わせカテゴリ0でも通知する。
	TWorld Events;
	Dxf::FWorldEventSettings EventSettings;
	EventSettings.bEnabled = true;
	Events.SetEventSettings(EventSettings);
	TBody ActorDescription;
	ActorDescription.Type = Dxf::EBodyType::Kinematic;
	ActorDescription.Position = Start;
	const auto Actor = Events.CreateBody(ActorDescription);
	TCollider ActorShape;
	ActorShape.Shape = BallShape;
	const auto ActorCollider = Events.AttachCollider(Actor, ActorShape);
	TBody SensorDescription;
	SensorDescription.Type = Dxf::EBodyType::Static;
	SensorDescription.Position = Start;
	const auto Sensor = Events.CreateBody(SensorDescription);
	TCollider SensorShape = ActorShape;
	SensorShape.Response = Dxf::EColliderResponse::Sensor;
	SensorShape.QueryCategory = 0;
	const auto SensorCollider = Events.AttachCollider(Sensor, SensorShape);
	Events.Step(StepSeconds);
	// バッチを読み直しても消費しない。完全なCollider IDで相手を照合する。
	const auto& Begin = Events.GetEventBatch();
	if (!Begin.bPublished || Begin.Events.Size() != 1 || Begin.Events[0].Kind != Dxf::EWorldEventKind::Trigger ||
	    Begin.Events[0].Phase != Dxf::EWorldEventPhase::Begin || Begin.Events[0].ColliderA != ActorCollider ||
	    Begin.Events[0].ColliderB != SensorCollider || Events.GetPosition(Actor) != Start)
	{
		return Code;
	}
	const Toolbox::uint64 BeginBatch = Begin.BatchId;
	if (Events.GetEventBatch().BatchId != BeginBatch || Events.GetEventBatch().Events.Size() != 1)
	{
		return Code + 1;
	}
	Events.Step(StepSeconds);
	if (Events.GetEventBatch().Events.Size() != 1 ||
	    Events.GetEventBatch().Events[0].Phase != Dxf::EWorldEventPhase::Stay)
	{
		return Code + 2;
	}
	Events.DestroyBody(Sensor);
	Events.Step(StepSeconds);
	const auto& End = Events.GetEventBatch();
	if (!End.bPublished || End.Events.Size() != 1 || End.Events[0].Phase != Dxf::EWorldEventPhase::End ||
	    End.Events[0].EndReason != Dxf::EWorldEventEndReason::Removed || Events.IsColliderAlive(SensorCollider))
	{
		return Code + 3;
	}

	// 上面y=0の床と、床にはならないSensorを置く。支持計算は描画・Gameplayに依存しない。
	TWorld World;
	TBody FloorDescription;
	FloorDescription.Type = Dxf::EBodyType::Kinematic;
	const auto Floor = World.CreateBody(FloorDescription);
	TCollider FloorColliderDescription;
	FloorColliderDescription.Shape = FloorShape;
	const auto FloorCollider = World.AttachCollider(Floor, FloorColliderDescription);
	const auto OverlapSensor = World.CreateBody(SensorDescription);
	SensorShape.QueryCategory = 1;
	World.AttachCollider(OverlapSensor, SensorShape);
	const TSettings Settings;
	TState State;
	State.Center = Start;
	State.Ground = Dxf::ProbeCharacterGround(World, Start, Settings);
	if (State.Ground.State != Dxf::ECharacterGroundState::Walkable || !State.Ground.Collider ||
	    *State.Ground.Collider != FloorCollider)
	{
		return Code + 4;
	}
	World.SetVelocity(Floor, Velocity);
	TInput Input;
	for (Toolbox::int32 Index = 0; Index < 8; ++Index)
	{
		// 解析値は一定速度×時間。候補計算が床を先に進めていないことも確認する。
		const auto Before = World.GetPosition(Floor);
		const auto Prediction = World.PredictBodyPoint(Floor, State.Center, StepSeconds);
		const auto Result = Dxf::StepCharacter(World, Settings, State, Input, StepSeconds);
		const Toolbox::f64 ExpectedX = 2.0 * StepSeconds * (Index + 1);
		if (World.GetPosition(Floor) != Before || !Result.bCarried || Result.bCarryBlocked || !Result.Carrier ||
		    *Result.Carrier != FloorCollider || Toolbox::Abs(Prediction.X - ExpectedX) > 2e-4 ||
		    Toolbox::Abs(Result.State.Center.X - ExpectedX) > 2e-4 ||
		    Toolbox::Abs(Result.State.Center.Y - Start.Y) > 2e-4)
		{
			return Code + 5;
		}
		State = Result.State;
		World.Step(StepSeconds);
		if (Toolbox::Abs(World.GetPosition(Floor).X - ExpectedX) > 2e-4)
		{
			return Code + 6;
		}
	}
	// 支持点の速度2m/sをジャンプ時に一度だけ受け取り、空中では繰り返さない。
	Input.bJump = true;
	const auto Jump = Dxf::StepCharacter(World, Settings, State, Input, StepSeconds);
	if (!Jump.bJumped || !Jump.bInheritedGroundVelocity || Toolbox::Abs(Jump.State.Velocity.X - 2.0f) > 2e-4)
	{
		return Code + 7;
	}
	World.Step(StepSeconds);
	Input.bJump = false;
	const auto Airborne = Dxf::StepCharacter(World, Settings, Jump.State, Input, StepSeconds);
	if (Airborne.bInheritedGroundVelocity || Airborne.bCarried)
	{
		return Code + 8;
	}
	return 0;
}
} // namespace

Toolbox::int32 RunPhysicsInteraction()
{
	// 2Dと3Dで同じ運動・寸法を使い、次元固有の公開型も実際にリンクする。
	const Toolbox::int32 Result2D =
	    CheckPhysicsInteraction<Dxf::FPhysicsWorld2D, Dxf::FBodyDescription2D, Dxf::FColliderDescription2D,
	                            Dxf::FCharacterMoveSettings2D, Dxf::FCharacterState2D, Dxf::FCharacterMoveInput2D>(
	        Toolbox::FVector2{0, 0.52f}, Toolbox::FVector2{2, 0}, Toolbox::FCircle2D{{}, 0.5f},
	        Toolbox::FOrientedBox2D{{0, -0.25f}, {4, 0.25f}, 0}, 301);
	if (Result2D != 0)
	{
		return Result2D;
	}
	return CheckPhysicsInteraction<Dxf::FPhysicsWorld3D, Dxf::FBodyDescription3D, Dxf::FColliderDescription3D,
	                               Dxf::FCharacterMoveSettings3D, Dxf::FCharacterState3D, Dxf::FCharacterMoveInput3D>(
	    Toolbox::FVector3{0, 0.52f, 0}, Toolbox::FVector3{2, 0, 0}, Toolbox::FSphere{{}, 0.5f},
	    Toolbox::FOBB{{0, -0.25f, 0}, {4, 0.25f, 4}}, 321);
}
