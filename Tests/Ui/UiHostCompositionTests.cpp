// SPDX-License-Identifier: NOASSERTION
// W3: 3Dのパネルの透明な合成（乗算済みアルファの中間画像）と、不透明・透明のパネルの描く順。
#include "Ui/UiRuntimeTestSupport.h"
#include "Dxf/UiAssetTextService.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiPanel.h"
using namespace UiTest;

namespace
{
// 色の部品を全面に置く。
void AddFill(FUiRoot& Root, FColor Color)
{
	auto Panel = Root.Create<DUiPanel>();
	Panel.Get()->SetWidth(FUiLength::Fill());
	Panel.Get()->SetHeight(FUiLength::Fill());
	FUiStylePatch Style;
	Style.Background = Color;
	Style.BorderWidth = 0.0f;
	Panel.Get()->SetStyleOverride(Style);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Panel.Cast<DUiElement>()));
}

// z平面上の1x1のパネル。
FUiWorldPanel3D PanelAt(Toolbox::f32 X, Toolbox::f32 Z, EUiPanelComposition Composition)
{
	FUiWorldPanel3D Panel;
	Panel.TopLeft = {X, 1, Z};
	Panel.TopRight = {X + 1, 1, Z};
	Panel.BottomLeft = {X, 0, Z};
	Panel.TextureWidth = 32;
	Panel.TextureHeight = 32;
	Panel.Composition = Composition;
	return Panel;
}
} // namespace

TEST("UI transparent panel draws premultiplied content and composites far to near after opaque panels")
{
	FRecordingRenderer Backend;
	Backend.Declared = TransparentCapabilities();
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	FUiRoot Near;
	FUiRoot Far;
	FUiRoot Solid;
	AddFill(Near, {0, 0, 255, 96});
	AddFill(Far, {255, 220, 0, 128});
	AddFill(Solid, {10, 20, 30, 255});
	FUiSceneHost Host;
	// 手前の透明なパネルを先に、不透明なパネルを最後に登録する。
	Host.AddWorldPanel3D(Near, PanelAt(0, -2, EUiPanelComposition::Transparent), Assets, PixelOptions());
	Host.AddWorldPanel3D(Far, PanelAt(0, 0, EUiPanelComposition::Transparent), Assets, PixelOptions());
	FUiWorldPanel3D Opaque = PanelAt(3, 1, EUiPanelComposition::Opaque);
	Opaque.Background = {0, 0, 0, 0};
	const auto OpaqueId = Host.AddWorldPanel3D(Solid, Opaque, Assets, PixelOptions());
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	// 不透明なパネルの背景は不透明に限る（透明な合成は背景を使わない）。
	REQUIRE(!Host.RenderWorldPanelTextures(Renderer.GetContext()));
	Opaque.Background = {0, 0, 0, 255};
	REQUIRE(Host.SetWorldPanel3D(OpaqueId, Opaque));
	Backend.Rectangles.Clear();
	REQUIRE(Host.RenderWorldPanelTextures(Renderer.GetContext()));
	REQUIRE(Renderer.Flush());
	// 透明な表示面の内容は乗算済みの合成で、不透明な表示面は通常の合成で描く。
	Toolbox::int32 Premultiplied = 0;
	Toolbox::int32 Straight = 0;
	for (const auto& Rectangle : Backend.Rectangles)
	{
		if (Rectangle.Options.Blend == EBlendMode2D::PremultipliedAlpha)
		{
			++Premultiplied;
		}
		else
		{
			++Straight;
		}
	}
	REQUIRE(Premultiplied == 2 && Straight == 1);
	REQUIRE(!Host.GetWorldPanelTexture(OpaqueId).AsTexture().IsPremultipliedAlpha());
	FRenderView3D View;
	View.Eye = {0, 0, -10};
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Host.DrawWorldPanels3D(Renderer.GetContext(), View));
	REQUIRE(Renderer.EndFrame());
	// 不透明→奥の透明→手前の透明の順。透明なパネルだけが乗算済みの合成で貼られる。
	REQUIRE(Backend.Quads.Size() == 3);
	REQUIRE(Backend.Quads[0].Corners[0].X == 3 && !Backend.Quads[0].bPremultipliedAlpha);
	REQUIRE(Backend.Quads[1].Corners[0].Z == 0 && Backend.Quads[1].bPremultipliedAlpha);
	REQUIRE(Backend.Quads[2].Corners[0].Z == -2 && Backend.Quads[2].bPremultipliedAlpha);
}

