// SPDX-License-Identifier: NOASSERTION
#include "InteractionUi.h"
#include "SampleHud.h"
#include "Dxf/UiButton.h"
namespace Dxf::GameplaySample
{
void FInteractionUi::Initialize(DScene& Scene, FAssetService& Assets, Toolbox::TFunction<void()> ToggleSplit,
                                Toolbox::TFunction<void()> ToggleShape, Toolbox::TFunction<void()> TogglePush)
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
	auto Panel = m_pRoot->Create<DUiPanel>(EUiStackMode::Vertical, 12.0f);
	Panel.Get()->SetWidth(FUiLength::Fixed(420));
	Panel.Get()->SetHeight(FUiLength::Content());
	Panel.Get()->SetAlign(EUiAlign::Center, EUiAlign::Center);
	Panel.Get()->SetPadding(FUiThickness::All(20));
	Panel.Get()->CreateChild<DUiLabel>("Settings / physics paused");
	auto Split = Panel.Get()->CreateChild<DUiButton>("Toggle 1 / 2 views");
	Split.Get()->SetHeight(FUiLength::Fixed(40));
	auto Shape = Panel.Get()->CreateChild<DUiButton>("Character: Round / Capsule");
	Shape.Get()->SetHeight(FUiLength::Fixed(40));
	auto Push = Panel.Get()->CreateChild<DUiButton>("Push boxes: On / Off");
	Push.Get()->SetHeight(FUiLength::Fixed(40));
	auto Close = Panel.Get()->CreateChild<DUiButton>("Resume / F1");
	Close.Get()->SetHeight(FUiLength::Fixed(40));
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
	m_Settings.Get()->SetContent(Panel.Cast<DUiElement>());
	m_Host.AddScreen(*m_pRoot);
	Scene.SetInputRouter(this);
}
FInteractionUi::~FInteractionUi()
{
	if (m_pScene != nullptr)
	{
		m_pScene->SetInputRouter(nullptr);
	}
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
	}
	if (Context.Input.WasPressed(EKey::H))
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
