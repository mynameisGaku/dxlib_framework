// SPDX-License-Identifier: NOASSERTION
#include "MechanismCourse3D.h"
#include "JointCourseSupport.h"
namespace Dxf::GameplaySample
{
namespace
{
// Scene所有の更新Object。1画面でも2画面でも固定更新一回につき一回だけ操作する。
class DMechanismUpdater3D final : public DGameObject
{
public:
	explicit DMechanismUpdater3D(FMechanismCourse3D& Course) : m_pCourse(&Course)
	{
	}

protected:
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		if (!Context.bPaused)
		{
			m_pCourse->FixedUpdate(Context.DeltaSeconds);
		}
	}

private:
	// Sceneメンバーとして子より長く生存する装置群。
	FMechanismCourse3D* m_pCourse;
};
} // namespace
void FMechanismCourse3D::Initialize(DPhysicsScene3D& Scene)
{
	m_pScene = &Scene;
	// 前回の成功観察を読み、Joint Componentが今Stepの観察を無効にする前に要求を保存する。
	(void)JointCourseValue_Internal(Scene.Spawn<DMechanismUpdater3D>(*this));
	Regenerate();
}
void FMechanismCourse3D::Regenerate()
{
	if (m_pScene == nullptr)
	{
		throw Toolbox::FException("Mechanism course has no scene");
	}
	for (const auto& Handle : m_Objects)
	{
		if (auto* Object = Handle.Get())
		{
			Object->Destroy();
		}
	}
	m_Bodies = {};
	m_Revolutes = {};
	m_Prismatics = {};
	m_Fixed = {};
	for (auto& Connected : m_Connected)
	{
		Connected = true;
	}
	// 支点・扉・スライド・昇降・固定連結・吊り下げの初期重心。
	const Toolbox::FVector3 Positions[12] = {{-8, 7, 0}, {-8, 7, 0}, {-4, 7, 0}, {-4, 7, 0}, {0, 7, 0}, {0, 7, 0}, {4, 7, 0}, {4, 7, 0}, {7, 8, 0}, {8.6f, 8, 0}, {4, 7.8, 0}, {4, 5.5, 0}};
	for (Toolbox::size_t Index = 0; Index < 12; ++Index)
	{
		m_Objects[Index] = JointCourseValue_Internal(m_pScene->Spawn<DGameObject>());
		DGameObject& Owner = *m_Objects[Index].Get();
		if (Index == 6)
		{
			FKinematicMoverDescription3D Mover;
			Mover.Pose.Position = Positions[Index];
			m_Carrier = JointCourseValue_Internal(Owner.AddComponent<DKinematicMover3DComponent>(Mover));
			m_Carrier.Get()->SetPath([](Toolbox::f64 Seconds)
			                         {
				                         FKinematicPose3D Pose;
				                         Pose.Position = {static_cast<Toolbox::f32>(4 + 0.2 * Toolbox::Sin(Seconds)), 7, 0};
				                         Pose.Rotation = Toolbox::FQuaternion{static_cast<Toolbox::f32>(0.06 * Toolbox::Sin(Seconds)), 0, 0, 1}.Normalized();
				                         return Pose;
			                         });
			continue;
		}
		FBodyDescription3D Body;
		Body.Position = Positions[Index];
		Body.Type = Index == 0 || Index == 2 || Index == 4 ? EBodyType::Static : EBodyType::Dynamic;
		Body.Mass = Index == 10 ? 5.0f : 2.0f;
		Body.DiagonalInertia = Toolbox::FVector3{0.17666667f, 0.51f, 0.41666667f} * (Body.Mass / 2.0f);
		Body.GravityScale = Index == 7 || Index == 10 ? 1.0f : 0.0f;
		Body.LinearDamping = 0.1f;
		Body.AngularDamping = 0.1f;
		if (Index < 6 || Index == 9)
		{
			Body.Orientation = Toolbox::FQuaternion{0.1f, 0.2f, 0.15f, 0.963068f}.Normalized();
		}
		if (Index == 1 || Index == 3)
		{
			Body.Position = Body.Position + Toolbox::FQuaternion{0.1f, 0.2f, 0.15f, 0.963068f}.Normalized().Rotate({0.8f, 0, 0});
		}
		m_Bodies[Index] = JointCourseValue_Internal(Owner.AddComponent<DRigidBody3DComponent>(Body));
		FColliderDescription3D Collider;
		Toolbox::FOBB Box;
		Box.HalfExtents = {0.75, 0.25, 0.45};
		Collider.Shape = Box;
		// 支点を除いて通常の接触を有効にする。連結の衝突Filterは独立している。
		if (Index != 0 && Index != 2 && Index != 4)
		{
			(void)JointCourseValue_Internal(Owner.AddComponent<DCollider3DComponent>(Collider));
		}
	}
	for (Toolbox::size_t Index = 0; Index < 2; ++Index)
	{
		FRevoluteJointComponentDescription3D Settings;
		Settings.BodyA = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[Index * 2]);
		Settings.BodyB = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[Index * 2 + 1]);
		Settings.Joint.FrameB.LocalAnchor = {-0.8, 0, 0};
		Settings.Joint.Limits = {true, -0.8, 0.8};
		m_Revolutes[Index] = JointCourseValue_Internal(m_Objects[Index * 2 + 1].Get()->AddComponent<DRevoluteJoint3DComponent>(Settings));
	}
	for (Toolbox::size_t Index = 0; Index < 2; ++Index)
	{
		FPrismaticJointComponentDescription3D Settings;
		Settings.BodyA = Index == 0 ? FPhysicsBodyReference3D::FromRigidBody(m_Bodies[4]) : FPhysicsBodyReference3D::FromKinematicMover(m_Carrier);
		Settings.BodyB = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[Index == 0 ? 5 : 7]);
		Settings.Joint.Limits = {true, 0, 2};
		if (Index == 1)
		{
			Settings.Joint.FrameA.LocalRotation = Toolbox::FQuaternion{0, 0, 0.70710678f, 0.70710678f};
			Settings.Joint.FrameB.LocalRotation = Settings.Joint.FrameA.LocalRotation;
		}
		m_Prismatics[Index] = JointCourseValue_Internal(m_Objects[Index == 0 ? 5 : 7].Get()->AddComponent<DPrismaticJoint3DComponent>(Settings));
	}
	for (Toolbox::size_t Index = 0; Index < 2; ++Index)
	{
		FFixedJointComponentDescription3D Settings;
		Settings.BodyA = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[Index == 0 ? 8 : 7]);
		Settings.BodyB = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[Index == 0 ? 9 : 10]);
		Settings.Joint.FrameA.LocalAnchor = Index == 0 ? Toolbox::FVector3{1.6f, 0, 0} : Toolbox::FVector3{0, 0.8, 0};
		if (Index == 0)
		{
			Settings.Joint.FrameB.LocalRotation = Toolbox::FQuaternion{0.1f, 0.2f, 0.15f, 0.963068f}.Normalized().Conjugate();
		}
		m_Fixed[Index] = JointCourseValue_Internal(m_Objects[Index == 0 ? 9 : 10].Get()->AddComponent<DFixedJoint3DComponent>(Settings));
	}
	FDistanceJointComponentDescription3D Hanging;
	Hanging.BodyA = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[7]);
	Hanging.BodyB = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[11]);
	Hanging.Joint.Length = 1.5;
	(void)JointCourseValue_Internal(m_Objects[11].Get()->AddComponent<DDistanceJoint3DComponent>(Hanging));
}
void FMechanismCourse3D::FixedUpdate(Toolbox::f64 Seconds)
{
	++m_Updates;
	if (m_Controller.TakeKick())
	{
		const Toolbox::int32 Selected = m_Controller.GetSelection();
		if (auto* Body = m_Bodies[Selected == 4 ? 9 : 1].Get())
		{
			Body->AddImpulseAt({0, 2, 0}, Body->GetGamePosition() + Toolbox::FVector3{0.5f, 0, 0});
		}
	}
	for (Toolbox::int32 Selected = 0; Selected < 5; ++Selected)
	{
		const auto& Values = m_Controller.GetValues(Selected);
		const bool Connected = Values.bConnected;
		if (m_Connected[Selected] != Connected)
		{
			auto Apply = [Connected](auto* Joint)
			{
				if (Joint == nullptr)
				{
					return;
				}
				if (Connected)
				{
					Joint->RequestConnect(Joint->GetDescription());
				}
				else
				{
					Joint->RequestDisconnect();
				}
			};
			if (Selected < 2)
			{
				Apply(m_Revolutes[Selected].Get());
			}
			else if (Selected < 4)
			{
				Apply(m_Prismatics[Selected - 2].Get());
			}
			else
			{
				Apply(m_Fixed[0].Get());
			}
			m_Connected[Selected] = Connected;
		}
		if (Selected < 2)
		{
			if (auto* Joint = m_Revolutes[Selected].Get())
			{
				FAngularJointLimits Limits{Values.bLimited, -0.8, 0.8};
				Joint->RequestLimits(Limits);
				if (Selected == 0)
				{
					continue;
				}
				const auto Observation = Joint->GetObservation();
				if (!Observation)
				{
					continue;
				}
				FAngularJointTargetSettings Target;
				Target.TargetAngle = Values.Target * 0.65;
				Target.MaxAngularSpeed = Values.Speed;
				Target.MaxTorque = Values.Effort;
				auto Command = ComputeJointTargetDrive(Target, Observation->State.Angle, Observation->State.AngularSpeed, Limits, Seconds);
				if (!Values.bRunning)
				{
					Command.Drive.TargetAngularSpeed = 0;
				}
				Joint->RequestDrive(Command.Drive);
			}
		}
		else if (Selected < 4)
		{
			if (auto* Joint = m_Prismatics[Selected - 2].Get())
			{
				FLinearJointLimits Limits{Values.bLimited, 0, 2};
				Joint->RequestLimits(Limits);
				const auto Observation = Joint->GetObservation();
				if (!Observation)
				{
					continue;
				}
				FLinearJointTargetSettings Target;
				Target.TargetTranslation = Values.Target + 1;
				Target.MaxSpeed = Values.Speed;
				Target.MaxForce = Values.Effort;
				auto Command = ComputeJointTargetDrive(Target, Observation->State.Translation, Observation->State.TranslationRate, Limits, Seconds);
				if (!Values.bRunning)
				{
					Command.Drive.TargetSpeed = 0;
				}
				Joint->RequestDrive(Command.Drive);
			}
		}
	}
}
} // namespace Dxf::GameplaySample