TEST("UI panel switching composition recreates its texture with the matching alpha")
{
	FRecordingRenderer Backend;
	Backend.Declared = TransparentCapabilities();
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	FUiRoot Root;
	AddFill(Root, {255, 0, 0, 128});
	FUiSceneHost Host;
	FUiWorldPanel3D Panel = PanelAt(0, 0, EUiPanelComposition::Opaque);
	const auto Id = Host.AddWorldPanel3D(Root, Panel, Assets, PixelOptions());
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	REQUIRE(Host.RenderWorldPanelTextures(Renderer.GetContext()));
	const auto First = Host.GetWorldPanelTexture(Id).AsTexture().GetNativeHandle_Internal();
	Panel.Composition = EUiPanelComposition::Transparent;
	REQUIRE(Host.SetWorldPanel3D(Id, Panel));
	REQUIRE(Host.RenderWorldPanelTextures(Renderer.GetContext()));
	const auto Second = Host.GetWorldPanelTexture(Id).AsTexture().GetNativeHandle_Internal();
	REQUIRE(First != Second);
	REQUIRE(Host.GetSurface(Id).IsPremultipliedAlpha());
	REQUIRE(Renderer.EndFrame());
}

TEST("UI queued text keeps the accepted string after SetText and destroy before the queue runs")
{
	FRecordingRenderer Backend;
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend);
	FUiAssetTextService Text(Assets);
	FUiRootSettings Settings;
	Settings.Text = &Text;
	FUiRoot Root(Settings);
	auto Label = Root.Create<DUiLabel>("accepted");
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Label.Cast<DUiElement>()));
	auto Other = Root.Create<DUiLabel>("removed");
	REQUIRE(Root.AddToLayer(EUiLayer::Panel, Other.Cast<DUiElement>()));
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	REQUIRE(Host.Draw(Renderer.GetContext()));
	// 受付の後、実行の前に文字を変え、別の要素を破棄する。
	Label.Get()->SetText("changed");
	Root.Destroy(Other.Cast<DUiElement>());
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.Texts.Size() == 2 && Backend.Texts[0] == "accepted" && Backend.Texts[1] == "removed");
	// 次のフレームは新しい文字を描く（共有の文字列を古い値のまま再利用しない）。
	Backend.Texts.Clear();
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	REQUIRE(Host.Draw(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.Texts.Size() == 1 && Backend.Texts[0] == "changed");
}

