// SPDX-License-Identifier: NOASSERTION
// W2: UIが受けた入力の所有と、押下中の非表示・破棄・切断・プレイヤーの割当の組合せ。
#include "Ui/UiRuntimeTestSupport.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiScrollView.h"
using namespace UiTest;

namespace
{
// ボタンの決定を数える購読。
struct FClickCounter
{
	Toolbox::int32 Clicks = 0;
	FUiSubscription Subscription;
	explicit FClickCounter(DUiButton& Button)
	{
		Subscription = Button.OnClicked().Subscribe(
		    [this]()
		    {
			    ++Clicks;
		    });
	}
};
} // namespace

TEST("UI Host without displays passes every input unchanged")
{
	FUiSceneHost Host;
	FHostInput Input;
	Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = true;
	Input.Raw.MouseButtons[0] = true;
	Input.Raw.Pads[0].bConnected = true;
	Input.Raw.Pads[0].Buttons[0] = true;
	Input.Raw.Pads[0].LeftX = 0.9f;
	Input.Raw.Wheel = 2;
	const auto Pressed = Input.Send(Host);
	REQUIRE(Pressed.WasPressed(EKey::Space) && Pressed.WasMousePressed(EMouseButton::Left) &&
	        Pressed.WasPadPressed(0, 0));
	REQUIRE(Pressed.GetRaw().Pads[0].LeftX == 0.9f && Pressed.GetRaw().Wheel == 2);
	Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = false;
	REQUIRE(Input.Send(Host).WasReleased(EKey::Space));
	REQUIRE(Host.GetLastRouting().OwnedKeys == 0 && Host.GetLastRouting().OwnedMouseButtons == 0);
}

TEST("UI mouse press stays owned after the pressed button is hidden or destroyed until physical release")
{
	for (Toolbox::int32 Variant = 0; Variant < 3; ++Variant)
	{
		FUiRoot Root;
		auto Button = FullButton(Root);
		FClickCounter Counter(*Button.Get());
		FUiSceneHost Host;
		const auto Display = Host.AddScreen(Root, PixelOptions());
		FHostInput Input;
		Input.Raw.MouseX = 100;
		Input.Raw.MouseY = 100;
		(void)Input.Send(Host);
		Input.Raw.MouseButtons[0] = true;
		REQUIRE(!Input.Send(Host).IsMouseDown(EMouseButton::Left));
		// 押下中に、ボタンを非表示・破棄・表示先から外す。
		if (Variant == 0)
		{
			Button.Get()->SetVisibility(EUiVisibility::Hidden);
		}
		else if (Variant == 1)
		{
			Root.Destroy(Button.Cast<DUiElement>());
		}
		else
		{
			REQUIRE(Host.Remove(Display));
		}
		// ゲームには押下も押し続けも届かない（新しい押下として流さない）。
		for (Toolbox::int32 Frame = 0; Frame < 3; ++Frame)
		{
			const auto Held = Input.Send(Host);
			REQUIRE(!Held.IsMouseDown(EMouseButton::Left) && !Held.WasMousePressed(EMouseButton::Left));
		}
		Input.Raw.MouseButtons[0] = false;
		const auto Released = Input.Send(Host);
		REQUIRE(!Released.WasMouseReleased(EMouseButton::Left));
		REQUIRE(Counter.Clicks == 0);
		// 離した後の押下は（UIがなければ）ゲームへ届く。
		if (Variant != 0)
		{
			Input.Raw.MouseButtons[0] = true;
			REQUIRE(Input.Send(Host).WasMousePressed(EMouseButton::Left));
		}
	}
}

