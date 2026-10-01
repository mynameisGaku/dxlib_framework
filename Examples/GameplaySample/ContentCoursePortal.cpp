// SPDX-License-Identifier: NOASSERTION
#include "ContentCoursePortal.h"
#include "ContentCourseScene2D.h"
#include "ContentCourseScene3D.h"
#include "Dxf/AssetService.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiRoot.h"
#include "Dxf/SceneNavigator.h"
namespace Dxf::GameplaySample
{
DContentCoursePortal::DContentCoursePortal(bool b3D) : m_b3D(b3D)
{
	SetTickWhenPaused(true);
}
void DContentCoursePortal::RequestLoad(Toolbox::FString Path)
{
	m_Path = Toolbox::Move(Path);
	m_bSubmitted = false;
	m_bCancel = false;
}
void DContentCoursePortal::RequestCancel() noexcept
{
	m_bCancel = true;
}
ESceneContentRequestState DContentCoursePortal::GetRequestState() const noexcept
{
	return m_Ticket.GetState();
}
TResult<void> DContentCoursePortal::OnInitialize(const FInitContext& Context)
{
	m_pAssets = &Context.Assets;
	return {};
}
void DContentCoursePortal::OnTick(const FTickContext& Context)
{
	// F2は既存コースからの入口。Modalが消費した入力は届かない。
	if (Context.Input.WasPressed(EKey::F2))
	{
		RequestLoad(m_b3D ? "Assets/Content/course3d.dxfscene.json" : "Assets/Content/course2d.dxfscene.json");
	}
	if (!m_Path.IsEmpty())
	{
		if (!m_pRequest)
		{
			m_pRequest = Toolbox::MakeUnique<FSceneContentRequest>(FSceneContentSource(m_pAssets->GetProjectRoot()), Context.Tasks, Context.TaskScope);
		}
		m_bRequested3D = m_b3D;
		m_Ticket = m_bRequested3D ? m_pRequest->LoadScene3D(m_Path) : m_pRequest->LoadScene2D(m_Path);
		m_Path.Clear();
		m_LastState = ESceneContentRequestState::Superseded;
	}
	if (!m_pRequest)
	{
		return;
	}
	if (m_bCancel)
	{
		m_pRequest->Cancel();
		m_bCancel = false;
	}
	m_pRequest->Poll(*m_pAssets);
	const auto State = m_Ticket.GetState();
	if (State != m_LastState && m_Status)
	{
		if (State == ESceneContentRequestState::Failed)
		{
			const auto Error = m_Ticket.GetDiagnostic();
			m_Status.Get()->SetText(Error.Path + ": " + Error.Reason);
		}
		else
		{
			const char* Names[] = {"Pending", "CPU preparing / owner assets", "Ready", "Failed", "Canceled",
			                       "Superseded"};
			m_Status.Get()->SetText(Names[static_cast<Toolbox::uint32>(State)]);
		}
	}
	m_LastState = State;
	if (State == ESceneContentRequestState::Ready && !m_bSubmitted && Context.Scenes)
	{
		const auto Requested = m_bRequested3D
		                           ? Context.Scenes->RequestChange<DContentCourse3DScene>(m_pRequest->GetPrepared3D())
		                           : Context.Scenes->RequestChange<DContentCourse2DScene>(m_pRequest->GetPrepared2D());
		if (!Requested)
		{
			throw Toolbox::FException(Requested.Error().Message);
		}
		m_bSubmitted = true;
	}
}
void DContentCoursePortal::OnDeinitialize() noexcept
{
	if (m_pRequest)
	{
		(void)m_pRequest->Retire();
	}
	m_pRequest.Reset();
	m_pAssets = nullptr;
}
void DContentCoursePortal::BuildPanel(DUiPanel& Panel, FUiScope& Scope, TObjectHandle<DContentCoursePortal> Self)
{
	auto Dimension = Panel.CreateChild<DUiButton>(m_b3D ? "Next scene: 3D (change)" : "Next scene: 2D (change)");
	Dimension.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Dimension.Get()->OnClicked().Subscribe(
	    [Self, Dimension]()
	    {
		    if (Self)
		    {
			    Self.Get()->m_b3D = !Self.Get()->m_b3D;
			    Dimension.Get()->SetText(Self.Get()->m_b3D ? "Next scene: 3D (change)" : "Next scene: 2D (change)");
		    }
	    }));
	auto Load = Panel.CreateChild<DUiButton>("Load / reload data course (F2)");
	Load.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Load.Get()->OnClicked().Subscribe(
	    [Self]()
	    {
		    if (Self)
		    {
			    Self.Get()->RequestLoad(Self.Get()->m_b3D ? "Assets/Content/course3d.dxfscene.json" : "Assets/Content/course2d.dxfscene.json");
		    }
	    }));
	auto Bad = Panel.CreateChild<DUiButton>("Try broken data (old scene retained)");
	Bad.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Bad.Get()->OnClicked().Subscribe(
	    [Self]()
	    {
		    if (Self)
		    {
			    Self.Get()->RequestLoad("Assets/Content/broken.dxfscene.json");
		    }
	    }));
	auto Cancel = Panel.CreateChild<DUiButton>("Cancel preparation");
	Cancel.Get()->SetHeight(FUiLength::Fixed(20));
	Scope.Add(Cancel.Get()->OnClicked().Subscribe(
	    [Self]()
	    {
		    if (Self)
		    {
			    Self.Get()->RequestCancel();
		    }
	    }));
	m_Status = Panel.CreateChild<DUiLabel>("No data request");
	m_Status.Get()->SetHeight(FUiLength::Fixed(20));
}
} // namespace Dxf::GameplaySample
