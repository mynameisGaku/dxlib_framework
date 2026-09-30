// SPDX-License-Identifier: NOASSERTION
#include "JointCourse3D.h"
#include "JointCourseSupport.h"
namespace Dxf::GameplaySample
{
namespace
{
// 固定配置。圧力板の左の重りはCharacterで押してSensorへ入れられる。
constexpr Toolbox::f32 BodyX[9] = {8.6f, 8.6f, -3, -3, -3, -3, -3, 5, 5};
constexpr Toolbox::f32 BodyY[9] = {2.25f, 0.28f, 5.5f, 4.8f, 4.1f, 3.4f, 2.7f, 4.2f, 2};
} // namespace
void FJointCourse3D::Initialize(DPhysicsScene3D& Scene)
{
	m_pScene = &Scene;
	Regenerate();
}
void FJointCourse3D::Regenerate()
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
			FKinematicMoverDescription3D Mover;
			Mover.Pose.Position = {BodyX[Index], BodyY[Index], static_cast<Toolbox::f32>(Index >= 2 && Index <= 6 ? 0.12f * (Index - 2) : 0)};
			m_Carrier = JointCourseValue_Internal(Owner.AddComponent<DKinematicMover3DComponent>(Mover));
			continue;
		}
		FBodyDescription3D Body;
		Body.Position = {BodyX[Index], BodyY[Index], static_cast<Toolbox::f32>(Index >= 2 && Index <= 6 ? 0.12f * (Index - 2) : 0)};
		Body.Type = Index == 0 || Index == 2 ? EBodyType::Static : EBodyType::Dynamic;
		Body.Orientation = Toolbox::FQuaternion::FromAxisAngle({1, 2, 3}, 0.31f);
		Body.DiagonalInertia = {0.04f, 0.04f, 0.04f};
		Body.LinearDamping = 0.35f;
		Body.AngularDamping = 0.35f;
		m_Bodies[Index] = JointCourseValue_Internal(Owner.AddComponent<DRigidBody3DComponent>(Body));
		FColliderDescription3D Collider;
		Toolbox::FSphere Sphere;
		Sphere.Radius = Index == 1 ? 0.28f : 0.2f;
		Collider.Shape = Sphere;
		// 接続したBody同士も、既存FilterのままContactを生成する。
		(void)JointCourseValue_Internal(Owner.AddComponent<DCollider3DComponent>(Collider));
	}
	// 吊り下げ、四連結、移動支点。長さは接続設定に固定する。
	const Toolbox::size_t EndsA[6] = {0, 2, 3, 4, 5, 7};
	const Toolbox::size_t EndsB[6] = {1, 3, 4, 5, 6, 8};
	for (Toolbox::size_t Index = 0; Index < 6; ++Index)
	{
		FDistanceJointComponentDescription3D Settings;
		Settings.BodyA = Index == 5 ? FPhysicsBodyReference3D::FromKinematicMover(m_Carrier) : FPhysicsBodyReference3D::FromRigidBody(m_Bodies[EndsA[Index]]);
		Settings.BodyB = FPhysicsBodyReference3D::FromRigidBody(m_Bodies[EndsB[Index]]);
		Settings.Joint.Length = Index == 0 ? 1.89 : (Index == 5 ? 2.2 : 0.7);
		if (Index == 0)
		{
			Settings.Joint.LocalAnchorB = {0.05f, 0.08f, 0.06f};
		}
		m_Joints[Index] = JointCourseValue_Internal(m_Objects[EndsB[Index]].Get()->AddComponent<DDistanceJoint3DComponent>(Settings));
	}
	SetCarrierMoving(m_bCarrierMoving);
}
void FJointCourse3D::RemoveBody(Toolbox::size_t Index)
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
void FJointCourse3D::Kick()
{
	if (auto* Weight = m_Bodies[1].Get())
	{
		Weight->AddImpulse({1, 0});
	}
}
void FJointCourse3D::Disconnect()
{
	if (auto* Joint = m_Joints[0].Get())
	{
		Joint->RequestDisconnect();
	}
}
void FJointCourse3D::Reconnect()
{
	if (auto* Joint = m_Joints[0].Get())
	{
		Joint->RequestConnect(Joint->GetDescription());
	}
}
void FJointCourse3D::SetCarrierMoving(bool bMoving)
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
			                 FKinematicPose3D Pose;
			                 Pose.Position = {static_cast<Toolbox::f32>(5 + 0.6 * Toolbox::Sin(Seconds)), 4.2f};
			                 Pose.Rotation = Toolbox::FQuaternion::FromAxisAngle({1, 2, 3}, static_cast<Toolbox::f32>(0.15 * Toolbox::Sin(Seconds)));
			                 return Pose;
		                 });
	}
}
void FJointCourse3D::HandleInput(const FInputSnapshot& Input, bool bPaused)
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
const Toolbox::TArray<TObjectHandle<DRigidBody3DComponent>, 9>& FJointCourse3D::GetBodies() const noexcept
{
	return m_Bodies;
}
const Toolbox::TArray<TObjectHandle<DDistanceJoint3DComponent>, 6>& FJointCourse3D::GetJoints() const noexcept
{
	return m_Joints;
}
DKinematicMover3DComponent* FJointCourse3D::GetCarrier() const noexcept
{
	return m_Carrier.Get();
}
} // namespace Dxf::GameplaySample
