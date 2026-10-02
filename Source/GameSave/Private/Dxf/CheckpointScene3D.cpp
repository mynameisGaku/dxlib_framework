// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointScene3D.h"
#include "Dxf/CheckpointSceneRuntime.h"
namespace Dxf::SavePrivate
{
class FCheckpointSceneRuntime3D : public TCheckpointSceneRuntime<DContentScene3D, FCheckpointCapture3D, FCheckpointDocument3D, DRevoluteJoint3DComponent, DPrismaticJoint3DComponent>
{
public:
	using TCheckpointSceneRuntime::TCheckpointSceneRuntime;
};
} // namespace Dxf::SavePrivate
namespace Dxf
{
ACheckpointScene3D::ACheckpointScene3D(FPreparedScene3D Prepared, FCheckpointDocument3D Checkpoint, FCheckpointLimits Limits) : DContentScene3D(Toolbox::Move(Prepared)), m_pRuntime(Toolbox::MakeUnique<SavePrivate::FCheckpointSceneRuntime3D>(Toolbox::Move(Checkpoint), Limits))
{
}
ACheckpointScene3D::~ACheckpointScene3D()
{
	Shutdown_Internal();
	m_pRuntime->Retire();
}
FCheckpointSceneTicket ACheckpointScene3D::GetResumeTicket() const noexcept
{
	return m_pRuntime->Ticket();
}
FCheckpointCapture3D& ACheckpointScene3D::GetCheckpointCapture() noexcept
{
	return m_pRuntime->Capture();
}
const Toolbox::FString& ACheckpointScene3D::GetGamePayload() const noexcept
{
	return m_pRuntime->Payload();
}
void ACheckpointScene3D::SetGamePayload(Toolbox::FString Payload)
{
	m_pRuntime->SetPayload(Toolbox::Move(Payload));
}
void ACheckpointScene3D::OnEnter(const FSceneActivationContext& Context) noexcept
{
	m_pRuntime->Activate();
	OnCheckpointActivated(Context);
}
void ACheckpointScene3D::OnCheckpointActivated(const FSceneActivationContext& Context) noexcept
{
	(void)Context;
}
void ACheckpointScene3D::OnCheckpointStepCompleted(const FFixedTickContext& Context)
{
	(void)Context;
}
void ACheckpointScene3D::OnPhysicsStepCompleted(const FFixedTickContext& Context)
{
	OnCheckpointStepCompleted(Context);
	m_pRuntime->Complete(*this, Context);
}
void ACheckpointScene3D::OnExit() noexcept
{
	m_pRuntime->Retire();
}
void ACheckpointScene3D::OnDeinitialize() noexcept
{
	m_pRuntime->Retire();
	DContentScene3D::OnDeinitialize();
}
} // namespace Dxf
