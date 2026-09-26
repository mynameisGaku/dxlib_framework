// SPDX-License-Identifier: NOASSERTION
// P5: 失焦・マウスの捕捉の喪失を通常の解放と区別し、誤った決定・押し続け・偽の押下を起こさない。カーソルの意図。
#include "Ui/UiRuntimeTestSupport.h"
#include "Dxf/UiSlider.h"
using namespace UiTest;

namespace
{
struct FClicks
{
	Toolbox::int32 Count = 0;
	FUiScope Scope;
	explicit FClicks(DUiButton& Button)
	{
		Scope.Add(Button.OnClicked().Subscribe(
		    [this]()
		    {
			    ++Count;
		    }));
	}
};
} // namespace

TEST("Input tracker masks inputs still held when focus returns until they are released or neutral")
{
	FInputStateTracker Tracker;
	FRawInput Raw;
	Raw.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = true;
	Raw.MouseButtons[0] = true;
	Raw.Pads[0].bConnected = true;
	Raw.Pads[0].Buttons[0] = true;
	Raw.Pads[0].LeftX = 0.9f;
	Tracker.Advance(Raw);
	REQUIRE(Tracker.GetSnapshot().WasPressed(EKey::Space));
	// 失焦の間は何も押されていない。
	Raw.bFocused = false;
	Tracker.Advance(Raw);
	REQUIRE(!Tracker.GetSnapshot().IsDown(EKey::Space) && Tracker.GetSnapshot().WasReleased(EKey::Space));
	// 押したまま復帰しても、新しい押下（ジャンプ・決定）にならない。
	Raw.bFocused = true;
	for (Toolbox::int32 Frame = 0; Frame < 3; ++Frame)
	{
		Tracker.Advance(Raw);
		const auto& Snapshot = Tracker.GetSnapshot();
		REQUIRE(!Snapshot.IsDown(EKey::Space) && !Snapshot.WasPressed(EKey::Space));
		REQUIRE(!Snapshot.IsMouseDown(EMouseButton::Left) && !Snapshot.IsPadDown(0, 0));
		REQUIRE(Snapshot.GetRaw().Pads[0].LeftX == 0.0f);
	}
	// 離す（スティックは中立へ戻す）と対象から外れ、次の押下は届く。
	Raw.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = false;
	Raw.Pads[0].LeftX = 0.1f;
	Tracker.Advance(Raw);
	REQUIRE(Tracker.GetSnapshot().GetRaw().Pads[0].LeftX == 0.1f);
	Raw.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = true;
	Raw.Pads[0].LeftX = 0.9f;
	Tracker.Advance(Raw);
	REQUIRE(Tracker.GetSnapshot().WasPressed(EKey::Space) && Tracker.GetSnapshot().GetRaw().Pads[0].LeftX == 0.9f);
	// マウス・パッドのボタンは離すまで対象のまま。
	REQUIRE(!Tracker.GetSnapshot().IsMouseDown(EMouseButton::Left) && !Tracker.GetSnapshot().IsPadDown(0, 0));
}

TEST("UI press cancelled by focus loss never clicks, even when released after focus returns")
{
	FUiRoot Root;
	auto Button = FullButton(Root);
	FClicks Clicks(*Button.Get());
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FHostInput Input;
	Input.Raw.MouseX = 100;
	Input.Raw.MouseY = 100;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	REQUIRE(Root.GetCaptured() == Button.Get());
	// 失焦は通常の解放ではなく取り消し（決定しない）。
	Input.Raw.bFocused = false;
	(void)Input.Send(Host);
	REQUIRE(Host.GetLastRouting().bPointerCancelled && Root.GetCaptured() == nullptr && Clicks.Count == 0);
	// 押したまま復帰し、その後に離しても決定しない。
	Input.Raw.bFocused = true;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Clicks.Count == 0);
	// 次の押下からは通常どおり。
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Clicks.Count == 1);
	// ウィンドウの状態の失焦（入力の取得より先に届いた場合）も同じ扱い。
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Window.bKnown = true;
	Input.Window.bFocused = false;
	Input.Window.RenderWidth = 1280;
	Input.Window.RenderHeight = 720;
	(void)Input.Send(Host);
	Input.Window.bFocused = true;
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Clicks.Count == 1);
}

