// SPDX-License-Identifier: NOASSERTION
// P4: DPI連動の倍率と、高さ基準・固定ピクセルとの区別（DPIを二重に掛けない）。期待値は既知の倍率から独立に求める。
#include "Ui/UiRuntimeTestSupport.h"
using namespace UiTest;

namespace
{
// 論理(Left,Top)に寸法(Size,Size)のボタンを置き、決定を数える。
struct FTarget
{
	TUiRef<DUiButton> Button;
	Toolbox::int32 Clicks = 0;
	FUiScope Scope;
};

void AddTarget(FUiRoot& Root, FTarget& Target, Toolbox::f32 Left, Toolbox::f32 Top, Toolbox::f32 Size)
{
	Target.Button = Root.Create<DUiButton>("");
	Target.Button.Get()->SetAbsolutePosition({Left, Top});
	Target.Button.Get()->SetWidth(FUiLength::Fixed(Size));
	Target.Button.Get()->SetHeight(FUiLength::Fixed(Size));
	REQUIRE(Root.AddToLayer(EUiLayer::Panel, Target.Button.Cast<DUiElement>()));
	Target.Scope.Add(Target.Button.Get()->OnClicked().Subscribe(
	    [&Target]()
	    {
		    ++Target.Clicks;
	    }));
}
} // namespace

TEST("UI DPI scale applies only to the DPI mode and composes with the user scale once")
{
	const Toolbox::int32 Dpis[] = {96, 120, 144, 192};
	const Toolbox::int32 Sizes[][2] = {{1280, 720}, {1280, 800}, {2560, 1080}, {1001, 501}};
	for (const Toolbox::int32 Dpi : Dpis)
	{
		for (const auto& Size : Sizes)
		{
			FUiRoot Height;
			FUiRoot Scaled;
			FUiRoot Fixed;
			FTarget Target;
			AddTarget(Scaled, Target, 10, 10, 20);
			FUiSceneHost Host;
			FUiDisplayOptions HeightOptions;
			HeightOptions.Scale.Mode = EUiScaleMode::ReferenceHeight;
			HeightOptions.Scale.ReferenceHeight = 360;
			FUiDisplayOptions DpiOptions;
			DpiOptions.Scale.Mode = EUiScaleMode::Dpi;
			DpiOptions.Scale.UserScale = 1.25f;
			DpiOptions.Layer = 2000;
			FUiDisplayOptions FixedOptions = PixelOptions();
			FixedOptions.Scale.UserScale = 2;
			const auto HeightId = Host.AddScreen(Height, HeightOptions);
			// 奇数幅の左右の表示先（左がDPI連動、右が固定ピクセル）。
			const Toolbox::int32 Split = Size[0] / 2 + 1;
			const auto DpiId = Host.AddViewport(Scaled, {0, 0, Split, Size[1]}, DpiOptions);
			const auto FixedId = Host.AddViewport(Fixed, {Split, 0, Size[0], Size[1]}, FixedOptions);
			FHostInput Input;
			Input.Window.bKnown = true;
			Input.Window.RenderWidth = Size[0];
			Input.Window.RenderHeight = Size[1];
			Input.Window.ClientWidth = Size[0];
			Input.Window.ClientHeight = Size[1];
			Input.Window.Dpi = Dpi;
			(void)Input.Send(Host);
			// 高さ基準は描画先の高さだけ、固定ピクセルは利用者倍率だけで決まり、DPIを掛けない。
			const Toolbox::f32 Expected = static_cast<Toolbox::f32>(Dpi) / 96.0f * 1.25f;
			REQUIRE(Toolbox::Abs(Host.GetSurface(HeightId).GetScale() - static_cast<Toolbox::f32>(Size[1]) / 360.0f) <
			        1.0e-5f);
			REQUIRE(Toolbox::Abs(Host.GetSurface(DpiId).GetScale() - Expected) < 1.0e-5f);
			REQUIRE(Host.GetSurface(FixedId).GetScale() == 2);
			// 論理(10,10)〜(30,30)のボタンは、画素では倍率を一度だけ掛けた位置にある。
			const Toolbox::f32 Center = 20 * Expected;
			Input.Raw.MouseX = static_cast<Toolbox::int32>(Center);
			Input.Raw.MouseY = static_cast<Toolbox::int32>(Center);
			(void)Input.Send(Host);
			Input.Raw.MouseButtons[0] = true;
			(void)Input.Send(Host);
			Input.Raw.MouseButtons[0] = false;
			(void)Input.Send(Host);
			REQUIRE(Target.Clicks == 1);
			// 倍率の外側（DPIを二度掛けた位置）には届かない。
			const Toolbox::f32 Outside = 31 * Expected + 1;
			if (Dpi != 96 && Outside < static_cast<Toolbox::f32>(Split))
			{
				Input.Raw.MouseX = static_cast<Toolbox::int32>(Outside);
				Input.Raw.MouseY = static_cast<Toolbox::int32>(Center);
				(void)Input.Send(Host);
				Input.Raw.MouseButtons[0] = true;
				(void)Input.Send(Host);
				Input.Raw.MouseButtons[0] = false;
				(void)Input.Send(Host);
				REQUIRE(Target.Clicks == 1);
			}
		}
	}
}

TEST("UI surface rejects invalid DPI instead of falling back to a guessed scale")
{
	FUiScaleSettings Settings;
	Settings.Mode = EUiScaleMode::Dpi;
	Settings.Dpi = 0;
	REQUIRE(!FUiSurface({0, 0, 100, 100}, Settings).IsDisplayable());
	Settings.Dpi = 144;
	const FUiSurface Surface({0, 0, 100, 100}, Settings);
	REQUIRE(Surface.IsDisplayable() && Surface.GetScale() == 1.5f);
	REQUIRE(Surface.ToPixel(FVector2{10, 10}).X == 15);
}
