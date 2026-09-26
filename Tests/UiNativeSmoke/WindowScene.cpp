// SPDX-License-Identifier: NOASSERTION
#include "WindowScene.h"
#include "Dxf/UiImage.h"
#include "Dxf/UiLabel.h"
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
} // namespace

TResult<void> DWindowScene::OnInitialize(const FInitContext& Context)
{
	m_pText = Toolbox::MakeUnique<FUiAssetTextService>(Context.Assets);
	FUiRootSettings Settings;
	Settings.Text = m_pText.Get();
	m_pRoot = Toolbox::MakeUnique<FUiRoot>(Settings);
	// 描画先の全面に追従するボタン（寸法が変わっても端まで塗る）。
	auto Button = m_pRoot->Create<DUiButton>("");
	Button.Get()->SetWidth(FUiLength::Fill());
	Button.Get()->SetHeight(FUiLength::Fill());
	FUiStylePatch Style;
	Style.Background = ButtonColor;
	Style.BorderWidth = 0.0f;
	Button.Get()->SetStyleOverride(Style);
	if (auto Added = m_pRoot->AddToLayer(EUiLayer::Normal, Button.Cast<DUiElement>()); !Added)
	{
		return Added;
	}
	m_Click = Button.Get()->OnClicked().Subscribe(
	    [this]()
	    {
		    ++Clicks;
	    });
	// 画面モードの変更の前に読んだ画像と、文字。
	auto Texture = Context.Assets.LoadTexture("Assets/player.bmp");
	if (!Texture)
	{
		return TResult<void>::Failure(Texture.Error());
	}
	auto Image = m_pRoot->Create<DUiImage>(Texture.Value());
	Image.Get()->SetFit(EUiImageFit::Stretch);
	Image.Get()->SetAbsolutePosition(
	    {static_cast<Toolbox::f32>(ImageRect.Left), static_cast<Toolbox::f32>(ImageRect.Top)});
	Image.Get()->SetWidth(FUiLength::Fixed(static_cast<Toolbox::f32>(ImageRect.Width())));
	Image.Get()->SetHeight(FUiLength::Fixed(static_cast<Toolbox::f32>(ImageRect.Height())));
	Image.Get()->SetHitTest(EUiHitTest::None);
	if (auto Added = m_pRoot->AddToLayer(EUiLayer::Panel, Image.Cast<DUiElement>()); !Added)
	{
		return Added;
	}
	auto Label = m_pRoot->Create<DUiLabel>("拡縮");
	Label.Get()->SetAbsolutePosition({100, 20});
	Label.Get()->SetWidth(FUiLength::Fixed(200));
	Label.Get()->SetHeight(FUiLength::Fixed(40));
	Label.Get()->SetHitTest(EUiHitTest::None);
	FUiStylePatch Text;
	Text.Foreground = FColor{255, 255, 255, 255};
	Text.FontSize = 28;
	Label.Get()->SetStyleOverride(Text);
	if (auto Added = m_pRoot->AddToLayer(EUiLayer::Panel, Label.Cast<DUiElement>()); !Added)
	{
		return Added;
	}
	FUiDisplayOptions Options;
	Options.Scale.Mode = EUiScaleMode::FixedPixel;
	m_Host.AddScreen(*m_pRoot, Options);
	m_Host.AttachTo(*this);
	return {};
}

void DWindowScene::OnTick(const FTickContext& Context)
{
	++Ticks;
	Window = Context.Window;
}

void DWindowScene::OnDraw(FRenderContext& Render) const
{
	++Draws;
	Require_Internal(m_Host.Draw(Render));
}
} // namespace Dxf::UiSmoke
