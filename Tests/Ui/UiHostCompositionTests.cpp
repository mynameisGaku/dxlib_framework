// SPDX-License-Identifier: NOASSERTION
// W3: 3Dのパネルの透明な合成（乗算済みアルファの中間画像）と、不透明・透明のパネルの描く順。
#include "Ui/UiRuntimeTestSupport.h"
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
