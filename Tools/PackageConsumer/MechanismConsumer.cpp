// SPDX-License-Identifier: NOASSERTION
#include "MechanismConsumer.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/RenderContext.h"
#include "Dxf/JointTargetMotion.h"
#include "Dxf/RevoluteJointComponent2D.h"
#include "Dxf/FixedJointComponent2D.h"
#include "Dxf/PrismaticJointComponent2D.h"
#include "Dxf/RevoluteJointComponent3D.h"
#include "Dxf/FixedJointComponent3D.h"
#include "Dxf/PrismaticJointComponent3D.h"
namespace
{
using namespace Dxf;
// 最初の公開操作の失敗を既存Sceneエラーとして返す。
void RequireMechanism(bool Value, const char* Message)
{
	if (!Value)
	{
		throw Toolbox::FException(Message);
	}
}
// Sceneの固定更新後に観察と次の要求を扱う、利用者側の小さいObject。
class DMechanismObserver final : public DGameObject
{
public:
	explicit DMechanismObserver(Toolbox::TFunction<void()> Callback) : m_Callback(Toolbox::Move(Callback))
	{
	}

protected:
	void OnTick(const FTickContext&) override
	{
		m_Callback();
	}

private:
	// Sceneに所有される間だけ有効な観察処理。
	Toolbox::TFunction<void()> m_Callback;
};
// 2Dの実Scene。所有は既存のScene/Componentだけを使用する。
class DMechanismConsumer2D final : public DPhysicsScene2D
{
public:
	explicit DMechanismConsumer2D(FMechanismConsumerResult& Result) : m_Result(Result)
	{
		GetPhysicsWorld().SetGravity({});
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		(void)Spawn<DMechanismObserver>([this]()
		                                {
			                                Observe_Internal();
		                                })
		    .Value();
		// 接続Objectを両Bodyより先に生成し、PrePhysicsで登録を待つ。
		auto Link0 = Spawn<DGameObject>().Value();
		auto Left0 = Spawn<DGameObject>().Value();
		auto Right0 = Spawn<DGameObject>().Value();
		FBodyDescription2D Support0;
		Support0.Type = EBodyType::Static;
		Support0.Position.X = -3;
		auto A0 = Left0.Get()->AddComponent<DRigidBody2DComponent>(Support0).Value();
		FBodyDescription2D Load0;
		Load0.Position = Support0.Position;
		Load0.Position.Y = 1;
		m_Bodies[0] = Right0.Get()->AddComponent<DRigidBody2DComponent>(Load0).Value();
		FRevoluteJointComponentDescription2D Description0;
		Description0.BodyA = FPhysicsBodyReference2D::FromRigidBody(A0);
		Description0.BodyB = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[0]);
		Description0.Joint.FrameA.LocalAnchor.Y = 1;
		Description0.Joint.Drive = {true, 0.5, 10};
		Description0.Joint.Limits = {true, -0.8, 0.8};
		m_Revolute = Link0.Get()->AddComponent<DRevoluteJoint2DComponent>(Description0).Value();
		// 接続Objectを両Bodyより先に生成し、PrePhysicsで登録を待つ。
		auto Link1 = Spawn<DGameObject>().Value();
		auto Left1 = Spawn<DGameObject>().Value();
		auto Right1 = Spawn<DGameObject>().Value();
		FBodyDescription2D Support1;
		Support1.Type = EBodyType::Static;
		Support1.Position.X = 0;
		auto A1 = Left1.Get()->AddComponent<DRigidBody2DComponent>(Support1).Value();
		FBodyDescription2D Load1;
		Load1.Position = Support1.Position;
		Load1.Position.Y = 1;
		m_Bodies[1] = Right1.Get()->AddComponent<DRigidBody2DComponent>(Load1).Value();
		FFixedJointComponentDescription2D Description1;
		Description1.BodyA = FPhysicsBodyReference2D::FromRigidBody(A1);
		Description1.BodyB = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[1]);
		Description1.Joint.FrameA.LocalAnchor.Y = 1;
		m_Fixed = Link1.Get()->AddComponent<DFixedJoint2DComponent>(Description1).Value();
		// 接続Objectを両Bodyより先に生成し、PrePhysicsで登録を待つ。
		auto Link2 = Spawn<DGameObject>().Value();
		auto Left2 = Spawn<DGameObject>().Value();
		auto Right2 = Spawn<DGameObject>().Value();
		FBodyDescription2D Support2;
		Support2.Type = EBodyType::Static;
		Support2.Position.X = 3;
		auto A2 = Left2.Get()->AddComponent<DRigidBody2DComponent>(Support2).Value();
		FBodyDescription2D Load2;
		Load2.Position = Support2.Position;
		Load2.Position.Y = 1;
		m_Bodies[2] = Right2.Get()->AddComponent<DRigidBody2DComponent>(Load2).Value();
		FPrismaticJointComponentDescription2D Description2;
		Description2.BodyA = FPhysicsBodyReference2D::FromRigidBody(A2);
		Description2.BodyB = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[2]);
		Description2.Joint.FrameA.LocalAnchor.Y = 1;
		Description2.Joint.Drive = {true, 0.5, 10};
		Description2.Joint.Limits = {true, -0.8, 0.8};
		m_Prismatic = Link2.Get()->AddComponent<DPrismaticJoint2DComponent>(Description2).Value();
		return {};
	}
	void Observe_Internal()
	{
		++m_Frame;
		if (const auto Observation = m_Revolute.Get()->GetObservation())
		{
			m_Result.ObservedTargetSpeed = Observation->State.Drive.TargetAngularSpeed;
			m_Result.ObservedAngularSpeed = Observation->State.AngularSpeed;
		}
		if (m_Result.bDriveRequested)
		{
			m_Revolute.Get()->RequestDrive({true, m_Result.RequestedSpeed, 10});
			m_Prismatic.Get()->RequestDrive({true, m_Result.RequestedSpeed, 10});
			m_Result.bDriveRequested = false;
			++m_Result.AppliedRequests;
		}
		if (m_Frame == 3)
		{
			RequireMechanism(m_Revolute.Get()->GetObservation() && m_Revolute.Get()->GetObservation()->State.AnchorError < 0.001, "external mechanism observation");
			m_First[0] = *m_Revolute.Get()->GetJointId();
		}
		if (m_Frame >= 5 && m_Frame < 20)
		{
			m_Revolute.Get()->RequestConnect(m_Revolute.Get()->GetDescription());
			RequireMechanism(*m_Revolute.Get()->GetJointId() == m_First[0], "external same connect changed ID");
		}
		if (m_Frame == 25)
		{
			m_Revolute.Get()->RequestDisconnect();
		}
		if (m_Frame == 28)
		{
			RequireMechanism(!m_Revolute.Get()->GetJointId(), "external disconnect");
			m_Revolute.Get()->RequestConnect(m_Revolute.Get()->GetDescription());
		}
		if (m_Frame == 32)
		{
			RequireMechanism(m_Revolute.Get()->GetJointId() && *m_Revolute.Get()->GetJointId() != m_First[0], "external reconnect generation");
		}
		if (m_Frame == 3)
		{
			RequireMechanism(m_Fixed.Get()->GetObservation() && m_Fixed.Get()->GetObservation()->State.AnchorError < 0.001, "external mechanism observation");
			m_First[1] = *m_Fixed.Get()->GetJointId();
		}
		if (m_Frame >= 5 && m_Frame < 20)
		{
			m_Fixed.Get()->RequestConnect(m_Fixed.Get()->GetDescription());
			RequireMechanism(*m_Fixed.Get()->GetJointId() == m_First[1], "external same connect changed ID");
		}
		if (m_Frame == 25)
		{
			m_Fixed.Get()->RequestDisconnect();
		}
		if (m_Frame == 28)
		{
			RequireMechanism(!m_Fixed.Get()->GetJointId(), "external disconnect");
			m_Fixed.Get()->RequestConnect(m_Fixed.Get()->GetDescription());
		}
		if (m_Frame == 32)
		{
			RequireMechanism(m_Fixed.Get()->GetJointId() && *m_Fixed.Get()->GetJointId() != m_First[1], "external reconnect generation");
		}
		if (m_Frame == 3)
		{
			RequireMechanism(m_Prismatic.Get()->GetObservation() && m_Prismatic.Get()->GetObservation()->State.AnchorError < 0.001, "external mechanism observation");
			m_First[2] = *m_Prismatic.Get()->GetJointId();
		}
		if (m_Frame >= 5 && m_Frame < 20)
		{
			m_Prismatic.Get()->RequestConnect(m_Prismatic.Get()->GetDescription());
			RequireMechanism(*m_Prismatic.Get()->GetJointId() == m_First[2], "external same connect changed ID");
		}
		if (m_Frame == 25)
		{
			m_Prismatic.Get()->RequestDisconnect();
		}
		if (m_Frame == 28)
		{
			RequireMechanism(!m_Prismatic.Get()->GetJointId(), "external disconnect");
			m_Prismatic.Get()->RequestConnect(m_Prismatic.Get()->GetDescription());
		}
		if (m_Frame == 32)
		{
			RequireMechanism(m_Prismatic.Get()->GetJointId() && *m_Prismatic.Get()->GetJointId() != m_First[2], "external reconnect generation");
		}
		if (m_Frame == 40)
		{
			FAngularJointTargetSettings Target;
			Target.TargetAngle = 0.5;
			Target.MaxTorque = 10;
			const auto State = m_Revolute.Get()->GetObservation()->State;
			const auto Command = ComputeJointTargetDrive(Target, State.Angle, State.AngularSpeed, m_Revolute.Get()->GetDescription().Joint.Limits, 1.0 / 60);
			m_Revolute.Get()->RequestDrive(Command.Drive);
			m_Prismatic.Get()->RequestDrive({true, -0.5, 10});
			m_Prismatic.Get()->RequestLimits({true, -0.8, 0.8});
		}
		if (m_Frame == 50)
		{
			m_Result.bComplete = true;
		}
	}
	void OnDeinitialize() noexcept override
	{
		m_Result.bShutdown = !m_Revolute || !m_Revolute.Get()->GetJointId();
	}
	void OnDraw(FRenderContext& Render) const override
	{
		for (Toolbox::int32 Index = 0; Index < 3; ++Index)
		{
			const auto* Body = m_Bodies[Index].Get();
			if (Body == nullptr || !Body->HasBody())
			{
				continue;
			}
			const auto Position = Body->GetRenderPosition();
			FDrawStyle Style;
			Style.Color = {80, 220, 190, 255};
			RequireMechanism(static_cast<bool>(Render.Get2D().FillCircle({640 + Position.X * 80, 540 - Position.Y * 80}, 18, Style)), "external mechanism circle");
			m_Result.Positions[Index] = {Position.X, Position.Y, 0};
		}
		if (m_Result.DrawOverlay)
		{
			m_Result.DrawOverlay(Render);
		}
	}

