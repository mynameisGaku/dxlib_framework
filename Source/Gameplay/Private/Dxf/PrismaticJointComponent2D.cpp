// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PrismaticJointComponent2D.h"
#include "Dxf/MechanismComponentValidation.h"
namespace Dxf
{
namespace
{
// Body登録を待つ場合も、数値の不正は待機で隠さない。
void Validate_Internal(const FPrismaticJointComponentDescription2D& Value)
{
	GameplayPrivate::ValidateJointFrame(Value.Joint.FrameA);
	GameplayPrivate::ValidateJointFrame(Value.Joint.FrameB);
	GameplayPrivate::ValidateJointLimits(Value.Joint.Limits);
	GameplayPrivate::ValidateJointDrive(Value.Joint.Drive);
}
// Drive／Limit変更では接続IDと基本拘束の再利用値を保持する。
bool Same_Internal(const FPrismaticJointComponentDescription2D& A, const FPrismaticJointComponentDescription2D& B) noexcept
{
	return A.BodyA == B.BodyA && A.BodyB == B.BodyB && GameplayPrivate::SameJointFrame(A.Joint.FrameA, B.Joint.FrameA) && GameplayPrivate::SameJointFrame(A.Joint.FrameB, B.Joint.FrameB);
}
} // namespace
DPrismaticJoint2DComponent::DPrismaticJoint2DComponent(FPrismaticJointComponentDescription2D Description) : m_Description(Description), m_Requested(Description)
{
	Validate_Internal(Description);
}
void DPrismaticJoint2DComponent::RequestDisconnect() noexcept
{
	m_bDisconnectRequested = true;
	m_bConnectRequested = false;
	m_bSettingsRequested = false;
}
void DPrismaticJoint2DComponent::RequestConnect(FPrismaticJointComponentDescription2D Description)
{
	Validate_Internal(Description);
	m_Requested = Description;
	m_bConnectRequested = true;
	m_bDisconnectRequested = false;
	m_bSettingsRequested = false;
}
void DPrismaticJoint2DComponent::RequestDrive(FLinearJointDrive Drive)
{
	// 待機中／解除要求中の値と現在接続の値を混同しない。
	auto Desired = m_bConnectRequested || m_bDisconnectRequested || m_bSettingsRequested ? m_Requested : m_Description;
	Desired.Joint.Drive = Drive;
	Validate_Internal(Desired);
	m_Requested = Desired;
	if (m_Connection == EJointConnection::Connected && !m_bDisconnectRequested)
	{
		m_bSettingsRequested = true;
	}
}
void DPrismaticJoint2DComponent::RequestLimits(FLinearJointLimits Limits)
{
	// 待機中／解除要求中の値と現在接続の値を混同しない。
	auto Desired = m_bConnectRequested || m_bDisconnectRequested || m_bSettingsRequested ? m_Requested : m_Description;
	Desired.Joint.Limits = Limits;
	Validate_Internal(Desired);
	m_Requested = Desired;
	if (m_Connection == EJointConnection::Connected && !m_bDisconnectRequested)
	{
		m_bSettingsRequested = true;
	}
}
bool DPrismaticJoint2DComponent::IsConnected_Internal() const noexcept
{
	if (IsDestroyRequested() || !IsInitialized() || m_pWorld == nullptr || !m_pWorld->IsJointAlive(m_Joint))
	{
		return false;
	}
	// ハンドルの失効はBodyの実解放より前に検出する。
	return m_Description.BodyA.IsAvailable_Internal(*m_pWorld) && m_Description.BodyB.IsAvailable_Internal(*m_pWorld);
}
EJointConnection DPrismaticJoint2DComponent::GetConnectionState() const noexcept
{
	if (m_Connection == EJointConnection::Connected && !IsConnected_Internal())
	{
		return EJointConnection::EndpointLost;
	}
	return m_Connection;
}
Toolbox::TOptional<FJointId2D> DPrismaticJoint2DComponent::GetJointId() const noexcept
{
	if (!IsConnected_Internal())
	{
		return {};
	}
	return m_Joint;
}
Toolbox::TOptional<FPrismaticJointObservation2D> DPrismaticJoint2DComponent::GetObservation() const noexcept
{
	if (!IsConnected_Internal())
	{
		return {};
	}
	return m_Observation;
}
const FPrismaticJointComponentDescription2D& DPrismaticJoint2DComponent::GetDescription() const noexcept
{
	return m_Description;
}
bool DPrismaticJoint2DComponent::GetRenderAnchors(Toolbox::FVector2& A, Toolbox::FVector2& B) const
{
	if (!IsConnected_Internal())
	{
		return false;
	}
	A = m_Description.BodyA.RenderAnchor_Internal(*m_pWorld, m_Description.Joint.FrameA.LocalAnchor);
	B = m_Description.BodyB.RenderAnchor_Internal(*m_pWorld, m_Description.Joint.FrameB.LocalAnchor);
	return true;
}
void DPrismaticJoint2DComponent::OnFixedTick(const FFixedTickContext& Context)
{
	if (Context.Physics2D == nullptr || Context.PrePhysicsStep == nullptr || Context.PostPhysicsStep == nullptr)
	{
		throw Toolbox::FException("Prismatic joint component requires PhysicsScene2D");
	}
	if (m_pWorld != nullptr && m_pWorld != Context.Physics2D)
	{
		throw Toolbox::FException("Prismatic joint component world changed");
	}
	m_pWorld = Context.Physics2D;
	// 失敗したStepの値を成功観察として残さない。
	m_Observation.Reset();
	Context.PrePhysicsStep->Enqueue(*this);
	Context.PostPhysicsStep->Enqueue(*this);
}
void DPrismaticJoint2DComponent::Release_Internal() noexcept
{
	if (m_pWorld != nullptr)
	{
		(void)m_pWorld->DestroyJoint(m_Joint);
	}
	m_Joint = {};
	m_Observation.Reset();
}
void DPrismaticJoint2DComponent::OnPrePhysicsStep_Internal(const FFixedTickContext&)
{
	if (!IsInitialized() || IsDestroyRequested())
	{
		return;
	}
	if (m_bDisconnectRequested)
	{
		Release_Internal();
		m_Connection = EJointConnection::Disconnected;
		m_bDisconnectRequested = false;
		m_bSettingsRequested = false;
		return;
	}
	if (m_Connection == EJointConnection::Connected && !IsConnected_Internal())
	{
		Release_Internal();
		m_Connection = EJointConnection::EndpointLost;
		m_bSettingsRequested = false;
	}
	if (!m_bConnectRequested && !m_bSettingsRequested)
	{
		return;
	}
	if (IsConnected_Internal() && Same_Internal(m_Description, m_Requested))
	{
		// 全設定は要求時に検証済み。同じ所有境界で設定を一括反映する。
		m_pWorld->SetPrismaticJointLimits(m_Joint, m_Requested.Joint.Limits);
		m_pWorld->SetPrismaticJointDrive(m_Joint, m_Requested.Joint.Drive);
		m_Description = m_Requested;
		m_bSettingsRequested = false;
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
			m_Connection = EJointConnection::PendingBodies;
		}
		return;
	}
	// 新規生成が成功するまで旧Jointを保持する。両方をStepへ持ち込まない。
	const auto NewJoint = m_pWorld->CreatePrismaticJoint(*A.Value(), *B.Value(), m_Requested.Joint);
	Release_Internal();
	m_Joint = NewJoint;
	m_Description = m_Requested;
	m_Connection = EJointConnection::Connected;
	m_bConnectRequested = false;
	m_bSettingsRequested = false;
}
bool DPrismaticJoint2DComponent::IsPostPhysicsStepAlive_Internal() const noexcept
{
	return IsInitialized() && !IsDestroyRequested();
}
void DPrismaticJoint2DComponent::OnPostPhysicsStep_Internal(const FFixedTickContext&)
{
	++m_SuccessfulStep;
	if (!IsConnected_Internal())
	{
		return;
	}
	// 成功した物理状態だけを採取する。
	const auto State = m_pWorld->GetPrismaticJoint(m_Joint);
	// 接続のA側。
	const auto A = m_Description.BodyA.Resolve_Internal(*m_pWorld);
	// 接続のB側。
	const auto B = m_Description.BodyB.Resolve_Internal(*m_pWorld);
	// 直近の成功Stepから得た値。
	FPrismaticJointObservation2D Observation;
	Observation.Joint = m_Joint;
	Observation.SuccessfulStep = m_SuccessfulStep;
	Observation.State = State;
	Observation.AnchorA = m_pWorld->GetPosition(*A.Value()) + GameplayPrivate::RotateJointAnchor(m_pWorld->GetAngle(*A.Value()), m_Description.Joint.FrameA.LocalAnchor);
	Observation.AnchorB = m_pWorld->GetPosition(*B.Value()) + GameplayPrivate::RotateJointAnchor(m_pWorld->GetAngle(*B.Value()), m_Description.Joint.FrameB.LocalAnchor);
	m_Observation = Observation;
}
void DPrismaticJoint2DComponent::OnDeinitialize() noexcept
{
	Release_Internal();
	m_pWorld = nullptr;
	m_bConnectRequested = false;
	m_bDisconnectRequested = false;
	m_bSettingsRequested = false;
	m_Connection = EJointConnection::Disconnected;
	m_Description = {};
	m_Requested = {};
}
} // namespace Dxf
