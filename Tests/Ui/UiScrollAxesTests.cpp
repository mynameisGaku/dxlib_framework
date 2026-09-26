// SPDX-License-Identifier: NOASSERTION
// W4: 横・両方向のスクロール、軸ごとの入れ子の消費、時間で決まる慣性。
#include "Ui/UiTestSupport.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiScrollView.h"
using namespace UiTest;

namespace
{
// 左上に固定寸法で置く。
TUiRef<DUiScrollView> MakeScroll(FUiRoot& Root, Toolbox::f32 Width, Toolbox::f32 Height, EUiScrollAxes Axes)
{
	auto Scroll = Root.Create<DUiScrollView>();
	Scroll.Get()->SetWidth(FUiLength::Fixed(Width));
	Scroll.Get()->SetHeight(FUiLength::Fixed(Height));
	Scroll.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	Scroll.Get()->SetScrollAxes(Axes);
	return Scroll;
}

// 固定寸法の内容。
TUiRef<DUiPanel> MakeContent(FUiRoot& Root, Toolbox::f32 Width, Toolbox::f32 Height)
{
	auto Content = Root.Create<DUiPanel>();
	Content.Get()->SetWidth(FUiLength::Fixed(Width));
	Content.Get()->SetHeight(FUiLength::Fixed(Height));
	Content.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	return Content;
}

// 指定位置のホイール（縦・横のノッチ）。
void Wheel(FUiRoot& Root, Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Vertical, Toolbox::f32 Horizontal)
{
	auto Frame = PointerFrame(X, Y, false, false);
	Frame.Pointer.WheelNotches = Vertical;
	Frame.Pointer.WheelNotchesX = Horizontal;
	REQUIRE(Root.ProcessInput(Frame));
}

// 不正な慣性の設定は例外で拒否する。
bool TrySetInertia(DUiScrollView& Scroll, const FUiScrollInertia& Inertia)
{
	try
	{
		Scroll.SetInertia(Inertia);
		return true;
	}
	catch (const Toolbox::FException&)
	{
		return false;
	}
}
} // namespace

TEST("UI horizontal scroll moves only its axis and arranges content by the offset")
{
	FUiRoot Root;
	auto Scroll = MakeScroll(Root, 200, 100, EUiScrollAxes::Horizontal);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Scroll.Cast<DUiElement>()));
	auto Content = MakeContent(Root, 1000, 50);
	Scroll.Get()->SetContent(Content.Cast<DUiElement>());
	LayoutRoot(Root);
	// 縦のバーはなく、横のバーの分だけ表示の高さが減る。
	REQUIRE(Scroll.Get()->GetScrollMaximumX() == 800 && Scroll.Get()->GetScrollMaximum() == 0);
	REQUIRE(Scroll.Get()->GetScrollThumbRect().IsEmpty() && !Scroll.Get()->GetScrollThumbRectX().IsEmpty());
	REQUIRE(Scroll.Get()->GetScrollThumbRectX().Y == 88);
	// 縦のホイールは横だけのスクロールでは使わない。
	Wheel(Root, 20, 20, 1, 0);
	REQUIRE(Scroll.Get()->GetScrollOffsetX() == 0 && Scroll.Get()->GetScrollOffset() == 0);
	Wheel(Root, 20, 20, 0, 1);
	REQUIRE(Scroll.Get()->GetScrollOffsetX() == 50);
	LayoutRoot(Root);
	REQUIRE(Content.Get()->GetRect().X == -50);
	// 右端で止まる。
	Wheel(Root, 20, 20, 0, 100);
	REQUIRE(Scroll.Get()->GetScrollOffsetX() == 800);
	// 横の方向操作はスクロールに使い、縦の方向操作はフォーカスの移動に残す。
	REQUIRE(Root.SetFocus(Scroll.Cast<DUiElement>()));
	REQUIRE(Root.ProcessInput(NavigationFrame(EUiNavigationCommand::Left, true, false)));
	REQUIRE(Scroll.Get()->GetScrollOffsetX() == 750);
}

TEST("UI both-axis scroll drags each thumb independently to its end")
{
	FUiRoot Root;
	auto Scroll = MakeScroll(Root, 200, 100, EUiScrollAxes::Both);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Scroll.Cast<DUiElement>()));
	Scroll.Get()->SetContent(MakeContent(Root, 1000, 1000).Cast<DUiElement>());
	LayoutRoot(Root);
	REQUIRE(Scroll.Get()->GetScrollMaximumX() == 812 && Scroll.Get()->GetScrollMaximum() == 912);
	const auto ThumbX = Scroll.Get()->GetScrollThumbRectX();
	REQUIRE(Root.ProcessInput(PointerFrame(ThumbX.X + 2, ThumbX.Y + 2, true, false)));
	REQUIRE(Root.GetCaptured() == Scroll.Get());
	REQUIRE(Root.ProcessInput(PointerFrame(500, ThumbX.Y + 2, true, true)));
	REQUIRE(Scroll.Get()->GetScrollOffsetX() == 812 && Scroll.Get()->GetScrollOffset() == 0);
	REQUIRE(Root.ProcessInput(PointerFrame(500, ThumbX.Y + 2, false, true)));
	LayoutRoot(Root);
	const auto ThumbY = Scroll.Get()->GetScrollThumbRect();
	REQUIRE(Root.ProcessInput(PointerFrame(ThumbY.X + 2, ThumbY.Y + 2, true, false)));
	REQUIRE(Root.ProcessInput(PointerFrame(ThumbY.X + 2, 500, true, true)));
	REQUIRE(Scroll.Get()->GetScrollOffset() == 912 && Scroll.Get()->GetScrollOffsetX() == 812);
	REQUIRE(Root.ProcessInput(PointerFrame(ThumbY.X + 2, 500, false, true)));
	// 両方のノッチを一度に受ける。
	Wheel(Root, 20, 20, -1, -1);
	REQUIRE(Scroll.Get()->GetScrollOffset() == 890 && Scroll.Get()->GetScrollOffsetX() == 765);
}

