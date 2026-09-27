// SPDX-License-Identifier: NOASSERTION
#include "InteractionRules.h"
#include "InteractionLayout.h"
namespace Dxf::GameplaySample
{
void FInteractionRules::Collect() noexcept
{
	++m_Collected;
}
void FInteractionRules::SetPlateOccupants(Toolbox::int32 Occupants) noexcept
{
	// 空になった瞬間から保持時間を数え始める。
	if (m_PlateOccupants > 0 && Occupants == 0)
	{
		m_DoorHold = InteractionLayout::DoorHoldSeconds;
	}
	m_PlateOccupants = Occupants;
}
void FInteractionRules::Advance(Toolbox::f64 Seconds) noexcept
{
	if (m_PlateOccupants == 0 && m_DoorHold > 0)
	{
		m_DoorHold = Toolbox::Max(0.0, m_DoorHold - Seconds);
	}
}
void FInteractionRules::CrateContact(bool bBegin) noexcept
{
	m_bTouchingCrate = bBegin;
	if (bBegin)
	{
		++m_CrateBegins;
	}
	else
	{
		++m_CrateEnds;
	}
}
void FInteractionRules::CrateStay() noexcept
{
	++m_CrateStays;
}
void FInteractionRules::ReachCheckpoint(Toolbox::int32 Index) noexcept
{
	if (Index > m_Checkpoint)
	{
		m_Checkpoint = Index;
	}
}
void FInteractionRules::Respawned() noexcept
{
	++m_Respawns;
}
} // namespace Dxf::GameplaySample