TEST("UI host submits every rectangle with the clip of its clipping ancestor in pixels")
{
	FRecordingRenderer Backend;
	FUiRoot Root;
	auto Frame = Root.Create<DUiPanel>(EUiStackMode::Overlay);
	Frame.Get()->SetAbsolutePosition({40, 30});
	Frame.Get()->SetWidth(FUiLength::Fixed(100));
	Frame.Get()->SetHeight(FUiLength::Fixed(50));
	Frame.Get()->SetClipChildren(true);
	REQUIRE(Root.AddToLayer(EUiLayer::Panel, Frame.Cast<DUiElement>()));
	auto Wide = Root.Create<DUiPanel>();
	Wide.Get()->SetWidth(FUiLength::Fixed(400));
	Wide.Get()->SetHeight(FUiLength::Fixed(20));
	FUiStylePatch Style;
	Style.Background = FColor{200, 10, 10, 255};
	Style.BorderWidth = 0.0f;
	Wide.Get()->SetStyleOverride(Style);
	REQUIRE(Root.AddChild(Frame.Cast<DUiElement>(), Wide.Cast<DUiElement>()));
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	REQUIRE(Host.Draw(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	bool bFound = false;
	for (const auto& Rectangle : Backend.Rectangles)
	{
		// 全命令が切り抜く。幅400の子は親の矩形(40,30)-(140,80)で切り抜かれる。
		REQUIRE(Rectangle.Options.bClip);
		if (Rectangle.Options.Color.R == 200)
		{
			bFound = true;
			REQUIRE(Rectangle.Options.ClipRect.Left == 40 && Rectangle.Options.ClipRect.Top == 30 &&
			        Rectangle.Options.ClipRect.Right == 140 && Rectangle.Options.ClipRect.Bottom == 80);
		}
	}
	REQUIRE(bFound);
}

namespace
{
// 画面(100,50)〜(260,130)に映る2Dのパネル（1単位20画素、Y下向き）。
FUiWorldPanel2D OffscreenPanel(EUiPanelComposition Composition)
{
	FUiWorldPanel2D Panel;
	Panel.WorldTopLeft = {5, 2.5f};
	Panel.WorldSize = {8, 4};
	Panel.Transform.Origin = {0, 0};
	Panel.Transform.PixelsPerUnit = 20;
	Panel.Transform.bYUp = false;
	Panel.bOffscreen = true;
	Panel.Composition = Composition;
	return Panel;
}
} // namespace

TEST("UI 2D panel can draw through an offscreen texture and blit it with its layer clip and composition")
{
	for (bool bTransparent : {false, true})
	{
		FRecordingRenderer Backend;
		// 2Dの透明な合成は、3Dの四角形の能力を要求しない。
		Backend.Declared.bPremultipliedBlend2D = true;
		Backend.Declared.bAlphaTargetClear = true;
		FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
		FUiRoot Root;
		auto Button = FullButton(Root);
		FUiStylePatch Style;
		Style.Background = FColor{200, 20, 20, 128};
		Style.BorderWidth = 0.0f;
		Button.Get()->SetStyleOverride(Style);
		Toolbox::int32 Clicks = 0;
		FUiScope Scope;
		Scope.Add(Button.Get()->OnClicked().Subscribe(
		    [&Clicks]()
		    {
			    ++Clicks;
		    }));
		FUiSceneHost Host;
		FUiWorldPanel2D Panel =
		    OffscreenPanel(bTransparent ? EUiPanelComposition::Transparent : EUiPanelComposition::Opaque);
		Panel.ScreenClip = {0, 0, 200, 720};
		FUiDisplayOptions Options = PixelOptions();
		Options.Layer = 700;
		const auto Id = Host.AddWorldPanel2D(Root, Panel, Assets, Options);
		FRenderSystem Renderer(Backend);
		REQUIRE(Renderer.BeginFrame(800, 600, {}));
		REQUIRE(Host.Draw(Renderer.GetContext()));
		// 呼出し前の描画先（画面）へ戻っている。
		REQUIRE(Backend.CurrentTarget < 0);
		REQUIRE(Renderer.EndFrame());
		// 中間画像の内容は表示面の合成で描き、画面へは一枚の画像として等倍で貼る。
		REQUIRE(!Backend.Rectangles.IsEmpty());
		for (const auto& Rectangle : Backend.Rectangles)
		{
			REQUIRE((Rectangle.Options.Blend == EBlendMode2D::PremultipliedAlpha) == bTransparent);
			REQUIRE(Rectangle.Rectangle.Left >= 0 && Rectangle.Rectangle.Right <= 160);
		}
		REQUIRE(Backend.Sprites.Size() == 1);
		const auto& Sprite = Backend.Sprites[0];
		REQUIRE(Sprite.Position.X == 100 && Sprite.Position.Y == 50 && Sprite.Options.Scale.X == 1);
		REQUIRE(Sprite.Options.Layer == 700 && Sprite.Options.bClip);
		REQUIRE(Sprite.Options.ClipRect.Left == 100 && Sprite.Options.ClipRect.Right == 200 &&
		        Sprite.Options.ClipRect.Top == 50 && Sprite.Options.ClipRect.Bottom == 130);
		REQUIRE((Sprite.Options.Blend == EBlendMode2D::PremultipliedAlpha) == bTransparent);
		REQUIRE(Sprite.Texture.GetWidth() == 160 && Sprite.Texture.GetHeight() == 80);
		REQUIRE(Host.GetSurface(Id).GetPixelRect().Left == 0 &&
		        Host.GetSurface(Id).IsPremultipliedAlpha() == bTransparent);
		// 画面の点は中間画像の原点へ移して選ぶ。切り抜きの外は選ばない。
		FHostInput Input;
		Input.Raw.MouseX = 150;
		Input.Raw.MouseY = 90;
		(void)Input.Send(Host);
		Input.Raw.MouseButtons[0] = true;
		(void)Input.Send(Host);
		Input.Raw.MouseButtons[0] = false;
		(void)Input.Send(Host);
		REQUIRE(Clicks == 1);
		Input.Raw.MouseX = 230;
		(void)Input.Send(Host);
		Input.Raw.MouseButtons[0] = true;
		(void)Input.Send(Host);
		Input.Raw.MouseButtons[0] = false;
		(void)Input.Send(Host);
		REQUIRE(Clicks == 1);
	}
}

TEST("UI offscreen 2D panel checks its own requirements and skips empty rectangles")
{
	FRecordingRenderer Backend;
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	FUiRoot Root;
	FullButton(Root);
	FUiSceneHost Host;
	const auto Id =
	    Host.AddWorldPanel2D(Root, OffscreenPanel(EUiPanelComposition::Transparent), Assets, PixelOptions());
	FRenderSystem Renderer(Backend);
	// 2Dの合成の能力だけが不足として書かれる。
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	const auto Missing = Host.Draw(Renderer.GetContext());
	REQUIRE(!Missing);
	REQUIRE(Missing.Error().Message ==
	        Toolbox::FString("UI display ") + Toolbox::ToString(Id) +
	            " requests transparent composition; backend lacks: premultiplied-2d-blend alpha-target-clear");
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.Sprites.IsEmpty() && Backend.Assets.GetTrace().Textures.IsEmpty());
	// 不透明な中間画像の背景は不透明に限る。
	FUiWorldPanel2D Opaque = OffscreenPanel(EUiPanelComposition::Opaque);
	Opaque.Background = {0, 0, 0, 10};
	REQUIRE(Host.SetWorldPanel2D(Id, Opaque));
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	REQUIRE(!Host.Draw(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	// 画面の範囲が空なら、0x0の中間画像を作らず何も描かない。
	FUiWorldPanel2D Empty = OffscreenPanel(EUiPanelComposition::Opaque);
	Empty.WorldSize = {0, 0};
	REQUIRE(Host.SetWorldPanel2D(Id, Empty));
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	REQUIRE(Host.Draw(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.Sprites.IsEmpty() && Backend.Assets.GetTrace().Textures.IsEmpty());
}

TEST("UI same root drawn as 2D offscreen and 3D transparent panels keeps separate textures and one input pass")
{
	FRecordingRenderer Backend;
	Backend.Declared = TransparentCapabilities();
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	FUiRoot Root;
	AddFill(Root, {10, 200, 30, 128});
	FUiSceneHost Host;
	const auto Id2D =
	    Host.AddWorldPanel2D(Root, OffscreenPanel(EUiPanelComposition::Transparent), Assets, PixelOptions());
	const auto Id3D =
	    Host.AddWorldPanel3D(Root, PanelAt(0, 0, EUiPanelComposition::Transparent), Assets, PixelOptions());
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	REQUIRE(Host.RenderWorldPanelTextures(Renderer.GetContext()));
	FRenderView3D View;
	View.Eye = {0, 0, -10};
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	REQUIRE(Host.DrawWorldPanels3D(Renderer.GetContext(), View));
	REQUIRE(Host.Draw(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	// 表示先ごとの寸法の中間画像（2Dは画面の範囲160x80、3Dは指定の32x32）。
	REQUIRE(Host.GetWorldPanelTexture(Id3D).GetWidth() == 32);
	REQUIRE(Backend.Sprites.Size() == 1 && Backend.Sprites[0].Texture.GetWidth() == 160);
	REQUIRE(Backend.Quads.Size() == 1 && Backend.Quads[0].bPremultipliedAlpha);
	REQUIRE(Host.GetSurface(Id2D).IsPremultipliedAlpha() && Host.GetSurface(Id3D).IsPremultipliedAlpha());
	// 二つの表示先でも、同じルートの入力と時間は1回だけ進む。
	FHostInput Input;
	(void)Input.Send(Host);
	REQUIRE(Host.GetLastRouting().ProcessedRoots == 1);
}
