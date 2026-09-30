// SPDX-License-Identifier: NOASSERTION
#include "Dxf/DistanceJointComponent2D.h"
namespace Dxf
{
namespace
{
// Body登録を待つ場合も、数値の不正は待機で隠さない。
void Validate_Internal(const FDistanceJointComponentDescription2D& Value)
{
	if (!Toolbox::IsFinite(Value.Joint.Length) || Value.Joint.Length < 0 || !Value.Joint.LocalAnchorA.IsValid() || !Value.Joint.LocalAnchorB.IsValid())
	{
		throw Toolbox::FException("Invalid distance joint component settings");
	}
}
// 同じ有効な接続を維持し、保存Impulseを毎Step破棄しない。
bool Same_Internal(const FDistanceJointComponentDescription2D& A, const FDistanceJointComponentDescription2D& B) noexcept
{
	return A.BodyA == B.BodyA && A.BodyB == B.BodyB && A.Joint.Length == B.Joint.Length && A.Joint.LocalAnchorA == B.Joint.LocalAnchorA && A.Joint.LocalAnchorB == B.Joint.LocalAnchorB;
}
// 重心からのAnchorを平面の姿勢で回す。
Toolbox::FVector2 RotateAnchor_Internal(Toolbox::f32 Angle, Toolbox::FVector2 Local)
{
	return {static_cast<Toolbox::f32>(Toolbox::Cos(Angle) * Local.X - Toolbox::Sin(Angle) * Local.Y), static_cast<Toolbox::f32>(Toolbox::Sin(Angle) * Local.X + Toolbox::Cos(Angle) * Local.Y)};
}
} // namespace
DDistanceJoint2DComponent::DDistanceJoint2DComponent(FDistanceJointComponentDescription2D Description) : m_Description(Description), m_Requested(Description)
{
	Validate_Internal(Description);
}
void DDistanceJoint2DComponent::RequestDisconnect() noexcept
{
	m_bDisconnectRequested = true;
	m_bConnectRequested = false;
}
void DDistanceJoint2DComponent::RequestConnect(FDistanceJointComponentDescription2D Description)
{
	Validate_Internal(Description);
	m_Requested = Description;
	m_bConnectRequested = true;
	m_bDisconnectRequested = false;
}
bool DDistanceJoint2DComponent::IsConnected_Internal() const noexcept
{
	if (IsDestroyRequested() || !IsInitialized() || m_pWorld == nullptr || !m_pWorld->IsJointAlive(m_Joint))
	{
		return false;
	}
	// ハンドルの失効はBodyの実解放より前に検出する。
	return m_Description.BodyA.IsAvailable_Internal(*m_pWorld) && m_Description.BodyB.IsAvailable_Internal(*m_pWorld);
}
EDistanceJointConnection DDistanceJoint2DComponent::GetConnectionState() const noexcept
{
	if (m_Connection == EDistanceJointConnection::Connected && !IsConnected_Internal())
	{
		return EDistanceJointConnection::EndpointLost;
	}
	return m_Connection;
}
Toolbox::TOptional<FJointId2D> DDistanceJoint2DComponent::GetJointId() const noexcept
{
	if (!IsConnected_Internal())
	{
		return {};
	}
	return m_Joint;
}
Toolbox::TOptional<FDistanceJointObservation2D> DDistanceJoint2DComponent::GetObservation() const noexcept
{
	if (!IsConnected_Internal())
	{
		return {};
	}
	return m_Observation;
}
const FDistanceJointComponentDescription2D& DDistanceJoint2DComponent::GetDescription() const noexcept
{
	return m_Description;
}
bool DDistanceJoint2DComponent::GetRenderAnchors(Toolbox::FVector2& A, Toolbox::FVector2& B) const
{
	if (!IsConnected_Internal())
	{
		return false;
	}
	A = m_Description.BodyA.RenderAnchor_Internal(*m_pWorld, m_Description.Joint.LocalAnchorA);
	B = m_Description.BodyB.RenderAnchor_Internal(*m_pWorld, m_Description.Joint.LocalAnchorB);
	return true;
}
void DDistanceJoint2DComponent::OnFixedTick(const FFixedTickContext& Context)
{
	if (Context.Physics2D == nullptr || Context.PrePhysicsStep == nullptr || Context.PostPhysicsStep == nullptr)
	{
		throw Toolbox::FException("Distance joint component requires PhysicsScene2D");
	}
	if (m_pWorld != nullptr && m_pWorld != Context.Physics2D)
	{
		throw Toolbox::FException("Distance joint component world changed");
	}
	m_pWorld = Context.Physics2D;
	// 失敗したStepの値を成功観察として残さない。
	m_Observation.Reset();
	Context.PrePhysicsStep->Enqueue(*this);
	Context.PostPhysicsStep->Enqueue(*this);
}
void DDistanceJoint2DComponent::Release_Internal() noexcept
{
	if (m_pWorld != nullptr)
	{
		(void)m_pWorld->DestroyJoint(m_Joint);
	}
	m_Joint = {};
	m_Observation.Reset();
}
void DDistanceJoint2DComponent::OnPrePhysicsStep_Internal(const FFixedTickContext&)
{
	if (!IsInitialized() || IsDestroyRequested())
	{
		return;
	}
	if (m_bDisconnectRequested)
	{
		Release_Internal();
		m_Connection = EDistanceJointConnection::Disconnected;
		m_bDisconnectRequested = false;
		return;
	}
	if (m_Connection == EDistanceJointConnection::Connected && !IsConnected_Internal())
	{
		Release_Internal();
		m_Connection = EDistanceJointConnection::EndpointLost;
	}
	if (!m_bConnectRequested)
	{
		return;
	}
	if (IsConnected_Internal() && Same_Internal(m_Description, m_Requested))
	{
		m_bConnectRequested = false;
		return;
	}
	// 不正な参照を初期待機に読み替えない。
	const auto A = m_Requested.BodyA.Resolve_Internal(*m_pWorld);
	// 接続のB側。
	const auto B = m_Requested.BodyB.Resolve_Internal(*m_pWorld);
	if (!A || !B)
	{
		throw Toolbox::FException(!A ? A.Error().Message : B.Error().Message);
	}
	if (!A.Value() || !B.Value())
	{
		if (!IsConnected_Internal())
		{
			m_Connection = EDistanceJointConnection::PendingBodies;
		}
		return;
	}
	// 新規生成が成功するまで旧Jointを保持する。両方をStepへ持ち込まない。
	const auto NewJoint = m_pWorld->CreateDistanceJoint(*A.Value(), *B.Value(), m_Requested.Joint);
	Release_Internal();
	m_Joint = NewJoint;
	m_Description = m_Requested;
	m_Connection = EDistanceJointConnection::Connected;
	m_bConnectRequested = false;
}
bool DDistanceJoint2DComponent::IsPostPhysicsStepAlive_Internal() const noexcept
{
	return IsInitialized() && !IsDestroyRequested();
}
void DDistanceJoint2DComponent::OnPostPhysicsStep_Internal(const FFixedTickContext&)
{
	++m_SuccessfulStep;
	if (!IsConnected_Internal())
	{
		return;
	}
	// 成功した物理状態だけを採取する。
	const auto State = m_pWorld->GetDistanceJoint(m_Joint);
	// 接続のA側。
	const auto A = m_Description.BodyA.Resolve_Internal(*m_pWorld);
	// 接続のB側。
	const auto B = m_Description.BodyB.Resolve_Internal(*m_pWorld);
	// 直近の成功Stepから得た値。
	FDistanceJointObservation2D Observation;
	Observation.Joint = m_Joint;
	Observation.SuccessfulStep = m_SuccessfulStep;
	Observation.TargetLength = m_Description.Joint.Length;
	Observation.CurrentLength = State.CurrentLength;
	Observation.Error = State.Error;
	Observation.AnchorA = m_pWorld->GetPosition(*A.Value()) + RotateAnchor_Internal(m_pWorld->GetAngle(*A.Value()), m_Description.Joint.LocalAnchorA);
	Observation.AnchorB = m_pWorld->GetPosition(*B.Value()) + RotateAnchor_Internal(m_pWorld->GetAngle(*B.Value()), m_Description.Joint.LocalAnchorB);
	m_Observation = Observation;
}
void DDistanceJoint2DComponent::OnDeinitialize() noexcept
{
	Release_Internal();
	m_pWorld = nullptr;
	m_bConnectRequested = false;
	m_bDisconnectRequested = false;
	m_Connection = EDistanceJointConnection::Disconnected;
	m_Description = {};
	m_Requested = {};
}
} // namespace Dxf
