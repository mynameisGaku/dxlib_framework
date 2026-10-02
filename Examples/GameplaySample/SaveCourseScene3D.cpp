// SPDX-License-Identifier: NOASSERTION
#include "SaveCourseScene3D.h"
namespace Dxf::GameplaySample
{
ASaveCourseScene3D::ASaveCourseScene3D(FPreparedScene3D Prepared, FCheckpointDocument3D Checkpoint, FPreparedPrefab3D Cargo, FUserSettings Settings, ASaveCourseGameInstance* SaveOwner) : ACheckpointScene3D(Toolbox::Move(Prepared), Toolbox::Move(Checkpoint)), m_Cargo(Toolbox::Move(Cargo)), m_Settings(Toolbox::Move(Settings)), m_pSaveOwner(SaveOwner)
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
void ASaveCourseScene3D::RequestDestroyCargo()
{
	if (m_Controller)
	{
		m_Controller.Get()->RequestDestroyCargo();
	}
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
	const auto Built = ACheckpointScene3D::OnInitialize(Context);
	if (!Built)
	{
		return Built;
	}
	if (m_pSaveOwner != nullptr)
	{
		m_pPanel = Toolbox::MakeUnique<FSaveCoursePanel>(*this, *m_pSaveOwner, Context.Assets, 3, [this]()
		                                                 {
			                                                 RequestCargo();
		                                                 },
		                                                 [this]()
		                                                 {
			                                                 RequestDestroyCargo();
		                                                 },
		                                                 [this](Toolbox::uint32 Door, bool Open)
		                                                 {
			                                                 return RequestDoor(Door, Open);
		                                                 });
	}
	return {};
}
const FInputSnapshot* ASaveCourseScene3D::RouteInput_Internal(const FTickContext& Context)
{
	const auto* Routed = ACheckpointScene3D::RouteInput_Internal(Context);
	if (m_pPanel && (m_Width != m_pPanel->GetWidth() || m_Height != m_pPanel->GetHeight() || m_bSplit != m_pPanel->IsSplit()))
	{
		m_Width = m_pPanel->GetWidth();
		m_Height = m_pPanel->GetHeight();
		m_bSplit = m_pPanel->IsSplit();
		auto First = GetDefinition().Views[0];
		auto Second = GetDefinition().Views[1];
		First.bViewport = m_bSplit;
		First.Viewport = {0, 0, m_bSplit ? m_Width / 2 : m_Width, m_Height};
		Second.bViewport = true;
		Second.Viewport = {m_Width / 2, 0, m_Width, m_Height};
		SetViews(First, m_bSplit ? Toolbox::TOptional<FRenderView3D>{Second} : Toolbox::TOptional<FRenderView3D>{});
	}
	if (m_Controller)
	{
		m_Controller.Get()->UpdateInput(Routed != nullptr ? *Routed : Context.Input, Context.Input, Context.Time.bPaused || !m_bInputArmed);
		m_bInputArmed = true;
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
void ASaveCourseScene3D::OnDraw(FRenderContext& Render) const
{
	ACheckpointScene3D::OnDraw(Render);
	if (m_pPanel)
	{
		m_pPanel->Draw(Render);
	}
}
} // namespace Dxf::GameplaySample
