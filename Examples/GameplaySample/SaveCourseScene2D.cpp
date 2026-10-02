// SPDX-License-Identifier: NOASSERTION
#include "SaveCourseScene2D.h"
namespace Dxf::GameplaySample
{
ASaveCourseScene2D::ASaveCourseScene2D(FPreparedScene2D Prepared, FCheckpointDocument2D Checkpoint, FPreparedPrefab2D Cargo, FUserSettings Settings) : ACheckpointScene2D(Toolbox::Move(Prepared), Toolbox::Move(Checkpoint)), m_Cargo(Toolbox::Move(Cargo)), m_Settings(Toolbox::Move(Settings))
{
}
ASaveCourseScene2D::~ASaveCourseScene2D()
{
	Shutdown_Internal();
}
const FSaveCourseProgress& ASaveCourseScene2D::GetProgress() const
{
	if (!m_Controller)
	{
		throw Toolbox::FException("Save course controller not initialized");
	}
	return m_Controller.Get()->GetProgress();
}
void ASaveCourseScene2D::ApplySettings(FUserSettings Settings, const FInputSnapshot& Current)
{
	if (!m_Controller)
	{
		throw Toolbox::FException("Save course controller not initialized");
	}
	m_Controller.Get()->ApplySettings(Settings, Current);
	m_Settings = Toolbox::Move(Settings);
}
bool ASaveCourseScene2D::RequestDoor(Toolbox::uint32 Door, bool bOpen) noexcept
{
	return m_Controller && m_Controller.Get()->RequestDoor(Door, bOpen);
}
void ASaveCourseScene2D::RequestCargo()
{
	if (!m_Controller)
	{
		throw Toolbox::FException("Save course controller not initialized");
	}
	m_Controller.Get()->RequestCargo();
}
TResult<void> ASaveCourseScene2D::OnInitialize(const FInitContext& Context)
{
	FWorldEventSettings Events;
	Events.bEnabled = true;
	Events.MaxPairs = 256;
	GetPhysicsWorld().SetEventSettings(Events);
	// 目標要求を専任Jointの予約より先に出す。実体は既存Collectionが所有する。
	const auto Spawned = Spawn<FController>(*this, m_Cargo, m_Settings);
	if (!Spawned)
	{
		return TResult<void>::Failure(Spawned.Error());
	}
	m_Controller = Spawned.Value();
	return ACheckpointScene2D::OnInitialize(Context);
}
const FInputSnapshot* ASaveCourseScene2D::RouteInput_Internal(const FTickContext& Context)
{
	const auto* Routed = ACheckpointScene2D::RouteInput_Internal(Context);
	if (m_Controller)
	{
		m_Controller.Get()->UpdateInput(Routed != nullptr ? *Routed : Context.Input, Context.Input, Context.Time.bPaused);
	}
	return Routed;
}
void ASaveCourseScene2D::OnCheckpointStepCompleted(const FFixedTickContext& Context)
{
	if (m_Controller)
	{
		m_Controller.Get()->Complete(Context);
	}
}
} // namespace Dxf::GameplaySample
