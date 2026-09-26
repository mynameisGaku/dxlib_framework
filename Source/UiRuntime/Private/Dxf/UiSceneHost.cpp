// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiSceneHost.h"
#include "UiHostState.h"
namespace Dxf
{
FUiSceneHost::FUiSceneHost(FUiSceneHostSettings Settings)
    : m_pState(Toolbox::MakeShared<Detail::FUiHostState>(Toolbox::Move(Settings)))
{
}
FUiSceneHost::~FUiSceneHost()
{
	DetachFromScene();
	const auto State = m_pState;
	State->Shutdown();
}

void FUiSceneHost::AttachTo(DScene& Scene)
{
	DetachFromScene();
	Scene.SetInputRouter(this);
	m_pConnection = Scene.GetInputRouteSlot_Internal();
}

void FUiSceneHost::DetachFromScene() noexcept
{
	const auto Slot = m_pConnection.Lock();
	m_pConnection = {};
	if (Slot)
	{
		const auto Router = Slot->Router.Lock();
		if (Router && Router->Router == this)
		{
			Slot->Router = {};
		}
	}
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
FUiDisplayId FUiSceneHost::AddScreen(FUiRoot& Root, const FUiDisplayOptions& Options)
{
	const auto State = m_pState;
	return State->AddScreen(Root, Options);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
FUiDisplayId FUiSceneHost::AddViewport(FUiRoot& Root, FUiPixelRect Rect, const FUiDisplayOptions& Options)
{
	const auto State = m_pState;
	return State->AddViewport(Root, Rect, Options);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
FUiDisplayId FUiSceneHost::AddWorldPanel2D(FUiRoot& Root, const FUiWorldPanel2D& Panel, const FUiDisplayOptions& Options)
{
	const auto State = m_pState;
	return State->AddWorldPanel2D(Root, Panel, Options);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
FUiDisplayId FUiSceneHost::AddWorldPanel3D(FUiRoot& Root, const FUiWorldPanel3D& Panel, FAssetService& Assets,
                                           const FUiDisplayOptions& Options)
{
	const auto State = m_pState;
	return State->AddWorldPanel3D(Root, Panel, Assets, Options);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
bool FUiSceneHost::Remove(FUiDisplayId Id) noexcept
{
	const auto State = m_pState;
	return State->Remove(Id);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
bool FUiSceneHost::SetViewportRect(FUiDisplayId Id, FUiPixelRect Rect) noexcept
{
	const auto State = m_pState;
	return State->SetViewportRect(Id, Rect);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
bool FUiSceneHost::SetWorldPanel2D(FUiDisplayId Id, const FUiWorldPanel2D& Panel) noexcept
{
	const auto State = m_pState;
	return State->SetWorldPanel2D(Id, Panel);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
bool FUiSceneHost::SetWorldPanel3D(FUiDisplayId Id, const FUiWorldPanel3D& Panel) noexcept
{
	const auto State = m_pState;
	return State->SetWorldPanel3D(Id, Panel);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
bool FUiSceneHost::SetOptions(FUiDisplayId Id, const FUiDisplayOptions& Options) noexcept
{
	const auto State = m_pState;
	return State->SetOptions(Id, Options);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
void FUiSceneHost::SetPlayerDevices(Toolbox::int32 Player, FUiPlayerDevices Devices) noexcept
{
	const auto State = m_pState;
	State->SetPlayerDevices(Player, Devices);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
void FUiSceneHost::SetNavigationBindings(FUiNavigationBindings Bindings)
{
	const auto State = m_pState;
	State->SetNavigationBindings(Toolbox::Move(Bindings));
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
void FUiSceneHost::SetWorldViews3D(const Toolbox::TVector<FRenderView3D>& Views)
{
	const auto State = m_pState;
	State->SetWorldViews3D(Views);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
void FUiSceneHost::SetScreenSize(Toolbox::int32 Width, Toolbox::int32 Height) noexcept
{
	const auto State = m_pState;
	State->SetScreenSize(Width, Height);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
FInputSnapshot FUiSceneHost::RouteInput(const FTickContext& Context)
{
	const auto State = m_pState;
	return State->RouteInput(Context);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
TResult<void> FUiSceneHost::RenderWorldPanelTextures(FRenderContext& Render)
{
	const auto State = m_pState;
	return State->RenderWorldPanelTextures(Render);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
TResult<void> FUiSceneHost::DrawWorldPanels3D(FRenderContext& Render, const FRenderView3D& View)
{
	const auto State = m_pState;
	return State->DrawWorldPanels3D(Render, View);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
TResult<void> FUiSceneHost::Draw(FRenderContext& Render)
{
	const auto State = m_pState;
	return State->Draw(Render);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
FUiRoutingResult FUiSceneHost::GetLastRouting() const noexcept
{
	const auto State = m_pState;
	return State->GetLastRouting();
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
Toolbox::size_t FUiSceneHost::GetDisplayCount() const noexcept
{
	const auto State = m_pState;
	return State->GetDisplayCount();
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
FRenderTarget FUiSceneHost::GetWorldPanelTexture(FUiDisplayId Id) const noexcept
{
	const auto State = m_pState;
	return State->GetWorldPanelTexture(Id);
}
// 独立した状態を保持して委譲する。通知中にHostが破棄されてもthisへ戻らない。
FUiSurface FUiSceneHost::GetSurface(FUiDisplayId Id) const noexcept
{
	const auto State = m_pState;
	return State->GetSurface(Id);
}
}
// namespace Dxf
