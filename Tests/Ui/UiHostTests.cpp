// SPDX-License-Identifier: NOASSERTION
#include "UiRuntimeTestSupport.h"
#include "Toolbox/UniquePtr.h"
#include "Dxf/UiRenderer.h"
#include "Dxf/UiSlider.h"
using namespace Dxf;
using namespace UiTest;
TEST("UI Host maps resized viewport before input and masks mouse press")
{
	FUiRoot Root;
	auto Button = FullButton(Root);
	Toolbox::int32 Clicks = 0;
	auto Sub = Button.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    ++Clicks;
	    });
	FUiSceneHost Host;
	const auto Display = Host.AddViewport(Root, {320, 0, 640, 240}, PixelOptions());
	FHostInput Input;
	Input.Raw.MouseX = 400;
	Input.Raw.MouseY = 100;
	Input.Raw.MouseButtons[0] = true;
	const auto Down = Input.Send(Host);
	REQUIRE(!Down.IsMouseDown(EMouseButton::Left));
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Clicks == 1);
	REQUIRE(Host.SetViewportRect(Display, {640, 0, 960, 240}));
	Input.Raw.MouseButtons[0] = true;
	REQUIRE(Input.Send(Host).IsMouseDown(EMouseButton::Left));
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Clicks == 1);
	Input.Raw.MouseX = 800;
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Clicks == 2);
}

TEST("UI Host draws shared Root at both viewports but processes it once")
{
	FUiRoot Root;
	auto Panel = Root.Create<DUiPanel>();
	FUiStylePatch Style;
	Style.Background = FColor{190, 20, 30, 255};
	Panel.Get()->SetStyleOverride(Style);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Panel.Cast<DUiElement>()));
	FUiSceneHost Host;
	Host.AddViewport(Root, {0, 0, 320, 240}, PixelOptions());
	Host.AddViewport(Root, {320, 0, 641, 481}, PixelOptions());
	FHostInput Input;
	(void)Input.Send(Host);
	REQUIRE(Host.GetLastRouting().ProcessedRoots == 1);
	FRecordingRenderer Backend;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(641, 481, {}));
	REQUIRE(Host.Draw(Renderer.GetContext()));
	REQUIRE(Renderer.EndFrame());
	bool Left = false;
	bool Right = false;
	for (const auto& Rect : Backend.Rectangles)
	{
		if (Rect.Options.Color.R != 190)
		{
			continue;
		}
		Left = Left || (Rect.Rectangle.Left == 0 && Rect.Rectangle.Right == 320 && Rect.Rectangle.Bottom == 240);
		Right = Right || (Rect.Rectangle.Left == 320 && Rect.Rectangle.Right == 641 && Rect.Rectangle.Bottom == 481);
	}
	REQUIRE(Left && Right);
}

TEST("UI Host survives display reallocation removal and Root deletion in callback")
{
	auto Root = Toolbox::MakeUnique<FUiRoot>();
	FUiRoot Other;
	auto Button = FullButton(*Root);
	FUiSceneHost Host;
	const auto Id = Host.AddScreen(*Root, PixelOptions());
	auto Sub = Button.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    for (Toolbox::int32 I = 0; I < 100; ++I)
		    {
			    Host.AddViewport(Other, {0, 0, 20, 20}, PixelOptions());
		    }
		    REQUIRE(Host.Remove(Id));
		    Root.Reset();
	    });
	FHostInput Input;
	Input.Raw.MouseX = 100;
	Input.Raw.MouseY = 100;
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(!Root);
	REQUIRE(Host.GetDisplayCount() == 100);
	(void)Input.Send(Host);
}

TEST("UI Host may die in an event and Scene route automatically expires")
{
	DScene Scene;
	FUiRoot Root;
	auto Button = FullButton(Root);
	auto Host = Toolbox::MakeUnique<FUiSceneHost>();
	Host->AttachTo(Scene);
	Host->AddScreen(Root, PixelOptions());
	auto Sub = Button.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    Host.Reset();
	    });
	FHostInput Input;
	Input.Raw.MouseX = 20;
	Input.Raw.MouseY = 20;
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(*Host);
	Input.Raw.MouseButtons[0] = false;
	const auto Result = Input.Send(*Host);
	REQUIRE(!Host);
	REQUIRE(Scene.GetInputRouter() == nullptr);
	REQUIRE(!Result.IsMouseDown(EMouseButton::Left));
	{
		FUiSceneHost Longer;
		auto ShortScene = Toolbox::MakeUnique<DScene>();
		Longer.AttachTo(*ShortScene);
		ShortScene.Reset();
		Longer.DetachFromScene();
	}
}

