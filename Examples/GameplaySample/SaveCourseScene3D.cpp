// SPDX-License-Identifier: NOASSERTION
#include "SaveCourseScene3D.h"
namespace Dxf::GameplaySample
{
ASaveCourseScene3D::ASaveCourseScene3D(FPreparedScene3D Prepared, FCheckpointDocument3D Checkpoint, FPreparedPrefab3D Cargo, FUserSettings Settings) : ACheckpointScene3D(Toolbox::Move(Prepared), Toolbox::Move(Checkpoint)), m_Cargo(Toolbox::Move(Cargo)), m_Settings(Toolbox::Move(Settings))
{
}
ASaveCourseScene3D::~ASaveCourseScene3D()
{
	Shutdown_Internal();
}
const FSaveCourseProgress& ASaveCourseScene3D::GetProgress() const
{
	if (!m_Controller)
	{
		throw Toolbox::FException("Save course controller not initialized");
	}
	return m_Controller.Get()->GetProgress();
}
void ASaveCourseScene3D::ApplySettings(FUserSettings Settings, const FInputSnapshot& Current)
{
	if (!m_Controller)
	{
		throw Toolbox::FException("Save course controller not initialized");
	}
	m_Controller.Get()->ApplySettings(Settings, Current);
	m_Settings = Toolbox::Move(Settings);
}
bool ASaveCourseScene3D::RequestDoor(Toolbox::uint32 Door, bool bOpen) noexcept
{
	return m_Controller && m_Controller.Get()->RequestDoor(Door, bOpen);
}
void ASaveCourseScene3D::RequestCargo()
{
	if (!m_Controller)
	{
		throw Toolbox::FException("Save course controller not initialized");
	}
	m_Controller.Get()->RequestCargo();
}
TResult<void> ASaveCourseScene3D::OnInitialize(const FInitContext& Context)
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
	return ACheckpointScene3D::OnInitialize(Context);
}
const FInputSnapshot* ASaveCourseScene3D::RouteInput_Internal(const FTickContext& Context)
{
	const auto* Routed = ACheckpointScene3D::RouteInput_Internal(Context);
	if (m_Controller)
	{
		m_Controller.Get()->UpdateInput(Routed != nullptr ? *Routed : Context.Input, Context.Input, Context.Time.bPaused);
	}
	return Routed;
}
void ASaveCourseScene3D::OnCheckpointStepCompleted(const FFixedTickContext& Context)
{
	if (m_Controller)
	{
		m_Controller.Get()->Complete(Context);
	}
}
} // namespace Dxf::GameplaySample
