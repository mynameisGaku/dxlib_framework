// SPDX-License-Identifier: NOASSERTION
#include "DisplayScene.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiPanel.h"
namespace Dxf::UiSmoke
{
namespace
{
// 描画の失敗は試験の失敗にする。
void Require_Internal(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}

// 画素と論理単位が等しい表示の設定。
FUiDisplayOptions PixelOptions_Internal(Toolbox::f32 UserScale, Toolbox::int32 Layer)
{
	FUiDisplayOptions Options;
	Options.Scale.Mode = EUiScaleMode::FixedPixel;
	Options.Scale.UserScale = UserScale;
	Options.Layer = Layer;
	return Options;
}
} // namespace

FRenderView3D DDisplayScene::MakeView() noexcept
{
	FRenderView3D View;
	View.Id = 7;
	View.Eye = {0, 0, -10};
	View.Target = {0, 0, 0};
	return View;
}

FUiWorldPanel2D DDisplayScene::MakePanel2D() noexcept
{
	FUiWorldPanel2D Panel;
	Panel.WorldTopLeft = {1, 3.5f};
	Panel.WorldSize = {4, 1.5f};
	Panel.Transform.Origin = {700, 160};
	Panel.Transform.PixelsPerUnit = 40;
	Panel.Transform.bYUp = true;
	Panel.ScreenClip = {0, 0, 1280, 720};
	return Panel;
}

FUiWorldPanel3D DDisplayScene::MakeOpaquePanel() noexcept
{
	FUiWorldPanel3D Panel;
	Panel.TopLeft = {-8, 3, 0};
	Panel.TopRight = {-4, 3, 0};
	Panel.BottomLeft = {-8, 0, 0};
	Panel.TextureWidth = 256;
	Panel.TextureHeight = 192;
	return Panel;
}

FUiWorldPanel3D DDisplayScene::MakeFarPanel() noexcept
{
	FUiWorldPanel3D Panel;
	Panel.TopLeft = {0, 3, 0};
	Panel.TopRight = {8, 3, 0};
	Panel.BottomLeft = {0, 0, 0};
	Panel.TextureWidth = 512;
	Panel.TextureHeight = 192;
	Panel.Composition = EUiPanelComposition::Transparent;
	return Panel;
}

FUiWorldPanel3D DDisplayScene::MakeNearPanel() noexcept
{
	FUiWorldPanel3D Panel;
	Panel.TopLeft = {4, 1, -2};
	Panel.TopRight = {7, 1, -2};
	Panel.BottomLeft = {4, -1.5f, -2};
	Panel.TextureWidth = 192;
	Panel.TextureHeight = 160;
	Panel.Composition = EUiPanelComposition::Transparent;
	return Panel;
}

Toolbox::FOBB DDisplayScene::MakeFrontBox() noexcept
{
	return Toolbox::FOBB{{-5, 0.5f, -2}, {0.6f, 0.6f, 0.6f}};
}

Toolbox::FOBB DDisplayScene::MakeLateBox() noexcept
{
	return Toolbox::FOBB{{1.25f, 2.5f, 2}, {0.75f, 0.3f, 0.3f}};
}

TResult<void> DDisplayScene::OnInitialize(const FInitContext& Context)
{
	m_pText = Toolbox::MakeUnique<FUiAssetTextService>(Context.Assets);
	m_pOverlay = MakeRoot_Internal();
	m_pLeft = MakeRoot_Internal();
	m_pRight = MakeRoot_Internal();
	m_pPanel2D = MakeRoot_Internal();
	m_pOpaque = MakeRoot_Internal();
	m_pFar = MakeRoot_Internal();
	m_pNear = MakeRoot_Internal();
	// 全画面：右上・Viewportの境界をまたぐ位置・3Dのパネルの上に目印を置く（入力は受けない）。
	const Toolbox::TArray<FUiRect, 3> Marks{FUiRect{1180, 20, 80, 40}, FUiRect{600, 680, 100, 30},
	                                        FUiRect{200, 250, 40, 40}};
	for (const FUiRect& Rect : Marks)
	{
		if (auto Added = AddPatch_Internal(*m_pOverlay, Rect, OverlayColor); !Added)
		{
			return Added;
		}
	}
	// 左右：全面のボタンと、論理(10,10)の目印。右は倍率2なので画素では2倍の位置と寸法になる。
	const Toolbox::TArray<FUiRoot*, 2> Sides{m_pLeft.Get(), m_pRight.Get()};
	for (Toolbox::size_t Index = 0; Index < 2; ++Index)
	{
		const bool bLeft = Index == 0;
		if (auto Added = AddButton_Internal(*Sides[Index], bLeft ? LeftColor : RightColor,
		                                    bLeft ? EDisplayButton::Left : EDisplayButton::Right);
		    !Added)
		{
			return Added;
		}
		if (auto Added = AddPatch_Internal(*Sides[Index], {10, 10, 20, 20}, MarkerColor); !Added)
		{
			return Added;
		}
	}
	if (auto Added = AddButton_Internal(*m_pPanel2D, Panel2DColor, EDisplayButton::Panel2D); !Added)
	{
		return Added;
	}
	if (auto Added = AddButton_Internal(*m_pOpaque, OpaqueColor, EDisplayButton::Panel3D); !Added)
	{
		return Added;
	}
	// 奥の透明なパネル：v=64〜160が半透明、160〜192が不透明、上の透明な行に白い文字。
	if (auto Added = AddPatch_Internal(*m_pFar, {0, 64, 512, 96}, FarSemiColor); !Added)
	{
		return Added;
	}
	if (auto Added = AddPatch_Internal(*m_pFar, {0, 160, 512, 32}, FarOpaqueColor); !Added)
	{
		return Added;
	}
	auto Label = m_pFar->Create<DUiLabel>("透過UI");
	Label.Get()->SetAbsolutePosition({272, 6});
	Label.Get()->SetWidth(FUiLength::Fixed(230));
	Label.Get()->SetHeight(FUiLength::Fixed(52));
	Label.Get()->SetHitTest(EUiHitTest::None);
	FUiStylePatch TextStyle;
	TextStyle.Foreground = FarTextColor;
	TextStyle.FontSize = 40;
	Label.Get()->SetStyleOverride(TextStyle);
	if (auto Added = m_pFar->AddToLayer(EUiLayer::Panel, Label.Cast<DUiElement>()); !Added)
	{
		return Added;
	}
	if (auto Added = AddPatch_Internal(*m_pNear, {0, 0, 192, 160}, NearColor); !Added)
	{
		return Added;
	}
	// 表示先。手前のパネルを先に登録し、奥から順に描く並べ替えを確かめる。
	m_Host.AddScreen(*m_pOverlay, PixelOptions_Internal(1, 5000));
	m_Host.AddViewport(*m_pLeft, LeftRect, PixelOptions_Internal(1, 1000));
	m_Host.AddViewport(*m_pRight, RightRect, PixelOptions_Internal(2, 1000));
	m_Host.AddWorldPanel2D(*m_pPanel2D, MakePanel2D(), PixelOptions_Internal(1, 0));
	m_Host.AddWorldPanel3D(*m_pOpaque, MakeOpaquePanel(), Context.Assets, PixelOptions_Internal(1, 0));
	m_Host.AddWorldPanel3D(*m_pNear, MakeNearPanel(), Context.Assets, PixelOptions_Internal(1, 0));
	m_Host.AddWorldPanel3D(*m_pFar, MakeFarPanel(), Context.Assets, PixelOptions_Internal(1, 0));
	if (m_Host.GetDisplayCount() != 7)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "display scene registration");
	}
	Toolbox::TVector<FRenderView3D> Views;
	Views.PushBack(MakeView());
	m_Host.SetWorldViews3D(Views);
	m_Host.AttachTo(*this);
	return {};
}