TEST("UI Host claims modal close key and stick until physical release")
{
	FUiRoot Root;
	auto Popup = Root.Create<DUiPopup>();
	REQUIRE(Popup.Get()->Open());
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FHostInput Input;
	Input.Raw.Pads[0].bConnected = true;
	Input.Raw.Pads[0].LeftX = 1;
	REQUIRE(Input.Send(Host).GetRaw().Pads[0].LeftX == 0);
	Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::Escape)] = true;
	REQUIRE(!Input.Send(Host).IsDown(EKey::Escape));
	REQUIRE(!Popup.Get()->IsOpen());
	const auto Still = Input.Send(Host);
	REQUIRE(!Still.IsDown(EKey::Escape));
	REQUIRE(Still.GetRaw().Pads[0].LeftX == 0);
	Input.Raw.Pads[0].LeftX = 0;
	Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::Escape)] = false;
	(void)Input.Send(Host);
	Input.Raw.Pads[0].LeftX = 1;
	Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::Escape)] = true;
	const auto Fresh = Input.Send(Host);
	REQUIRE(Fresh.WasPressed(EKey::Escape));
	REQUIRE(Fresh.GetRaw().Pads[0].LeftX == 1);
}

TEST("UI Host chooses nearest world panel independently of registration order")
{
	FRecordingRenderer Backend;
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	FUiRoot Near;
	FUiRoot Far;
	auto A = FullButton(Near);
	auto B = FullButton(Far);
	Toolbox::int32 Selected = 0;
	auto SA = A.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    Selected = 1;
	    });
	auto SB = B.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    Selected = 2;
	    });
	FUiSceneHost Host;
	FUiWorldPanel3D Panel;
	Host.AddWorldPanel3D(Near, Panel, Assets, PixelOptions());
	Panel.TopLeft.Z = 2;
	Panel.TopRight.Z = 2;
	Panel.BottomLeft.Z = 2;
	Host.AddWorldPanel3D(Far, Panel, Assets, PixelOptions());
	FRenderView3D View;
	View.Eye = {0, 0.5f, -5};
	View.Target = {0, 0.5f, 0};
	Host.SetWorldViews3D({View});
	FHostInput Input;
	Input.Raw.MouseX = 640;
	Input.Raw.MouseY = 360;
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Selected == 1);
	// 同じIdでも、異なる矩形の指定ビューは両方を保持する。
	View.bViewport = true;
	View.Viewport = {0, 0, 640, 720};
	FRenderView3D Second = View;
	Second.Viewport = {640, 0, 1280, 720};
	Host.SetWorldViews3D({View, Second});
	Input.Raw.MouseX = 960;
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Selected == 1);
}

TEST("UI world texture returns to caller target and quad uses explicit view")
{
	FRecordingRenderer Backend;
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	FUiRoot Root;
	FullButton(Root);
	FUiSceneHost Host;
	Host.AddWorldPanel3D(Root, {}, Assets, PixelOptions());
	const auto Target = Assets.CreateRenderTarget(700, 500, true);
	REQUIRE(Target);
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(800, 600, {}));
	REQUIRE(Renderer.GetContext().SetRenderTarget(Target.Value()));
	REQUIRE(Host.RenderWorldPanelTextures(Renderer.GetContext()));
	REQUIRE(Backend.CurrentTarget == Target.Value().AsTexture().GetNativeHandle_Internal());
	REQUIRE(Renderer.GetContext().GetTargetWidth() == 700);
	FRenderView3D View;
	View.Eye = {0, 1, -7};
	REQUIRE(Host.DrawWorldPanels3D(Renderer.GetContext(), View));
	REQUIRE(Renderer.EndFrame());
	REQUIRE(Backend.Views.Size() == 1 && Backend.Views[0].Eye.Z == -7);
	REQUIRE(Backend.Quads.Size() == 1);
}

TEST("UI clip resets after throwing setter draw and cleanup preserving first failure")
{
	for (Toolbox::int32 Mode = 0; Mode < 3; ++Mode)
	{
		FRecordingRenderer Backend;
		FRenderSystem Renderer(Backend);
		REQUIRE(Renderer.BeginFrame(100, 100, {}));
		Backend.ThrowSet = Mode == 0;
		Backend.ThrowDraw = Mode == 1;
		Backend.FailDraw = Mode == 2;
		Backend.ThrowReset = true;
		FDrawStyle Style;
		Style.bClip = true;
		Style.ClipRect = {10, 10, 50, 50};
		REQUIRE(Renderer.GetContext().Get2D().FillRectangle({0, 0, 80, 80}, Style));
		const auto Result = Renderer.EndFrame();
		REQUIRE(!Result);
		REQUIRE(
		    Result.Error().Message ==
		    (Mode == 0 ? "primary clip exception" : (Mode == 1 ? "primary draw exception" : "primary draw failure")));
		REQUIRE(Backend.ClipResets >= 1 && !Backend.Clip && Backend.Presents == 0);
		Backend.ThrowSet = false;
		Backend.ThrowDraw = false;
		Backend.FailDraw = false;
		Backend.ThrowReset = false;
		REQUIRE(Renderer.BeginFrame(100, 100, {}));
		REQUIRE(Renderer.GetContext().Get2D().FillRectangle({0, 0, 80, 80}));
		REQUIRE(Renderer.EndFrame());
	}
}

