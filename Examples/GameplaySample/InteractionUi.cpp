// SPDX-License-Identifier: NOASSERTION
#include "InteractionUi.h"
#include "SampleHud.h"
#include "Dxf/UiButton.h"
namespace Dxf::GameplaySample
{
void FInteractionUi::Initialize(DScene& Scene, FAssetService& Assets, Toolbox::TFunction<void()> ToggleSplit,
                                Toolbox::TFunction<void()> ToggleShape, Toolbox::TFunction<void()> TogglePush, Toolbox::TFunction<void(DUiPanel&, FUiScope&)> Populate, bool bContent)
{
	m_pScene = &Scene;
	m_pText = Toolbox::MakeUnique<FUiAssetTextService>(Assets);
	FUiRootSettings Settings;
	Settings.Text = m_pText.Get();
	m_pRoot = Toolbox::MakeUnique<FUiRoot>(Settings);
	m_Status = m_pRoot->Create<DUiLabel>();
	m_Status.Get()->SetAbsolutePosition({16, 40});
	m_Status.Get()->SetWidth(FUiLength::Fixed(1248));
	m_Status.Get()->SetHeight(FUiLength::Fixed(28));
	m_Status.Get()->SetHitTest(EUiHitTest::None);
	RequireSample(m_pRoot->AddToLayer(EUiLayer::Normal, m_Status.Cast<DUiElement>()));
	m_Settings = m_pRoot->Create<DUiPopup>();
	auto Panel = m_pRoot->Create<DUiPanel>(EUiStackMode::Vertical, Populate ? 6.0f : 12.0f);
	Panel.Get()->SetWidth(FUiLength::Fixed(420));
	Panel.Get()->SetHeight(FUiLength::Content());
	Panel.Get()->SetAlign(EUiAlign::Center, EUiAlign::Center);
	Panel.Get()->SetPadding(FUiThickness::All(Populate ? 10.0f : 20.0f));
	Panel.Get()->CreateChild<DUiLabel>("Settings / physics paused");
	auto Split = Panel.Get()->CreateChild<DUiButton>("Toggle 1 / 2 views");
	Split.Get()->SetHeight(FUiLength::Fixed(Populate ? 24.0f : 40.0f));
	auto Shape = Panel.Get()->CreateChild<DUiButton>(bContent ? "Spawn extra instance" : "Character: Round / Capsule");
	Shape.Get()->SetHeight(FUiLength::Fixed(Populate ? 24.0f : 40.0f));
	auto Push = Panel.Get()->CreateChild<DUiButton>(bContent ? "Destroy selected instance" : "Push boxes: On / Off");
	Push.Get()->SetHeight(FUiLength::Fixed(Populate ? 24.0f : 40.0f));
	if (Populate)
	{
		Populate(*Panel.Get(), m_Scope);
	}
	auto Close = Panel.Get()->CreateChild<DUiButton>("Resume / F1");
	Close.Get()->SetHeight(FUiLength::Fixed(Populate ? 24.0f : 40.0f));
	m_Scope.Add(Split.Get()->OnClicked().Subscribe(Toolbox::Move(ToggleSplit)));
	m_Scope.Add(Shape.Get()->OnClicked().Subscribe(Toolbox::Move(ToggleShape)));
	m_Scope.Add(Push.Get()->OnClicked().Subscribe(Toolbox::Move(TogglePush)));
	m_Scope.Add(Close.Get()->OnClicked().Subscribe(
	    [this]()
	    {
		    m_Settings.Get()->Close();
	    }));
	m_Scope.Add(m_Settings.Get()->OnClosed().Subscribe(
	    [this]()
	    {
		    m_pScene->GetClock().SetPaused(m_bWasPaused);
		    m_pRoot->SetFocus({});
	    }));
	if (bContent || Populate)
	{
		Panel.Get()->SetWidth(FUiLength::Fill());
		Panel.Get()->SetAlign(EUiAlign::Stretch, EUiAlign::Start);
		m_ContentScroll = m_pRoot->Create<DUiScrollView>();
		// 従来メニューと同じく、最初の方向キーは先頭Buttonへ送る。
		m_ContentScroll.Get()->SetFocusable(false);
		m_ContentScroll.Get()->SetWidth(FUiLength::Fixed(440));
		m_ContentScroll.Get()->SetHeight(FUiLength::Fixed(static_cast<Toolbox::f32>(m_Height - 80)));
		m_ContentScroll.Get()->SetAlign(EUiAlign::Center, EUiAlign::Center);
		m_ContentScroll.Get()->SetContent(Panel.Cast<DUiElement>());
		m_Settings.Get()->SetContent(m_ContentScroll.Cast<DUiElement>());
	}
	else
	{
		m_Settings.Get()->SetContent(Panel.Cast<DUiElement>());
	}
	m_Display = m_Host.AddScreen(*m_pRoot, m_DisplayOptions);
	Scene.SetInputRouter(this);
}
FInteractionUi::~FInteractionUi()
{
	if (m_pScene != nullptr)
	{
		m_pScene->SetInputRouter(nullptr);
	}
}
void FInteractionUi::SetUserScale(Toolbox::f32 Scale)
{
	if (!Toolbox::IsFinite(Scale) || Scale < .25f || Scale > 8)
	{
		throw Toolbox::FException("Invalid UI user scale");
	}
	if (Scale != m_DisplayOptions.Scale.UserScale)
	{
		m_DisplayOptions.Scale.UserScale = Scale;
		if (!m_Host.SetOptions(m_Display, m_DisplayOptions))
		{
			throw Toolbox::FException("UI display is not available");
		}
	}
}
bool FInteractionUi::IsSettingsOpen() const noexcept
{
	return m_Settings && m_Settings.Get()->IsOpen();
}
void FInteractionUi::SetHudToggleEnabled(bool bEnabled) noexcept
{
	m_bHudToggleEnabled = bEnabled;
}
void FInteractionUi::ToggleSettings_Internal()
{
	if (m_Settings.Get()->IsOpen())
	{
		m_Settings.Get()->Close();
		return;
	}
	m_bWasPaused = m_pScene->GetClock().IsPaused();
	RequireSample(m_Settings.Get()->Open());
	m_pScene->GetClock().SetPaused(true);
}
FInputSnapshot FInteractionUi::RouteInput(const FTickContext& Context)
{
	if (Context.Window.bKnown && Context.Window.RenderWidth > 0 && Context.Window.RenderHeight > 0)
	{
		m_Width = Context.Window.RenderWidth;
		m_Height = Context.Window.RenderHeight;
		if (m_ContentScroll)
		{
			const FUiSurface Surface({0, 0, m_Width, m_Height}, m_DisplayOptions.Scale);
			const auto Size = Surface.GetLogicalSize();
			m_ContentScroll.Get()->SetWidth(FUiLength::Fixed(Toolbox::Min(440.0f, Toolbox::Max(40.0f, Size.Width - 24))));
			m_ContentScroll.Get()->SetHeight(FUiLength::Fixed(Toolbox::Max(40.0f, Size.Height - 40)));
		}
	}
	if (m_bHudToggleEnabled && Context.Input.WasPressed(EKey::H))
	{
		m_bVisible = !m_bVisible;
	}
	// 閉じるフレームも最初にModalとして入力を処理する。同時に新しく押したキーも離すまで消費する。
	const bool bToggle = Context.Input.WasPressed(EKey::F1);
	const bool bClosing = bToggle && m_Settings.Get()->IsOpen();
	if (bToggle && !bClosing)
	{
		ToggleSettings_Internal();
	}
	const FInputSnapshot Routed = m_Host.RouteInput(Context);
	if (bClosing && m_Settings.Get()->IsOpen())
	{
		m_Settings.Get()->Close();
	}
	return Routed;
}
void FInteractionUi::Draw(FRenderContext& Render, const char* Status)
{
	if (!m_bVisible && !m_Settings.Get()->IsOpen())
	{
		return;
	}
	m_Status.Get()->SetText(Status);
	RequireSample(m_Host.Draw(Render));
}
} // namespace Dxf::GameplaySample