TEST("UI drag requests OS pointer capture and a lost capture cancels the drag without committing a click")
{
	FUiRoot Root;
	auto Slider = Root.Create<DUiSlider>();
	Slider.Get()->SetRange(0, 1, 0.01);
	Slider.Get()->SetWidth(FUiLength::Fixed(200));
	Slider.Get()->SetHeight(FUiLength::Fixed(40));
	Slider.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Slider.Cast<DUiElement>()));
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FHostInput Input;
	Input.Window.bKnown = true;
	Input.Window.RenderWidth = 1280;
	Input.Window.RenderHeight = 720;
	Input.Window.bPointerCaptureSupported = true;
	Input.Raw.MouseX = 50;
	Input.Raw.MouseY = 20;
	(void)Input.Send(Host);
	// ホバーの間は横のドラッグのカーソル、捕捉は求めない。
	REQUIRE(Input.Requests.bCursorRequested && Input.Requests.Cursor == ECursorShape::ResizeHorizontal);
	REQUIRE(!Input.Requests.bPointerCapture);
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	REQUIRE(Input.Requests.bPointerCapture && Root.GetCaptured() == Slider.Get());
	const Toolbox::f64 Held = Slider.Get()->GetValue();
	// Platformが捕捉を取得した。ウィンドウの外（負の座標）へ動かしても値は端へ制限されるだけ。
	Input.Window.bPointerCaptured = true;
	Input.Raw.MouseX = -300;
	(void)Input.Send(Host);
	REQUIRE(Slider.Get()->GetValue() == 0 && Input.Requests.bPointerCapture);
	Input.Raw.MouseX = 150;
	(void)Input.Send(Host);
	REQUIRE(Slider.Get()->GetValue() > Held);
	const Toolbox::f64 Committed = Slider.Get()->GetValue();
	// 捕捉を失った（他のウィンドウが奪った等）：ドラッグを取り消し、確定済みの値を保つ。
	Input.Window.bPointerCaptured = false;
	Input.Raw.MouseX = 190;
	(void)Input.Send(Host);
	REQUIRE(Host.GetLastRouting().bPointerCancelled && Root.GetCaptured() == nullptr);
	REQUIRE(Slider.Get()->GetValue() == Committed && !Input.Requests.bPointerCapture);
	// 以後の移動は値を変えない（押したままでも取り消したドラッグは戻らない）。
	Input.Raw.MouseX = 20;
	(void)Input.Send(Host);
	REQUIRE(Slider.Get()->GetValue() == Committed);
}

TEST("UI cursor intent follows the front element and capture support is optional")
{
	FUiRoot Root;
	auto Button = FullButton(Root);
	FClicks Clicks(*Button.Get());
	FUiSceneHost Host;
	Host.AddScreen(Root, PixelOptions());
	FHostInput Input;
	Input.Raw.MouseX = 100;
	Input.Raw.MouseY = 100;
	(void)Input.Send(Host);
	REQUIRE(Input.Requests.Cursor == ECursorShape::Hand);
	// 無効な部品の上は標準の矢印。
	Button.Get()->SetEnabled(false);
	(void)Input.Send(Host);
	REQUIRE(Input.Requests.Cursor == ECursorShape::Arrow);
	Button.Get()->SetEnabled(true);
	// 捕捉に対応しないPlatform（既定）では、捕捉していないことを喪失として扱わない。
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	(void)Input.Send(Host);
	REQUIRE(!Host.GetLastRouting().bPointerCancelled && Root.GetCaptured() == Button.Get());
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Clicks.Count == 1);
	// ウィンドウの外で離すと決定しない（座標を画面内へ寄せない）。
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	Input.Raw.MouseX = -40;
	Input.Raw.MouseY = 900;
	(void)Input.Send(Host);
	Input.Raw.MouseButtons[0] = false;
	(void)Input.Send(Host);
	REQUIRE(Clicks.Count == 1);
	// 捕捉中にルートを表示先から外すと、捕捉の要求も止まる。
	Input.Raw.MouseX = 100;
	Input.Raw.MouseY = 100;
	Input.Raw.MouseButtons[0] = true;
	(void)Input.Send(Host);
	REQUIRE(Input.Requests.bPointerCapture);
	Root.Destroy(Button.Cast<DUiElement>());
	(void)Input.Send(Host);
	REQUIRE(!Input.Requests.bPointerCapture);
}