TEST("UI modal interrupts a held game key and does not deliver a new press when it closes while held")
{
	FUiRoot Root;
	auto Popup = Root.Create<DUiPopup>();
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FHostInput Input;
	const auto Space = static_cast<Toolbox::size_t>(EKey::Space);
	const auto Jump = static_cast<Toolbox::size_t>(EKey::W);
	// ゲームがWを押し続けている（UIの操作に割り当てていないキー）。
	Input.Raw.Keys[Jump] = true;
	REQUIRE(Input.Send(Host).WasPressed(EKey::W));
	REQUIRE(Input.Send(Host).IsDown(EKey::W));
	// Modalを開くと、押し続けを安全に中断する（ゲームには解放として見える）。
	REQUIRE(Popup.Get()->Open());
	const auto Interrupted = Input.Send(Host);
	REQUIRE(!Interrupted.IsDown(EKey::W) && Interrupted.WasReleased(EKey::W));
	// Modal中に押したキーもゲームへ届かない。
	Input.Raw.Keys[Space] = true;
	REQUIRE(!Input.Send(Host).IsDown(EKey::Space));
	// 押したまま閉じても、離すまでゲームへ新しい押下として届けない。
	Popup.Get()->Close();
	for (Toolbox::int32 Frame = 0; Frame < 3; ++Frame)
	{
		const auto Held = Input.Send(Host);
		REQUIRE(!Held.IsDown(EKey::W) && !Held.WasPressed(EKey::W) && !Held.IsDown(EKey::Space));
	}
	Input.Raw.Keys[Jump] = false;
	Input.Raw.Keys[Space] = false;
	(void)Input.Send(Host);
	Input.Raw.Keys[Jump] = true;
	REQUIRE(Input.Send(Host).WasPressed(EKey::W));
}

TEST("UI independent left and right roots receive only their own player's confirm")
{
	FUiRoot Left;
	FUiRoot Right;
	auto LeftButton = FullButton(Left);
	auto RightButton = FullButton(Right);
	FClickCounter LeftClicks(*LeftButton.Get());
	FClickCounter RightClicks(*RightButton.Get());
	FUiSceneHost Host;
	FUiDisplayOptions First = PixelOptions();
	First.Navigation = EUiNavigationPolicy::Always;
	First.Player = 0;
	FUiDisplayOptions Second = First;
	Second.Player = 1;
	Host.AddViewport(Left, {0, 0, 640, 720}, First);
	Host.AddViewport(Right, {640, 0, 1280, 720}, Second);
	REQUIRE(Left.SetFocus(LeftButton.Cast<DUiElement>()) && Right.SetFocus(RightButton.Cast<DUiElement>()));
	FHostInput Input;
	Input.Raw.Pads[1].bConnected = true;
	(void)Input.Send(Host);
	// プレイヤー1（パッド1）の決定は右だけ。
	Input.Raw.Pads[1].Buttons[0] = true;
	const auto PadFrame = Input.Send(Host);
	REQUIRE(LeftClicks.Clicks == 0 && RightClicks.Clicks == 1);
	REQUIRE(!PadFrame.IsPadDown(1, 0));
	// 押し続けても再決定しない。
	(void)Input.Send(Host);
	(void)Input.Send(Host);
	REQUIRE(RightClicks.Clicks == 1);
	Input.Raw.Pads[1].Buttons[0] = false;
	(void)Input.Send(Host);
	// プレイヤー0（キーボード）の決定は左だけ。
	Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::Enter)] = true;
	const auto KeyFrame = Input.Send(Host);
	REQUIRE(LeftClicks.Clicks == 1 && RightClicks.Clicks == 1);
	REQUIRE(!KeyFrame.IsDown(EKey::Enter));
	REQUIRE(Host.GetLastRouting().NavigationDisplays[0] != Host.GetLastRouting().NavigationDisplays[1]);
}

TEST("UI pad disconnect while holding a direction clears ownership and repeat")
{
	FUiRoot Root;
	auto Popup = Root.Create<DUiPopup>();
	REQUIRE(Popup.Get()->Open());
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FHostInput Input;
	Input.Raw.Pads[0].bConnected = true;
	Input.Raw.Pads[0].Buttons[2] = true;
	Input.Raw.Pads[0].LeftY = 1;
	for (Toolbox::int32 Frame = 0; Frame < 40; ++Frame)
	{
		const auto Held = Input.Send(Host);
		REQUIRE(!Held.IsPadDown(0, 2) && Held.GetRaw().Pads[0].LeftY == 0);
	}
	REQUIRE(Host.GetLastRouting().OwnedPadButtons == 1);
	// 切断すると押下・スティックの所有は残らない。
	Input.Raw.Pads[0] = {};
	(void)Input.Send(Host);
	REQUIRE(Host.GetLastRouting().OwnedPadButtons == 0);
	// 閉じた後に再接続して押すと、ゲームへ新しい押下として届く。
	Popup.Get()->Close();
	Input.Raw.Pads[0].bConnected = true;
	Input.Raw.Pads[0].Buttons[2] = true;
	Input.Raw.Pads[0].LeftY = 1;
	const auto Fresh = Input.Send(Host);
	REQUIRE(Fresh.WasPadPressed(0, 2) && Fresh.GetRaw().Pads[0].LeftY == 1);
}

