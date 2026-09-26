// SPDX-License-Identifier: NOASSERTION
#include "NativePixels.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiLabel.h"
#include "DxLib.h"
namespace Dxf::UiSmoke
{
namespace
{
void Require(bool Value, const char* Error)
{
	if (!Value)
	{
		throw Toolbox::FException(Error);
	}
}

void ReleaseImage(void*, Toolbox::int32 Handle) noexcept
{
	(void)DxLib::DeleteSoftImage(Handle);
}

TUiRef<DUiPanel> Panel(FUiRoot& Root, FVector2 Position, FUiSize Size, FColor Color)
{
	auto Ref = Root.Create<DUiPanel>(EUiStackMode::Overlay);
	Ref.Get()->SetAbsolutePosition(Position);
	Ref.Get()->SetWidth(FUiLength::Fixed(Size.Width));
	Ref.Get()->SetHeight(FUiLength::Fixed(Size.Height));
	FUiStylePatch Style;
	Style.Background = Color;
	Ref.Get()->SetStyleOverride(Style);
	Ref.Get()->SetHitTest(EUiHitTest::None);
	return Ref;
}
} // namespace

void InstallPixelFixture(FUiRoot& Root)
{
	auto Under = Panel(Root, {32, 600}, {220, 60}, {17, 195, 51, 255});
	Require(static_cast<bool>(Root.AddToLayer(EUiLayer::Panel, Under.Cast<DUiElement>())), "fixture underlay");
	auto Clip = Panel(Root, {32, 600}, {160, 60}, {10, 20, 200, 255});
	Clip.Get()->SetClipChildren(true);
	Require(static_cast<bool>(Root.AddToLayer(EUiLayer::Panel, Clip.Cast<DUiElement>())), "fixture clip");
	auto Child = Panel(Root, {120, 0}, {80, 60}, {253, 17, 201, 255});
	Require(static_cast<bool>(Root.AddChild(Clip.Cast<DUiElement>(), Child.Cast<DUiElement>())),
	        "fixture clipped child");
	auto Label = Root.Create<DUiLabel>("UI日本語");
	Label.Get()->SetAbsolutePosition({5, 5});
	Label.Get()->SetWidth(FUiLength::Fixed(110));
	Label.Get()->SetHeight(FUiLength::Fixed(30));
	FUiStylePatch Style;
	Style.Foreground = FColor{255, 255, 255, 255};
	Style.FontSize = 20;
	Label.Get()->SetStyleOverride(Style);
	Require(static_cast<bool>(Root.AddChild(Clip.Cast<DUiElement>(), Label.Cast<DUiElement>())), "fixture text");
}

void VerifyPixels(const Toolbox::FPath& Path)
{
	FNativeHandle Image(DxLib::MakeARGB8ColorSoftImage(1280, 720), nullptr, &ReleaseImage);
	Require(Image.Get() >= 0, "UI readback image allocation");
	Require(DxLib::GetDrawScreenSoftImage(0, 0, 1280, 720, Image.Get()) == 0, "UI one-shot image readback");
	auto Pixel = [&](Toolbox::int32 X, Toolbox::int32 Y)
	{
		int R = 0;
		int G = 0;
		int B = 0;
		int A = 0;
		Require(DxLib::GetPixelSoftImage(Image.Get(), X, Y, &R, &G, &B, &A) == 0, "UI CPU image pixel read");
		return FColor{static_cast<Toolbox::uint8>(R), static_cast<Toolbox::uint8>(G), static_cast<Toolbox::uint8>(B),
		              static_cast<Toolbox::uint8>(A)};
	};
	const auto Inside = Pixel(170, 645);
	const auto Outside = Pixel(210, 645);
	Require(Inside.R == 253 && Inside.G == 17 && Inside.B == 201, "UI clipped color inside");
	Require(Outside.R == 17 && Outside.G == 195 && Outside.B == 51, "UI clip preserves outside pixels");
	Toolbox::int32 White = 0;
	for (Toolbox::int32 Y = 606; Y < 636; ++Y)
		for (Toolbox::int32 X = 38; X < 146; ++X)
		{
			const auto C = Pixel(X, Y);
			if (C.R > 180 && C.G > 180 && C.B > 180)
			{
				++White;
			}
		}
	Require(White > 10, "UI real-font text pixels");
	Require(DxLib::SaveDrawScreenToPNG(0, 0, 1280, 720, Path.ToUtf8().CStr()) == 0, "UI screenshot save");
}
} // namespace Dxf::UiSmoke
