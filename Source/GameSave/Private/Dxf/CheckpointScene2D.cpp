// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointScene2D.h"
#include "Dxf/CheckpointSceneRuntime.h"
namespace Dxf::SavePrivate
{
class FCheckpointSceneRuntime2D : public TCheckpointSceneRuntime<DContentScene2D, FCheckpointCapture2D, FCheckpointDocument2D, DRevoluteJoint2DComponent, DPrismaticJoint2DComponent>
{
public:
	using TCheckpointSceneRuntime::TCheckpointSceneRuntime;
};
} // namespace Dxf::SavePrivate
namespace Dxf
{
ACheckpointScene2D::ACheckpointScene2D(FPreparedScene2D Prepared, FCheckpointDocument2D Checkpoint, FCheckpointLimits Limits) : DContentScene2D(Toolbox::Move(Prepared)), m_pRuntime(Toolbox::MakeUnique<SavePrivate::FCheckpointSceneRuntime2D>(Toolbox::Move(Checkpoint), Limits))
{
}
ACheckpointScene2D::~ACheckpointScene2D()
{
	Shutdown_Internal();
	m_pRuntime->Retire();
}
FCheckpointSceneTicket ACheckpointScene2D::GetResumeTicket() const noexcept
{
	return m_pRuntime->Ticket();
}
FCheckpointCapture2D& ACheckpointScene2D::GetCheckpointCapture() noexcept
{
	return m_pRuntime->Capture();
}
const Toolbox::FString& ACheckpointScene2D::GetGamePayload() const noexcept
{
	return m_pRuntime->Payload();
}
void ACheckpointScene2D::SetGamePayload(Toolbox::FString Payload)
{
	m_pRuntime->SetPayload(Toolbox::Move(Payload));
}
void ACheckpointScene2D::OnEnter(const FSceneActivationContext& Context) noexcept
{
	m_pRuntime->Activate();
	OnCheckpointActivated(Context);
}
void ACheckpointScene2D::OnCheckpointActivated(const FSceneActivationContext& Context) noexcept
{
	(void)Context;
}
void ACheckpointScene2D::OnCheckpointStepCompleted(const FFixedTickContext& Context)
{
	(void)Context;
}
void ACheckpointScene2D::OnPhysicsStepCompleted(const FFixedTickContext& Context)
{
	OnCheckpointStepCompleted(Context);
	m_pRuntime->Complete(*this, Context);
}
void ACheckpointScene2D::OnExit() noexcept
{
	m_pRuntime->Retire();
}
void ACheckpointScene2D::OnDeinitialize() noexcept
{
	m_pRuntime->Retire();
	DContentScene2D::OnDeinitialize();
}
} // namespace Dxf
