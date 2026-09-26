// SPDX-License-Identifier: NOASSERTION
// 再配置されたSceneアダプターを利用。入力の取得境界以外にテスト用ソースを使わない。
#include "Dxf/UiRoot.h"
#include "Toolbox/Platform.h"
#include "Dxf/UiButton.h"
#include "Dxf/UiSceneHost.h"
#include "Dxf/InputStateTracker.h"
int main()
{
	using namespace Dxf;
	DScene Scene;
	FUiRoot Root;
	FUiSceneHost Host;
	auto Button = Root.Create<DUiButton>("apply");
	Button.Get()->SetWidth(FUiLength::Fixed(160));
	Button.Get()->SetHeight(FUiLength::Fixed(80));
	if (!Root.AddToLayer(EUiLayer::Normal, Button.Cast<DUiElement>()))
	{
		return 1;
	}
	Toolbox::int32 Clicks = 0;
	auto Subscription = Button.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    ++Clicks;
	    });
	FUiDisplayOptions Options;
	Options.Scale.Mode = EUiScaleMode::FixedPixel;
	Host.AttachTo(Scene);
	Host.AddViewport(Root, {0, 0, 320, 240}, Options);
	Host.AddViewport(Root, {320, 0, 640, 240}, Options);
	FInputStateTracker Input;
	FRawInput Raw;
	Raw.MouseX = 350;
	Raw.MouseY = 40;
	Raw.MouseButtons[0] = true;
	Input.Advance(Raw);
	FFrameTime Time;
	Time.FrameIndex = 1;
	Time.DeltaSeconds = 1.0 / 60.0;
	Time.UnscaledDeltaSeconds = Time.DeltaSeconds;
	FTickContext Context{Input.GetSnapshot(), Time};
	if (Host.RouteInput(Context).IsMouseDown(EMouseButton::Left))
	{
		return 2;
	}
	Raw.MouseButtons[0] = false;
	Input.Advance(Raw);
	Time.FrameIndex = 2;
	FTickContext Released{Input.GetSnapshot(), Time};
	(void)Host.RouteInput(Released);
	if (Clicks != 1 || Host.GetLastRouting().ProcessedRoots != 1)
	{
		return 3;
	}
	Host.DetachFromScene();
	if (Scene.GetInputRouter() != nullptr)
	{
		return 4;
	}
	Toolbox::Out << "UI_RUNTIME_CONSUMER_PASSED\n";
	return 0;
}
