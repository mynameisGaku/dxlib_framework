// SPDX-License-Identifier: NOASSERTION
#include "Dxf/CheckpointSceneTicket.h"
#include "Dxf/CheckpointSceneStatus.h"
namespace Dxf
{
FCheckpointSceneTicket::FCheckpointSceneTicket(Toolbox::TSharedPtr<SavePrivate::FCheckpointSceneStatus> Status) : m_Status(Toolbox::Move(Status))
{
}
ECheckpointSceneState FCheckpointSceneTicket::GetState() const noexcept
{
	return m_Status ? m_Status->State : ECheckpointSceneState::Ended;
}
} // namespace Dxf
