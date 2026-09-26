// SPDX-License-Identifier: NOASSERTION
#include "UiSampleShell.h"
#include "UiTitleScreen.h"
#include "UiPausePanel.h"
#include "UiSettingsPanel.h"
#include "UiConfirmPanel.h"
#include "UiPlayerHud.h"
#include "UiItemBrowser.h"
#include "UiWorldControls.h"
#include "UiSampleViews.h"
#include "UiSampleScenes.h"
#include "Dxf/SceneNavigator.h"
#include "Dxf/AudioPlayer.h"
#include "Dxf/ViewCoordinates.h"
namespace Dxf::UiSample
{
namespace
{
void Require_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
} // namespace
FUiSampleShell::FUiSampleShell(Toolbox::TSharedPtr<FUiSampleState> State) : m_pState(Toolbox::Move(State))
{
}

TResult<void> FUiSampleShell::Initialize(DScene& Scene, FAssetService& Assets, bool bTitle, bool b3D,
                                         Toolbox::TFunction<Toolbox::FString()> ReadStatus)
{
	m_pScene = &Scene;
	m_pAssets = &Assets;
	m_bTitle = bTitle;
	m_b3D = b3D;
	m_ReadStatus = Toolbox::Move(ReadStatus);
	m_pText = Toolbox::MakeUnique<FUiAssetTextService>(Assets);
	FUiRootSettings Settings;
	Settings.Text = m_pText.Get();
	m_pRoot = Toolbox::MakeUnique<FUiRoot>(Settings);
	m_pViewHud = Toolbox::MakeUnique<FUiRoot>(Settings);
	m_pWorldRoot = Toolbox::MakeUnique<FUiRoot>(Settings);
	m_Styles.Attach(*m_pRoot);
	m_Styles.Attach(*m_pViewHud);
	m_Styles.Attach(*m_pWorldRoot);
	m_StylePath = Assets.GetProjectRoot() / Toolbox::FPath("Build/UiStyles.bundle.dxfui");
	// 集約ファイルがなくても、組込みの既定スタイルだけで起動できる。
	m_Pause = m_pRoot->Create<DUiPopup>();
	m_Pause.Get()->SetName("PausePopup");
	m_Pause.Get()->SetContent(m_pRoot->Create<DUiPausePanel>(m_pState).Cast<DUiElement>());
	m_Settings = m_pRoot->Create<DUiPopup>();
	m_Settings.Get()->SetName("SettingsPopup");
	m_Settings.Get()->SetContent(m_pRoot->Create<DUiSettingsPanel>(m_pState).Cast<DUiElement>());
	m_Confirm = m_pRoot->Create<DUiPopup>();
	m_Confirm.Get()->SetName("ConfirmPopup");
	m_Confirm.Get()->SetContent(m_pRoot->Create<DUiConfirmPanel>(m_pState).Cast<DUiElement>());
	m_Scope.Add(m_Pause.Get()->OnClosed().Subscribe(
	    [this]()
	    {
		    m_bPaused = false;
		    m_pScene->GetClock().SetPaused(m_Settings.Get()->IsOpen());
		    m_pRoot->SetFocus({});
	    }));
	m_Scope.Add(m_Settings.Get()->OnClosed().Subscribe(
	    [this]()
	    {
		    m_pScene->GetClock().SetPaused(m_bPaused);
		    if (!m_bPaused)
		    {
			    m_pRoot->SetFocus({});
		    }
	    }));
	const auto State = m_pState;
	m_Scope.Add(m_pRoot->OnCancelRequested().Subscribe(
	    [State, bTitle]()
	    {
		    State->Action = bTitle ? EUiSampleAction::Quit : EUiSampleAction::Pause;
	    }));
	if (bTitle)
	{
		Require_Internal(
		    m_pRoot->AddToLayer(EUiLayer::Normal, m_pRoot->Create<DUiTitleScreen>(State).Cast<DUiElement>()));
	}
	else
	{
		Require_Internal(
		    m_pRoot->AddToLayer(EUiLayer::Normal, m_pRoot->Create<DUiPlayerHud>(State).Cast<DUiElement>()));
		Require_Internal(
		    m_pRoot->AddToLayer(EUiLayer::Normal, m_pRoot->Create<DUiItemBrowser>(State).Cast<DUiElement>()));
		Require_Internal(m_pViewHud->AddToLayer(EUiLayer::Normal,
		                                        m_pViewHud->Create<DUiPlayerHud>(State, false).Cast<DUiElement>()));
		Require_Internal(m_pWorldRoot->AddToLayer(EUiLayer::Normal,
		                                          m_pWorldRoot->Create<DUiWorldControls>(State).Cast<DUiElement>()));
		for (Toolbox::size_t I = 0; I < 2; ++I)
		{
			m_Markers[I] = m_pRoot->Create<DUiLabel>("Player");
			m_Markers[I].Get()->SetWidth(FUiLength::Fixed(70));
			m_Markers[I].Get()->SetHeight(FUiLength::Fixed(28));
			m_Markers[I].Get()->SetHitTest(EUiHitTest::None);
			Require_Internal(m_pRoot->AddToLayer(EUiLayer::Panel, m_Markers[I].Cast<DUiElement>()));
		}
	}
	auto Sound = Assets.LoadSound("Assets/confirm.wav");
	if (!Sound)
	{
		return TResult<void>::Failure(Sound.Error());
	}
	m_Sound = Sound.Value();
	FUiDisplayOptions Options;
	Options.Navigation = EUiNavigationPolicy::Always;
	Options.Layer = 3000;
	m_Screen = m_Host.AddScreen(*m_pRoot, Options);
	RefreshDisplays();
	Scene.SetInputRouter(this);
	return {};
}

void FUiSampleShell::RefreshDisplays()
{
	const bool Split = m_pState->Split.Get();
	const auto Scale = m_pState->Scale.Get();
	if (m_LastScale == Scale && m_bLastSplit == Split)
	{
		return;
	}
	m_LastScale = Scale;
	m_bLastSplit = Split;
	FUiDisplayOptions Screen;
	Screen.Navigation = EUiNavigationPolicy::Always;
	Screen.Scale.UserScale = static_cast<Toolbox::f32>(Scale);
	Screen.Layer = 3000;
	m_Host.SetOptions(m_Screen, Screen);
	for (const auto Id : m_ChangingDisplays)
	{
		m_Host.Remove(Id);
	}
	m_ChangingDisplays.Clear();
	if (m_bTitle)
	{
		return;
	}
	FUiDisplayOptions Hud;
	Hud.Navigation = EUiNavigationPolicy::Never;
	Hud.Scale.UserScale = static_cast<Toolbox::f32>(Scale);
	Hud.Layer = 1000;
	for (Toolbox::int32 I = 0; I < (Split ? 2 : 1); ++I)
	{
		const FUiPixelRect Area =
		    Split ? FUiPixelRect{I * 640, 650, (I + 1) * 640, 720} : FUiPixelRect{0, 650, 1280, 720};
		Hud.Scale.Mode = EUiScaleMode::FixedPixel;
		m_ChangingDisplays.PushBack(m_Host.AddViewport(*m_pViewHud, Area, Hud));
		if (!m_b3D)
		{
			FUiWorldPanel2D Panel;
			Panel.WorldTopLeft = {3, 5};
			Panel.WorldSize = {4, 2};
			Panel.Transform = MakeSampleTransform2D(Split, I);
			Panel.ScreenClip = Split ? FUiPixelRect{I * 640, 0, (I + 1) * 640, 720} : FUiPixelRect{0, 0, 1280, 720};
			FUiDisplayOptions Options;
			Options.Scale.ReferenceHeight = 240;
			Options.Layer = 0;
			m_ChangingDisplays.PushBack(m_Host.AddWorldPanel2D(*m_pWorldRoot, Panel, Options));
		}
	}
	if (m_b3D)
	{
		FUiWorldPanel3D Panel;
		Panel.TopLeft = {-3, 4, 0};
		Panel.TopRight = {1, 4, 0};
		Panel.BottomLeft = {-3, 2, 0};
		FUiDisplayOptions Options;
		Options.Scale.ReferenceHeight = 240;
		m_ChangingDisplays.PushBack(m_Host.AddWorldPanel3D(*m_pWorldRoot, Panel, *m_pAssets, Options));
	}
}

void FUiSampleShell::ExecuteAction(const FTickContext& Context)
{
	const auto Action = m_pState->Action;
	m_pState->Action = EUiSampleAction::None;
	switch (Action)
	{
	case EUiSampleAction::Pause:
		if (m_bPaused)
		{
			m_Pause.Get()->Close();
		}

		else if (!m_bTitle)
		{
			Require_Internal(m_Pause.Get()->Open());
			m_bPaused = true;
			m_pScene->GetClock().SetPaused(true);
		}
		break;
	case EUiSampleAction::Settings:
		Require_Internal(m_Settings.Get()->Open());
		m_pScene->GetClock().SetPaused(true);
		break;
	case EUiSampleAction::CloseSettings:
		m_Settings.Get()->Close();
		break;
	case EUiSampleAction::ConfirmTitle:
		// 一時停止の上に重ねて開く（いいえで一時停止へ戻る）。
		Require_Internal(m_Confirm.Get()->Open());
		break;
	case EUiSampleAction::CancelConfirm:
		m_Confirm.Get()->Close();
		break;
	case EUiSampleAction::PlaySound:
		if (Context.Audio != nullptr)
		{
			FPlaybackOptions Options;
			Options.Volume = static_cast<Toolbox::f32>(m_pState->Volume.Get());
			Options.Scope = Context.AudioScope;
			auto Played = Context.Audio->Play(m_Sound, Options);
			if (!Played)
			{
				throw Toolbox::FException(Played.Error().Message);
			}
		}
		break;
	case EUiSampleAction::ReloadStyle:
	{
		auto Reloaded = m_Styles.ReloadFile(m_StylePath);
		m_pState->Notice.Set(Reloaded ? Toolbox::FString("スタイルを再読込しました") : Reloaded.Error().Message);
		break;
	}
	case EUiSampleAction::Start2D:
	case EUiSampleAction::Start3D:
		if (Context.Scenes != nullptr)
		{
			Require_Internal(
			    Context.Scenes->RequestChange(MakePlayScene(Action == EUiSampleAction::Start3D, m_pState)));
		}
		break;
	case EUiSampleAction::Title:
		if (Context.Scenes != nullptr)
		{
			Require_Internal(Context.Scenes->RequestChange(MakeTitleScene(m_pState)));
		}
		break;
	case EUiSampleAction::Quit:
		if (Context.Scenes != nullptr)
		{
			Context.Scenes->RequestQuit();
		}
		break;
	default:
		break;
	}
}

FInputSnapshot FUiSampleShell::RouteInput(const FTickContext& Context)
{
	RefreshDisplays();
	if (m_b3D)
	{
		m_Host.SetWorldViews3D(MakeSampleViews(m_pState->Split.Get()));
	}
	if (m_ReadStatus)
	{
		m_pState->Status.Set(m_ReadStatus());
	}
	const auto Routed = m_Host.RouteInput(Context);
	ExecuteAction(Context);
	RefreshDisplays();
	return Routed;
}

TResult<void> FUiSampleShell::PreparePanels(FRenderContext& Render)
{
	return m_Host.RenderWorldPanelTextures(Render);
}

TResult<void> FUiSampleShell::DrawPanels(FRenderContext& Render, const FRenderView3D& View)
{
	return m_Host.DrawWorldPanels3D(Render, View);
}

TResult<void> FUiSampleShell::Draw(FRenderContext& Render)
{
	return m_Host.Draw(Render);
}

void FUiSampleShell::SetWorldMarker(Toolbox::FVector3 Center)
{
	const bool Split = m_pState->Split.Get();
	const auto Views = MakeSampleViews(Split);
	const auto Surface = m_Host.GetSurface(m_Screen);
	for (Toolbox::size_t I = 0; I < 2; ++I)
	{
		auto* Marker = m_Markers[I].Get();
		if (Marker == nullptr)
		{
			continue;
		}
		if (I > 0 && !Split)
		{
			Marker->SetVisibility(EUiVisibility::Collapsed);
			continue;
		}
		FVector2 Screen;
		if (m_b3D)
		{
			auto P = ProjectWorldToScreen(Views[I], 1280, 720, Center);
			if (!P || !P.Value().bInsideView)
			{
				Marker->SetVisibility(EUiVisibility::Collapsed);
				continue;
			}
			Screen = P.Value().Screen;
		}
		else
		{
			Screen = MakeSampleTransform2D(Split, static_cast<Toolbox::int32>(I)).ToScreen({Center.X, Center.Y});
		}
		Marker->SetVisibility(EUiVisibility::Visible);
		Marker->SetAbsolutePosition(Surface.ToLogical({Screen.X, Screen.Y - 40}));
	}
}
} // namespace Dxf::UiSample