TEST("UI routes once per frame even when the scene asks twice")
{
	FUiRoot Root;
	auto Button = FullButton(Root);
	FClickCounter Counter(*Button.Get());
	FUiSceneHost Host;
	FUiDisplayOptions Options = PixelOptions();
	Options.Navigation = EUiNavigationPolicy::Always;
	Host.AddScreen(Root, Options);
	REQUIRE(Root.SetFocus(Button.Cast<DUiElement>()));
	FHostInput Input;
	(void)Input.Send(Host);
	Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::Enter)] = true;
	Input.Tracker.Advance(Input.Raw);
	FFrameTime Time;
	Time.FrameIndex = 1000;
	Time.UnscaledDeltaSeconds = 1.0 / 60.0;
	const auto First = Host.RouteInput({Input.Tracker.GetSnapshot(), Time});
	const auto Second = Host.RouteInput({Input.Tracker.GetSnapshot(), Time});
	REQUIRE(Counter.Clicks == 1);
	REQUIRE(First.IsDown(EKey::Enter) == Second.IsDown(EKey::Enter) && !First.IsDown(EKey::Enter));
}

TEST("UI host maps Shift wheel to horizontal scroll and keeps unconsumed game wheel filtered over UI")
{
	FUiRoot Root;
	auto Scroll = Root.Create<DUiScrollView>();
	Scroll.Get()->SetWidth(FUiLength::Fixed(200));
	Scroll.Get()->SetHeight(FUiLength::Fixed(100));
	Scroll.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	Scroll.Get()->SetScrollAxes(EUiScrollAxes::Horizontal);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Scroll.Cast<DUiElement>()));
	auto Content = Root.Create<DUiPanel>();
	Content.Get()->SetWidth(FUiLength::Fixed(1000));
	Content.Get()->SetHeight(FUiLength::Fixed(40));
	Scroll.Get()->SetContent(Content.Cast<DUiElement>());
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FHostInput Input;
	Input.Raw.MouseX = 20;
	Input.Raw.MouseY = 20;
	(void)Input.Send(Host);
	// DxLibの奥への回転は負。Shiftなしの縦は横だけのスクロールを動かさないが、UIの上なのでゲームへ流さない。
	Input.Raw.Wheel = -1;
	REQUIRE(Input.Send(Host).GetRaw().Wheel == 0);
	REQUIRE(Scroll.Get()->GetScrollOffsetX() == 0);
	Input.Raw.Keys[static_cast<Toolbox::size_t>(EKey::LeftShift)] = true;
	(void)Input.Send(Host);
	REQUIRE(Scroll.Get()->GetScrollOffsetX() == 50);
}

TEST("UI modal blocks a click on the element behind it outside the modal content")
{
	FUiRoot Root;
	auto Behind = FullButton(Root);
	FClickCounter Counter(*Behind.Get());
	auto Popup = Root.Create<DUiPopup>();
	auto Content = Root.Create<DUiButton>("modal");
	Content.Get()->SetWidth(FUiLength::Fixed(100));
	Content.Get()->SetHeight(FUiLength::Fixed(40));
	Popup.Get()->SetContent(Content.Cast<DUiElement>());
	// 背景が画面を覆わない小さいModalでも、外側の押下は背後へ抜けない（ヒット判定のModalの遮り）。
	Popup.Get()->SetWidth(FUiLength::Fixed(200));
	Popup.Get()->SetHeight(FUiLength::Fixed(100));
	Popup.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FHostInput Input;
	Input.Raw.MouseX = 600;
	Input.Raw.MouseY = 400;
	(void)Input.Send(Host);
	// Modalなしなら背後のボタンが押せる（前提の確認）。
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Counter.Clicks == 1);
	// Modalの外を押しても、背後の要素へ抜けない。
	REQUIRE(Popup.Get()->Open());
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Counter.Clicks == 1);
}