TEST("UI nested scroll consumes wheel per axis and passes the other axis outward")
{
	FUiRoot Root;
	auto Outer = MakeScroll(Root, 240, 100, EUiScrollAxes::Vertical);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Outer.Cast<DUiElement>()));
	auto OuterContent = MakeContent(Root, 200, 400);
	Outer.Get()->SetContent(OuterContent.Cast<DUiElement>());
	auto Inner = MakeScroll(Root, 150, 80, EUiScrollAxes::Horizontal);
	REQUIRE(Root.AddChild(OuterContent.Cast<DUiElement>(), Inner.Cast<DUiElement>()));
	Inner.Get()->SetContent(MakeContent(Root, 600, 40).Cast<DUiElement>());
	LayoutRoot(Root);
	// 縦は内側が使わないので外側が使う。
	Wheel(Root, 20, 20, 1, 0);
	REQUIRE(Outer.Get()->GetScrollOffset() == 25 && Inner.Get()->GetScrollOffsetX() == 0);
	LayoutRoot(Root);
	// 横は内側だけが使い、外側の縦は変えない。
	Wheel(Root, 20, 20, 0, 1);
	REQUIRE(Inner.Get()->GetScrollOffsetX() == 37.5 && Outer.Get()->GetScrollOffset() == 25);
	// 同じフレームの縦と横は、それぞれの軸を持つ要素が使う。
	Wheel(Root, 20, 20, 1, 1);
	REQUIRE(Inner.Get()->GetScrollOffsetX() == 75 && Outer.Get()->GetScrollOffset() == 50);
	// 横の端を越えた分は外側へ渡るが、縦だけの外側は動かない。
	Wheel(Root, 20, 20, 0, 100);
	REQUIRE(Inner.Get()->GetScrollOffsetX() == 450 && Outer.Get()->GetScrollOffset() == 50);
}

TEST("UI scroll inertia is off by default and reaches the same positions at 30 60 and 144 Hz")
{
	FUiRoot Root;
	auto Plain = MakeScroll(Root, 200, 100, EUiScrollAxes::Vertical);
	REQUIRE(!Plain.Get()->GetInertia().bEnabled);
	REQUIRE(!TrySetInertia(*Plain.Get(), {true, 0, 0.5}) && !TrySetInertia(*Plain.Get(), {true, 12, -1}));
	const Toolbox::f64 Rates[3] = {30, 60, 144};
	Toolbox::f64 AtSixth[3] = {};
	for (Toolbox::size_t Index = 0; Index < 3; ++Index)
	{
		FUiRoot Case;
		auto Scroll = MakeScroll(Case, 200, 100, EUiScrollAxes::Both);
		REQUIRE(Case.AddToLayer(EUiLayer::Normal, Scroll.Cast<DUiElement>()));
		Scroll.Get()->SetContent(MakeContent(Case, 2000, 2000).Cast<DUiElement>());
		FUiScrollInertia Inertia;
		Inertia.bEnabled = true;
		Scroll.Get()->SetInertia(Inertia);
		Scroll.Get()->SetWheelStep(50, false);
		LayoutRoot(Case);
		// 2ノッチ（100）の移動は、ホイールの時点ではまだ進まない。
		Wheel(Case, 20, 20, 2, 0);
		REQUIRE(Scroll.Get()->GetScrollOffset() == 0 && Scroll.Get()->GetPendingScroll().Y == 100);
		const Toolbox::f64 Delta = 1.0 / Rates[Index];
		const Toolbox::int32 Frames = static_cast<Toolbox::int32>(Rates[Index]) / 6;
		for (Toolbox::int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE(Case.Update(Delta));
		}
		AtSixth[Index] = Scroll.Get()->GetScrollOffset();
		// 残りは e^(-12/6) の割合。
		REQUIRE(Toolbox::Abs(AtSixth[Index] - 100 * (1 - Toolbox::Exp(-2.0))) < 1.0e-6);
		for (Toolbox::int32 Frame = 0; Frame < static_cast<Toolbox::int32>(Rates[Index]) * 2; ++Frame)
		{
			REQUIRE(Case.Update(Delta));
		}
		// 最終位置は慣性なしと同じで、到着後は毎フレームの更新を受けない。
		REQUIRE(Scroll.Get()->GetScrollOffset() == 100 && Scroll.Get()->GetPendingScroll().Y == 0);
		REQUIRE(!Scroll.Get()->WantsUpdate());
		// 明示の位置の設定は慣性の残りを捨てる。
		Wheel(Case, 20, 20, 0, 3);
		REQUIRE(Scroll.Get()->GetPendingScroll().X == 150);
		Scroll.Get()->SetScrollOffsetX(10);
		REQUIRE(Scroll.Get()->GetPendingScroll().X == 0 && Scroll.Get()->GetScrollOffsetX() == 10);
	}
	REQUIRE(Toolbox::Abs(AtSixth[0] - AtSixth[1]) < 1.0e-9 && Toolbox::Abs(AtSixth[1] - AtSixth[2]) < 1.0e-9);
}
