// SPDX-License-Identifier: NOASSERTION
#include "JointCourse2D.h"
#include "JointCourseSupport.h"
namespace Dxf::GameplaySample
{
namespace
{
// 固定配置。圧力板の左の重りはCharacterで押してSensorへ入れられる。
constexpr Toolbox::f32 BodyX[9] = {8.6f, 8.6f, -3, -3, -3, -3, -3, 5, 5};
constexpr Toolbox::f32 BodyY[9] = {2.25f, 0.28f, 5.5f, 4.8f, 4.1f, 3.4f, 2.7f, 4.2f, 2};
} // namespace
void FJointCourse2D::Initialize(DPhysicsScene2D& Scene)
{
	m_pScene = &Scene;
	Regenerate();
}
void FJointCourse2D::Regenerate()
{
	if (m_pScene == nullptr)
	{
		throw Toolbox::FException("Joint course has no scene");
	}
	for (const auto& Object : m_Objects)
	{
		if (auto* Alive = Object.Get())
		{
			Alive->Destroy();
		}
	}
	m_Bodies = {};
	m_Joints = {};
	for (Toolbox::size_t Index = 0; Index < 9; ++Index)
	{
		// Sceneが所有する次世代のBody Object。
		const auto Added = m_pScene->Spawn<DGameObject>();
		if (!Added)
		{
			throw Toolbox::FException(Added.Error().Message);
		}
		m_Objects[Index] = Added.Value();
		DGameObject& Owner = *Added.Value().Get();
		if (Index == 7)
		{
			FKinematicMoverDescription2D Mover;
			Mover.Pose.Position = {BodyX[Index], BodyY[Index]};
			m_Carrier = JointCourseValue_Internal(Owner.AddComponent<DKinematicMover2DComponent>(Mover));
			continue;
		}
		FBodyDescription2D Body;
		Body.Position = {BodyX[Index], BodyY[Index]};
		Body.Type = Index == 0 || Index == 2 ? EBodyType::Static : EBodyType::Dynamic;
		Body.Angle = 0.31f;
		Body.Inertia = 0.04f;
		Body.LinearDamping = 0.35f;
		Body.AngularDamping = 0.35f;
		m_Bodies[Index] = JointCourseValue_Internal(Owner.AddComponent<DRigidBody2DComponent>(Body));
		FColliderDescription2D Collider;
		Toolbox::FCircle2D Sphere;
		Sphere.Radius = Index == 1 ? 0.28f : 0.2f;
		Collider.Shape = Sphere;
		// 接続したBody同士も、既存FilterのままContactを生成する。
		(void)JointCourseValue_Internal(Owner.AddComponent<DCollider2DComponent>(Collider));
	}
	// 吊り下げ、四連結、移動支点。長さは接続設定に固定する。
	const Toolbox::size_t EndsA[6] = {0, 2, 3, 4, 5, 7};
	const Toolbox::size_t EndsB[6] = {1, 3, 4, 5, 6, 8};
	for (Toolbox::size_t Index = 0; Index < 6; ++Index)
	{
		FDistanceJointComponentDescription2D Settings;
		Settings.BodyA = Index == 5 ? FPhysicsBodyReference2D::FromKinematicMover(m_Carrier) : FPhysicsBodyReference2D::FromRigidBody(m_Bodies[EndsA[Index]]);
		Settings.BodyB = FPhysicsBodyReference2D::FromRigidBody(m_Bodies[EndsB[Index]]);
		Settings.Joint.Length = Index == 0 ? 1.89 : (Index == 5 ? 2.2 : 0.7);
		if (Index == 0)
		{
			Settings.Joint.LocalAnchorB = {0.05f, 0.08f};
		}
		m_Joints[Index] = JointCourseValue_Internal(m_Objects[EndsB[Index]].Get()->AddComponent<DDistanceJoint2DComponent>(Settings));
	}
	SetCarrierMoving(m_bCarrierMoving);
}
void FJointCourse2D::RemoveBody(Toolbox::size_t Index)
{
	if (Index >= m_Objects.Size())
	{
		throw Toolbox::FException("Joint course body index out of range");
	}
	if (auto* Body = m_Objects[Index].Get())
	{
		Body->Destroy();
	}
}
void FJointCourse2D::Kick()
{
	if (auto* Weight = m_Bodies[1].Get())
	{
		Weight->AddImpulse({1, 0});
	}
}
void FJointCourse2D::Disconnect()
{
	if (auto* Joint = m_Joints[0].Get())
	{
		Joint->RequestDisconnect();
	}
}
void FJointCourse2D::Reconnect()
{
	if (auto* Joint = m_Joints[0].Get())
	{
		Joint->RequestConnect(Joint->GetDescription());
	}
}
void FJointCourse2D::SetCarrierMoving(bool bMoving)
{
	m_bCarrierMoving = bMoving;
	if (auto* Carrier = m_Carrier.Get())
	{
		if (!bMoving)
		{
			Carrier->SetPath({});
			return;
		}
		Carrier->SetPath([](Toolbox::f64 Seconds)
		                 {
			                 FKinematicPose2D Pose;
			                 Pose.Position = {static_cast<Toolbox::f32>(5 + 0.6 * Toolbox::Sin(Seconds)), 4.2f};
			                 Pose.Rotation = static_cast<Toolbox::f32>(0.15 * Toolbox::Sin(Seconds));
			                 return Pose;
		                 });
	}
}
void FJointCourse2D::HandleInput(const FInputSnapshot& Input, bool bPaused)
{
	if (bPaused)
	{
		return;
	}
	if (Input.WasPressed(EKey::J))
	{
		Kick();
	}
	if (Input.WasPressed(EKey::K))
	{
		Disconnect();
	}
	if (Input.WasPressed(EKey::L))
	{
		Reconnect();
	}
	if (Input.WasPressed(EKey::N))
	{
		Regenerate();
	}
	if (Input.WasPressed(EKey::B))
	{
		SetCarrierMoving(!m_bCarrierMoving);
	}
}
const Toolbox::TArray<TObjectHandle<DRigidBody2DComponent>, 9>& FJointCourse2D::GetBodies() const noexcept
{
	return m_Bodies;
}
const Toolbox::TArray<TObjectHandle<DDistanceJoint2DComponent>, 6>& FJointCourse2D::GetJoints() const noexcept
{
	return m_Joints;
}
DKinematicMover2DComponent* FJointCourse2D::GetCarrier() const noexcept
{
	return m_Carrier.Get();
}
} // namespace Dxf::GameplaySample
