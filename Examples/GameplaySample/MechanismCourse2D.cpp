// SPDX-License-Identifier: NOASSERTION
#include "MechanismCourse2D.h"
#include "JointCourseSupport.h"
namespace Dxf::GameplaySample
{
namespace
{
// Scene所有の更新Object。1画面でも2画面でも固定更新一回につき一回だけ操作する。
class DMechanismUpdater2D final : public DGameObject
{
public:
	explicit DMechanismUpdater2D(FMechanismCourse2D& Course) : m_pCourse(&Course)
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
	FMechanismCourse2D* m_pCourse;
};
} // namespace
void FMechanismCourse2D::Initialize(DPhysicsScene2D& Scene)
{
	m_pScene = &Scene;
	// 前回の成功観察を読み、Joint Componentが今Stepの観察を無効にする前に要求を保存する。
	(void)JointCourseValue_Internal(Scene.Spawn<DMechanismUpdater2D>(*this));
	Regenerate();
}
void FMechanismCourse2D::Regenerate()
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
	const Toolbox::FVector2 Positions[12] = {{-8, 7}, {-8, 7}, {-4, 7}, {-4, 7}, {0, 7}, {0, 7}, {4, 7}, {4, 7}, {7, 8}, {8.6f, 8}, {4, 7.8}, {4, 5.5}};
	for (Toolbox::size_t Index = 0; Index < 12; ++Index)
	{
		m_Objects[Index] = JointCourseValue_Internal(m_pScene->Spawn<DGameObject>());
		DGameObject& Owner = *m_Objects[Index].Get();
		if (Index == 6)
		{
			FKinematicMoverDescription2D Mover;
			Mover.Pose.Position = Positions[Index];
			m_Carrier = JointCourseValue_Internal(Owner.AddComponent<DKinematicMover2DComponent>(Mover));
			m_Carrier.Get()->SetPath([](Toolbox::f64 Seconds)
			                         {
				                         FKinematicPose2D Pose;
				                         Pose.Position = {static_cast<Toolbox::f32>(4 + 0.2 * Toolbox::Sin(Seconds)), 7};
				                         Pose.Rotation = static_cast<Toolbox::f32>(0.12 * Toolbox::Sin(Seconds));
				                         return Pose;
			                         });
			continue;
		}
		FBodyDescription2D Body;
		Body.Position = Positions[Index];
		Body.Type = Index == 0 || Index == 2 || Index == 4 ? EBodyType::Static : EBodyType::Dynamic;
		Body.Mass = Index == 10 ? 5.0f : 2.0f;
		Body.Inertia = Body.Mass * (0.75f * 0.75f + 0.25f * 0.25f) / 3.0f;
		Body.GravityScale = Index == 7 || Index == 10 ? 1.0f : 0.0f;
		Body.LinearDamping = 0.1f;
		Body.AngularDamping = 0.1f;
		if (Index < 6 || Index == 9)
		{
			Body.Angle = 0.25f;
		}
		if (Index == 1 || Index == 3)
		{
			Body.Position = Body.Position + Toolbox::FVector2{static_cast<Toolbox::f32>(0.8 * Toolbox::Cos(0.25)), static_cast<Toolbox::f32>(0.8 * Toolbox::Sin(0.25))};
		}
		m_Bodies[Index] = JointCourseValue_Internal(Owner.AddComponent<DRigidBody2DComponent>(Body));
		FColliderDescription2D Collider;
		Toolbox::FOrientedBox2D Box;
		Box.HalfExtents = {0.75, 0.25};
		Collider.Shape = Box;
		// 支点を除いて通常の接触を有効にする。連結の衝突Filterは独立している。
		if (Index != 0 && Index != 2 && Index != 4)
		{
			(void)JointCourseValue_Internal(Owner.AddComponent<DCollider2DComponent>(Collider));
		}
	}
	for (Toolbox::size_t Index = 0; Index < 2; ++Index)
	{
		FRevoluteJointComponentDescription2D Settings;
		Settings.BodyA = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[Index * 2]);
		Settings.BodyB = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[Index * 2 + 1]);
		Settings.Joint.FrameB.LocalAnchor = {-0.8, 0};
		Settings.Joint.Limits = {true, -0.8, 0.8};
		m_Revolutes[Index] = JointCourseValue_Internal(m_Objects[Index * 2 + 1].Get()->AddComponent<DRevoluteJoint2DComponent>(Settings));
	}
	for (Toolbox::size_t Index = 0; Index < 2; ++Index)
	{
		FPrismaticJointComponentDescription2D Settings;
		Settings.BodyA = Index == 0 ? FPhysicsBodyReference2D::FromRigidBody(m_Bodies[4]) : FPhysicsBodyReference2D::FromKinematicMover(m_Carrier);
		Settings.BodyB = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[Index == 0 ? 5 : 7]);
		Settings.Joint.Limits = {true, 0, 2};
		if (Index == 1)
		{
			Settings.Joint.FrameA.LocalAngle = static_cast<Toolbox::f32>(1.5707963267948966);
			Settings.Joint.FrameB.LocalAngle = Settings.Joint.FrameA.LocalAngle;
		}
		m_Prismatics[Index] = JointCourseValue_Internal(m_Objects[Index == 0 ? 5 : 7].Get()->AddComponent<DPrismaticJoint2DComponent>(Settings));
	}
	for (Toolbox::size_t Index = 0; Index < 2; ++Index)
	{
		FFixedJointComponentDescription2D Settings;
		Settings.BodyA = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[Index == 0 ? 8 : 7]);
		Settings.BodyB = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[Index == 0 ? 9 : 10]);
		Settings.Joint.FrameA.LocalAnchor = Index == 0 ? Toolbox::FVector2{1.6f, 0} : Toolbox::FVector2{0, 0.8};
		if (Index == 0)
		{
			Settings.Joint.FrameB.LocalAngle = -0.25f;
		}
		m_Fixed[Index] = JointCourseValue_Internal(m_Objects[Index == 0 ? 9 : 10].Get()->AddComponent<DFixedJoint2DComponent>(Settings));
	}
	FDistanceJointComponentDescription2D Hanging;
	Hanging.BodyA = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[7]);
	Hanging.BodyB = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[11]);
	Hanging.Joint.Length = 1.5;
	(void)JointCourseValue_Internal(m_Objects[11].Get()->AddComponent<DDistanceJoint2DComponent>(Hanging));
}
void FMechanismCourse2D::FixedUpdate(Toolbox::f64 Seconds)
{
	++m_Updates;
	if (m_Controller.TakeKick())
	{
		const Toolbox::int32 Selected = m_Controller.GetSelection();
		if (auto* Body = m_Bodies[Selected == 4 ? 9 : 1].Get())
		{
			Body->AddImpulseAt({0, 2}, Body->GetGamePosition() + Toolbox::FVector2{0.5f, 0});
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