TEST("UI panel maps skew basis and finite front back and outside bounds")
{
	FUiWorldPanel3D Panel;
	Panel.TopLeft = {0, 1, 0};
	Panel.TopRight = {2, 1, 0};
	Panel.BottomLeft = {1, -1, 0};
	const auto UV = IntersectUiWorldPanel3D(Panel, {1.5f, 0, -1}, {1.5f, 0, 1});
	REQUIRE(UV && Toolbox::Abs(UV->X - 0.5f) < 1e-6f && Toolbox::Abs(UV->Y - 0.5f) < 1e-6f);
	REQUIRE(!IntersectUiWorldPanel3D(Panel, {1.5f, 0, 1}, {1.5f, 0, -1}));
	REQUIRE(!IntersectUiWorldPanel3D(Panel, {1.5f, 0, -2}, {1.5f, 0, -1}));
	REQUIRE(!IntersectUiWorldPanel3D(Panel, {3, 0, -1}, {3, 0, 1}));
	const auto Outside = IntersectUiWorldPanel3D(Panel, {3, 0, -1}, {3, 0, 1}, nullptr, true);
	REQUIRE(Outside && Outside->X > 1);
	Panel.bDoubleSided = true;
	REQUIRE(IntersectUiWorldPanel3D(Panel, {1.5f, 0, 1}, {1.5f, 0, -1}));
}

TEST("UI recorded draw rejects invalid values and surface integer overflow without pretending success")
{
	FUiSurface Surface({0, 0, 640, 480}, {});
	FRecordingRenderer Backend;
	FRenderSystem Renderer(Backend);
	REQUIRE(Renderer.BeginFrame(640, 480, {}));
	FUiDrawList List;
	FUiDrawItem Item;
	Item.Rect = {0, 0, 100, 100};
	Item.Clip = {0, 0, 100, 100};
	Item.Opacity = -1;
	List.EditItems().PushBack(Item);
	REQUIRE(!SubmitUiDrawList(Renderer.GetContext().Get2D(), List, Surface));
	List.EditItems()[0].Opacity = 1;
	List.EditItems()[0].Kind = static_cast<EUiDrawKind>(255);
	REQUIRE(!SubmitUiDrawList(Renderer.GetContext().Get2D(), List, Surface));
	REQUIRE(Renderer.EndFrame());
	FUiSurface Extreme(
	    {(-Toolbox::TNumericLimits<Toolbox::int32>::Max() - 1), 0, Toolbox::TNumericLimits<Toolbox::int32>::Max(), 480},
	    {});
	REQUIRE(!Extreme.IsDisplayable());
}

TEST("UI world slider preserves captured position when leaving the configured view")
{
	FRecordingRenderer Backend;
	FAssetService Assets(Backend.Assets, Backend.Assets, Backend.Assets);
	FUiRoot Root;
	auto Slider = Root.Create<DUiSlider>();
	Slider.Get()->SetWidth(FUiLength::Fill());
	Slider.Get()->SetHeight(FUiLength::Fill());
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Slider.Cast<DUiElement>()));
	FUiSceneHost Host;
	Host.AddWorldPanel3D(Root, {}, Assets, PixelOptions());
	FRenderView3D View;
	View.Eye = {0, 0.5f, -5};
	View.Target = {0, 0.5f, 0};
	View.bViewport = true;
	View.Viewport = {0, 0, 640, 720};
	Host.SetWorldViews3D({View});
	FHostInput Input;
	Input.Raw.MouseX = 320;
	Input.Raw.MouseY = 360;
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	REQUIRE(Root.GetCaptured() == Slider.Get());
	const auto Value = Slider.Get()->GetValue();
	REQUIRE(Value > 0.4 && Value < 0.6);
	Input.Raw.MouseX = 1200;
	(void)Input.Send(Host);
	REQUIRE(Slider.Get()->GetValue() == Value);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Root.GetCaptured() == nullptr);
	REQUIRE(Slider.Get()->GetValue() == Value);
}