private:
	// 保存先はScene終了まで生存する。
	FMechanismConsumerResult& m_Result;
	// 次回要求を送るまでの描画フレーム数。
	Toolbox::int32 m_Frame = 0;
	// Sceneが所有する接続への非所有参照。
	TObjectHandle<DRevoluteJoint2DComponent> m_Revolute;
	// Sceneが所有する接続への非所有参照。
	TObjectHandle<DFixedJoint2DComponent> m_Fixed;
	// Sceneが所有する接続への非所有参照。
	TObjectHandle<DPrismaticJoint2DComponent> m_Prismatic;
	// 物理と描画で共有する剛体Component。
	TObjectHandle<DRigidBody2DComponent> m_Bodies[3];
	// 同値要求との比較用の完全ID。
	FJointId2D m_First[3];
};
// 3Dの実Scene。所有は既存のScene/Componentだけを使用する。
class DMechanismConsumer3D final : public DPhysicsScene3D
{
public:
	explicit DMechanismConsumer3D(FMechanismConsumerResult& Result) : m_Result(Result)
	{
		GetPhysicsWorld().SetGravity({});
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		(void)Spawn<DMechanismObserver>([this]()
		                                {
			                                Observe_Internal();
		                                })
		    .Value();
		// 接続Objectを両Bodyより先に生成し、PrePhysicsで登録を待つ。
		auto Link0 = Spawn<DGameObject>().Value();
		auto Left0 = Spawn<DGameObject>().Value();
		auto Right0 = Spawn<DGameObject>().Value();
		FBodyDescription3D Support0;
		Support0.Type = EBodyType::Static;
		Support0.Position.X = -3;
		auto A0 = Left0.Get()->AddComponent<DRigidBody3DComponent>(Support0).Value();
		FBodyDescription3D Load0;
		Load0.Position = Support0.Position;
		Load0.Position.Y = 1;
		m_Bodies[0] = Right0.Get()->AddComponent<DRigidBody3DComponent>(Load0).Value();
		FRevoluteJointComponentDescription3D Description0;
		Description0.BodyA = FPhysicsBodyReference3D::FromRigidBody(A0);
		Description0.BodyB = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[0]);
		Description0.Joint.FrameA.LocalAnchor.Y = 1;
		Description0.Joint.Drive = {true, 0.5, 10};
		Description0.Joint.Limits = {true, -0.8, 0.8};
		m_Revolute = Link0.Get()->AddComponent<DRevoluteJoint3DComponent>(Description0).Value();
		// 接続Objectを両Bodyより先に生成し、PrePhysicsで登録を待つ。
		auto Link1 = Spawn<DGameObject>().Value();
		auto Left1 = Spawn<DGameObject>().Value();
		auto Right1 = Spawn<DGameObject>().Value();
		FBodyDescription3D Support1;
		Support1.Type = EBodyType::Static;
		Support1.Position.X = 0;
		auto A1 = Left1.Get()->AddComponent<DRigidBody3DComponent>(Support1).Value();
		FBodyDescription3D Load1;
		Load1.Position = Support1.Position;
		Load1.Position.Y = 1;
		Load1.Orientation = Toolbox::FQuaternion{0.1f, 0.2f, 0.15f, 0.963068f}.Normalized();
		m_Bodies[1] = Right1.Get()->AddComponent<DRigidBody3DComponent>(Load1).Value();
		FFixedJointComponentDescription3D Description1;
		Description1.BodyA = FPhysicsBodyReference3D::FromRigidBody(A1);
		Description1.BodyB = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[1]);
		Description1.Joint.FrameA.LocalAnchor.Y = 1;
		Description1.Joint.FrameB.LocalRotation = Load1.Orientation.Conjugate();
		m_Fixed = Link1.Get()->AddComponent<DFixedJoint3DComponent>(Description1).Value();
		// 接続Objectを両Bodyより先に生成し、PrePhysicsで登録を待つ。
		auto Link2 = Spawn<DGameObject>().Value();
		auto Left2 = Spawn<DGameObject>().Value();
		auto Right2 = Spawn<DGameObject>().Value();
		FBodyDescription3D Support2;
		Support2.Type = EBodyType::Static;
		Support2.Position.X = 3;
		auto A2 = Left2.Get()->AddComponent<DRigidBody3DComponent>(Support2).Value();
		FBodyDescription3D Load2;
		Load2.Position = Support2.Position;
		Load2.Position.Y = 1;
		m_Bodies[2] = Right2.Get()->AddComponent<DRigidBody3DComponent>(Load2).Value();
		FPrismaticJointComponentDescription3D Description2;
		Description2.BodyA = FPhysicsBodyReference3D::FromRigidBody(A2);
		Description2.BodyB = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[2]);
		Description2.Joint.FrameA.LocalAnchor.Y = 1;
		Description2.Joint.Drive = {true, 0.5, 10};
		Description2.Joint.Limits = {true, -0.8, 0.8};
		m_Prismatic = Link2.Get()->AddComponent<DPrismaticJoint3DComponent>(Description2).Value();
		return {};
	}
	void Observe_Internal()
	{
		++m_Frame;
		if (const auto Observation = m_Revolute.Get()->GetObservation())
		{
			m_Result.ObservedTargetSpeed = Observation->State.Drive.TargetAngularSpeed;
			m_Result.ObservedAngularSpeed = Observation->State.AngularSpeed;
		}
		if (m_Result.bDriveRequested)
		{
			m_Revolute.Get()->RequestDrive({true, m_Result.RequestedSpeed, 10});
			m_Prismatic.Get()->RequestDrive({true, m_Result.RequestedSpeed, 10});
			m_Result.bDriveRequested = false;
			++m_Result.AppliedRequests;
		}
		if (m_Frame == 3)
		{
			RequireMechanism(m_Revolute.Get()->GetObservation() && m_Revolute.Get()->GetObservation()->State.AnchorError < 0.001, "external mechanism observation");
			m_First[0] = *m_Revolute.Get()->GetJointId();
		}
		if (m_Frame >= 5 && m_Frame < 20)
		{
			m_Revolute.Get()->RequestConnect(m_Revolute.Get()->GetDescription());
			RequireMechanism(*m_Revolute.Get()->GetJointId() == m_First[0], "external same connect changed ID");
		}
		if (m_Frame == 25)
		{
			m_Revolute.Get()->RequestDisconnect();
		}
		if (m_Frame == 28)
		{
			RequireMechanism(!m_Revolute.Get()->GetJointId(), "external disconnect");
			m_Revolute.Get()->RequestConnect(m_Revolute.Get()->GetDescription());
		}
		if (m_Frame == 32)
		{
			RequireMechanism(m_Revolute.Get()->GetJointId() && *m_Revolute.Get()->GetJointId() != m_First[0], "external reconnect generation");
		}
		if (m_Frame == 3)
		{
			RequireMechanism(m_Fixed.Get()->GetObservation() && m_Fixed.Get()->GetObservation()->State.AnchorError < 0.001, "external mechanism observation");
			m_First[1] = *m_Fixed.Get()->GetJointId();
		}
		if (m_Frame >= 5 && m_Frame < 20)
		{
			m_Fixed.Get()->RequestConnect(m_Fixed.Get()->GetDescription());
			RequireMechanism(*m_Fixed.Get()->GetJointId() == m_First[1], "external same connect changed ID");
		}
		if (m_Frame == 25)
		{
			m_Fixed.Get()->RequestDisconnect();
		}
		if (m_Frame == 28)
		{
			RequireMechanism(!m_Fixed.Get()->GetJointId(), "external disconnect");
			m_Fixed.Get()->RequestConnect(m_Fixed.Get()->GetDescription());
		}
		if (m_Frame == 32)
		{
			RequireMechanism(m_Fixed.Get()->GetJointId() && *m_Fixed.Get()->GetJointId() != m_First[1], "external reconnect generation");
		}
		if (m_Frame == 3)
		{
			RequireMechanism(m_Prismatic.Get()->GetObservation() && m_Prismatic.Get()->GetObservation()->State.AnchorError < 0.001, "external mechanism observation");
			m_First[2] = *m_Prismatic.Get()->GetJointId();
		}
		if (m_Frame >= 5 && m_Frame < 20)
		{
			m_Prismatic.Get()->RequestConnect(m_Prismatic.Get()->GetDescription());
			RequireMechanism(*m_Prismatic.Get()->GetJointId() == m_First[2], "external same connect changed ID");
		}
		if (m_Frame == 25)
		{
			m_Prismatic.Get()->RequestDisconnect();
		}
		if (m_Frame == 28)
		{
			RequireMechanism(!m_Prismatic.Get()->GetJointId(), "external disconnect");
			m_Prismatic.Get()->RequestConnect(m_Prismatic.Get()->GetDescription());
		}
		if (m_Frame == 32)
		{
			RequireMechanism(m_Prismatic.Get()->GetJointId() && *m_Prismatic.Get()->GetJointId() != m_First[2], "external reconnect generation");
		}
		if (m_Frame == 40)
		{
			FAngularJointTargetSettings Target;
			Target.TargetAngle = 0.5;
			Target.MaxTorque = 10;
			const auto State = m_Revolute.Get()->GetObservation()->State;
			const auto Command = ComputeJointTargetDrive(Target, State.Angle, State.AngularSpeed, m_Revolute.Get()->GetDescription().Joint.Limits, 1.0 / 60);
			m_Revolute.Get()->RequestDrive(Command.Drive);
			m_Prismatic.Get()->RequestDrive({true, -0.5, 10});
			m_Prismatic.Get()->RequestLimits({true, -0.8, 0.8});
		}
		if (m_Frame == 50)
		{
			m_Result.bComplete = true;
		}
	}
	void OnDeinitialize() noexcept override
	{
		m_Result.bShutdown = !m_Revolute || !m_Revolute.Get()->GetJointId();
	}
	void OnDraw(FRenderContext& Render) const override
	{
		RequireMechanism(static_cast<bool>(Render.Get3D().SetView(GetMechanismConsumerView())), "external mechanism view");
		for (Toolbox::int32 Index = 0; Index < 3; ++Index)
		{
			const auto* Body = m_Bodies[Index].Get();
			if (Body == nullptr || !Body->HasBody())
			{
				continue;
			}
			const auto Position = Body->GetRenderPosition();
			FDrawStyle3D Style;
			Style.Color = {80, 220, 190, 255};
			RequireMechanism(static_cast<bool>(Render.Get3D().DrawSphere({Position, 0.25f}, Style, 16)), "external mechanism sphere");
			m_Result.Positions[Index] = Position;
		}
		if (m_Result.DrawOverlay)
		{
			m_Result.DrawOverlay(Render);
		}
	}

