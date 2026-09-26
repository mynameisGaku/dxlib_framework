// SPDX-License-Identifier: NOASSERTION
// P1: 合成の能力の宣言。未対応の乗算済みアルファの合成を、描画を始めずに失敗させる。
#include "Ui/UiRuntimeTestSupport.h"
#include "Dxf/UiPanel.h"
using namespace UiTest;

namespace
{
// 全面の半透明の色。
void AddFill(FUiRoot& Root)
{
	auto Panel = Root.Create<DUiPanel>();
	Panel.Get()->SetWidth(FUiLength::Fill());
	Panel.Get()->SetHeight(FUiLength::Fill());
	FUiStylePatch Style;
	Style.Background = FColor{255, 0, 0, 128};
	Style.BorderWidth = 0.0f;
	Panel.Get()->SetStyleOverride(Style);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Panel.Cast<DUiElement>()));
}

bool Contains(const Toolbox::FString& Text, const char* Part)
{
	const Toolbox::FString Needle(Part);
	for (Toolbox::size_t I = 0; I + Needle.Size() <= Text.Size(); ++I)
	{
		if (Text.Substr(I, Needle.Size()) == Needle)
		{
			return true;
		}
	}
	return false;
}
// 透明なパネルを一つ持つHostを、宣言した能力のBackendで描く。成功ならtrue。
bool DrawTransparentPanel(FRecordingRenderer& Backend, Toolbox::FString* Error)
{
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	FUiRoot Root;
	AddFill(Root);
	FUiSceneHost Host;
	FUiWorldPanel3D Panel;
	Panel.TextureWidth = 32;
	Panel.TextureHeight = 32;
	Panel.Composition = EUiPanelComposition::Transparent;
	const auto Id = Host.AddWorldPanel3D(Root, Panel, Assets, PixelOptions());
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	auto Result = Host.RenderWorldPanelTextures(Renderer.GetContext());
	if (!Result && Error != nullptr)
	{
		*Error = Result.Error().Message;
		// 表示先の番号を失敗に書く。
		REQUIRE(Contains(*Error, (Toolbox::FString("UI display ") + Toolbox::ToString(Id) + " ").CStr()));
	}
	if (Result)
	{
		FRenderView3D View;
		View.Eye = {0, 0.5f, -5};
		REQUIRE(Renderer.GetContext().Get3D().SetView(View));
		Result = Host.DrawWorldPanels3D(Renderer.GetContext(), View);
	}
	REQUIRE(Renderer.EndFrame());
	return static_cast<bool>(Result);
}

} // namespace

TEST("UI transparent panel fails before drawing on backends without the full composition capabilities")
{
	// 通常描画だけ・四角形だけ・中間画像の蓄積だけのBackendは、透明な合成を始めない。
	for (Toolbox::int32 Variant = 0; Variant < 3; ++Variant)
	{
		FRecordingRenderer Backend;
		if (Variant == 1)
		{
			Backend.Declared.bTexturedQuads3D = true;
		}
		if (Variant == 2)
		{
			Backend.Declared.bAlphaTargetClear = true;
		}
		Toolbox::FString Error;
		REQUIRE(!DrawTransparentPanel(Backend, &Error));
		REQUIRE(Contains(Error, "premultiplied-2d-blend") && Contains(Error, "premultiplied-quads-3d"));
		REQUIRE(Contains(Error, "alpha-target-clear") == (Variant != 2));
		// 中間画像の作成・描画先の切替・描画・貼付を一つも行っていない。
		REQUIRE(Backend.CurrentTarget < 0 && Backend.Rectangles.IsEmpty() && Backend.Quads.IsEmpty());
		REQUIRE(Backend.Assets.GetTrace().Textures.IsEmpty());
	}
	// 必要な能力をすべて宣言したBackendでは描く。
	FRecordingRenderer Capable;
	Capable.Declared = TransparentCapabilities();
	REQUIRE(DrawTransparentPanel(Capable, nullptr));
	REQUIRE(Capable.Quads.Size() == 1 && Capable.Quads[0].bPremultipliedAlpha);
}

TEST("Render queue rejects premultiplied commands and alpha clears before any native call")
{
	FRecordingRenderer Backend;
	FRenderSystem Renderer(Backend);
	// 乗算済みの2D命令は、実行の前に失敗し、提示もしない。
	REQUIRE(Renderer.BeginFrame(100, 100, {}));
	FDrawStyle Style;
	Style.Blend = EBlendMode2D::PremultipliedAlpha;
	REQUIRE(Renderer.GetContext().Get2D().FillRectangle({0, 0, 10, 10}));
	REQUIRE(Renderer.GetContext().Get2D().FillRectangle({0, 0, 10, 10}, Style));
	const auto Ended = Renderer.EndFrame();
	REQUIRE(!Ended && Contains(Ended.Error().Message, "premultiplied 2D blend"));
	REQUIRE(Backend.Rectangles.IsEmpty() && Backend.Presents == 0);
	// 透明な消去は、アルファを蓄積できると宣言したBackendだけ。
	REQUIRE(Renderer.BeginFrame(100, 100, {}));
	const auto Cleared = Renderer.GetContext().ClearTarget({0, 0, 0, 0});
	REQUIRE(!Cleared && Contains(Cleared.Error().Message, "alpha render target clear"));
	REQUIRE(!Renderer.EndFrame());
	REQUIRE(Backend.Presents == 0);
	// 乗算済みの3Dの四角形は、四角形を描けるだけでは通さない。
	REQUIRE(Renderer.BeginFrame(100, 100, {}));
	FRenderView3D View;
	REQUIRE(Renderer.GetContext().Get3D().SetView(View));
	FTexturedQuad3D Quad;
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	auto Target = Assets.CreateRenderTarget(8, 8, true);
	REQUIRE(Target);
	Quad.Texture = Target.Value().AsTexture();
	Quad.Corners = {Toolbox::FVector3{-1, 1, 0}, Toolbox::FVector3{1, 1, 0}, Toolbox::FVector3{1, -1, 0},
	                Toolbox::FVector3{-1, -1, 0}};
	Quad.bPremultipliedAlpha = true;
	REQUIRE(Renderer.GetContext().Get3D().DrawTexturedQuad(Quad));
	const auto Ended3D = Renderer.EndFrame();
	REQUIRE(!Ended3D && Contains(Ended3D.Error().Message, "premultiplied textured quad"));
	REQUIRE(Backend.Quads.IsEmpty() && Backend.Presents == 0);
	// 失敗の後も次のフレームは受け付け、通常の描画は提示する。
	REQUIRE(Renderer.BeginFrame(100, 100, {}));
	REQUIRE(Renderer.GetContext().Get2D().FillRectangle({0, 0, 10, 10}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.Rectangles.Size() == 1 && Backend.Presents == 1);
	// 宣言をそろえたBackendは、同じ命令を描く。
	Backend.Declared = TransparentCapabilities();
	REQUIRE(Renderer.BeginFrame(100, 100, {}));
	REQUIRE(Renderer.GetContext().Get2D().FillRectangle({0, 0, 10, 10}, Style));
	REQUIRE(Renderer.GetContext().ClearTarget({0, 0, 0, 0}));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Renderer.GetContext().GetCapabilities() == TransparentCapabilities());
}