void DDisplayScene::OnDraw(FRenderContext& Render) const
{
	// 中間画像を先に描き、描画先を画面へ戻す。
	Require_Internal(m_Host.RenderWorldPanelTextures(Render));
	const FRenderView3D View = MakeView();
	Require_Internal(Render.Get3D().SetView(View));
	FDrawStyle3D Style;
	// 奥の二色の箱（透明なパネルの背景が場所で異なるようにする）。
	Style.Color = FColor{200, 40, 40, 255};
	Require_Internal(Render.Get3D().DrawBox(Toolbox::FOBB{{2, 1.5f, 3}, {1.5f, 1.5f, 0.5f}}, Style));
	Style.Color = FColor{40, 80, 220, 255};
	Require_Internal(Render.Get3D().DrawBox(Toolbox::FOBB{{6, 1.5f, 3}, {1.5f, 1.5f, 0.5f}}, Style));
	Style.Color = FColor{60, 60, 240, 255};
	Require_Internal(Render.Get3D().DrawBox(MakeFrontBox(), Style));
	if (bDrawPanels3D)
	{
		Require_Internal(m_Host.DrawWorldPanels3D(Render, View));
	}
	// パネルの後に描く箱。透明な画素が深度を書いていなければ見える。
	Style.Color = FColor{240, 160, 40, 255};
	Require_Internal(Render.Get3D().DrawBox(MakeLateBox(), Style));
	// 2DのUIは3Dの後に描く。
	Require_Internal(m_Host.Draw(Render));
}

TResult<void> DDisplayScene::AddButton_Internal(FUiRoot& Root, FColor Color, EDisplayButton Button)
{
	auto Ref = Root.Create<DUiButton>("");
	Ref.Get()->SetWidth(FUiLength::Fill());
	Ref.Get()->SetHeight(FUiLength::Fill());
	FUiStylePatch Style;
	Style.Background = Color;
	Style.BorderWidth = 0.0f;
	Ref.Get()->SetStyleOverride(Style);
	auto Added = Root.AddToLayer(EUiLayer::Normal, Ref.Cast<DUiElement>());
	if (!Added)
	{
		return Added;
	}
	const auto Index = static_cast<Toolbox::size_t>(Button);
	m_Subscriptions.PushBack(Ref.Get()->OnClicked().Subscribe(
	    [this, Index]()
	    {
		    ++m_Clicks[Index];
	    }));
	return {};
}

TResult<void> DDisplayScene::AddPatch_Internal(FUiRoot& Root, FUiRect Rect, FColor Color)
{
	auto Ref = Root.Create<DUiPanel>(EUiStackMode::Overlay);
	Ref.Get()->SetAbsolutePosition({Rect.X, Rect.Y});
	Ref.Get()->SetWidth(FUiLength::Fixed(Rect.Width));
	Ref.Get()->SetHeight(FUiLength::Fixed(Rect.Height));
	FUiStylePatch Style;
	Style.Background = Color;
	Style.BorderWidth = 0.0f;
	Ref.Get()->SetStyleOverride(Style);
	Ref.Get()->SetHitTest(EUiHitTest::None);
	return Root.AddToLayer(EUiLayer::Panel, Ref.Cast<DUiElement>());
}

Toolbox::TUniquePtr<FUiRoot> DDisplayScene::MakeRoot_Internal() const
{
	FUiRootSettings Settings;
	Settings.Text = m_pText.Get();
	return Toolbox::MakeUnique<FUiRoot>(Settings);
}
} // namespace Dxf::UiSmoke