private:
	// 保存先はScene終了まで生存する。
	FMechanismConsumerResult& m_Result;
	// 次回要求を送るまでの描画フレーム数。
	Toolbox::int32 m_Frame = 0;
	// Sceneが所有する接続への非所有参照。
	TObjectHandle<DRevoluteJoint3DComponent> m_Revolute;
	// Sceneが所有する接続への非所有参照。
	TObjectHandle<DFixedJoint3DComponent> m_Fixed;
	// Sceneが所有する接続への非所有参照。
	TObjectHandle<DPrismaticJoint3DComponent> m_Prismatic;
	// 物理と描画で共有する剛体Component。
	TObjectHandle<DRigidBody3DComponent> m_Bodies[3];
	// 同値要求との比較用の完全ID。
	FJointId3D m_First[3];
};
} // namespace
Dxf::FRenderView3D GetMechanismConsumerView()
{
	Dxf::FRenderView3D View;
	View.Eye = {0, 4, -10};
	View.Target = {0, 1, 0};
	return View;
}
Toolbox::TUniquePtr<Dxf::DScene> MakeMechanismConsumer2D(FMechanismConsumerResult& Result)
{
	return Toolbox::MakeUnique<DMechanismConsumer2D>(Result);
}
Toolbox::TUniquePtr<Dxf::DScene> MakeMechanismConsumer3D(FMechanismConsumerResult& Result)
{
	return Toolbox::MakeUnique<DMechanismConsumer3D>(Result);
}
Toolbox::int32 RunMechanismConsumer(Dxf::FAssetService& Assets, Dxf::FAudioPlayer& Audio)
{
	for (Toolbox::int32 Dimension = 0; Dimension < 2; ++Dimension)
	{
		FMechanismConsumerResult Result;
		Dxf::FSceneNavigator Navigator(Assets, Audio);
		Dxf::FInputStateTracker Input;
		Dxf::FFrameTime Time;
		Time.DeltaSeconds = 1.0 / 60;
		Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
		if (!Navigator.RequestChange(Dimension == 0 ? MakeMechanismConsumer2D(Result) : MakeMechanismConsumer3D(Result)) || !Navigator.Commit())
		{
			return 711;
		}
		for (Toolbox::int32 Frame = 0; Frame < 54; ++Frame)
		{
			if (!Navigator.CommitObjects() || !Navigator.Tick(Time, Input.GetSnapshot()))
			{
				return 712;
			}
		}
		Navigator.Shutdown();
		if (!Result.bComplete || !Result.bShutdown)
		{
			return 713;
		}
	}
	return 0;
}
