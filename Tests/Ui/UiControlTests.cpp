// SPDX-License-Identifier: NOASSERTION
#include "UiTestSupport.h"
#include "Dxf/UiToggle.h"
#include "Dxf/UiSlider.h"
#include "Dxf/UiProgressBar.h"
#include "Dxf/UiScrollView.h"
#include "Dxf/UiListView.h"
#include "Dxf/UiPopup.h"
#include "Dxf/UiChoice.h"
#include "Dxf/UiStack.h"
using namespace Dxf;
using namespace UiTest;
namespace
{
// 小さい試験部品を左上の固定領域へ置く。
template <typename T> void Place(FUiRoot& Root, const TUiRef<T>& Element, Toolbox::f32 Width, Toolbox::f32 Height)
{
	Element.Get()->SetWidth(FUiLength::Fixed(Width));
	Element.Get()->SetHeight(FUiLength::Fixed(Height));
	Element.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Element.template Cast<DUiElement>()));
}
// 安定キーは1から。試験の期待値を列の添字と区別する。
Toolbox::TVector<FUiListItem> Items(Toolbox::size_t Count)
{
	Toolbox::TVector<FUiListItem> Out;
	Out.Reserve(Count);
	for (Toolbox::size_t Index = 0; Index < Count; ++Index)
	{
		Out.PushBack({static_cast<Toolbox::uint64>(Index + 1), "Item " + Toolbox::ToString(Index + 1), "Tip"});
	}
	return Out;
}
} // namespace

TEST("UI toggle and choice separate model reflection from user changes")
{
	FUiRoot Root;
	auto Toggle = Root.Create<DUiToggle>("Enabled");
	Place(Root, Toggle, 120, 40);
	Toolbox::int32 Changes = 0;
	auto Sub = Toggle.Get()->OnValueChanged().Subscribe(
	    [&](bool)
	    {
		    ++Changes;
	    });
	Toggle.Get()->SetValue(true);
	LayoutRoot(Root);
	REQUIRE(Changes == 0);
	Click(Root, 20, 20);
	REQUIRE(!Toggle.Get()->GetValue());
	REQUIRE(Changes == 1);
	auto Choice = Root.Create<DUiChoice>();
	Toolbox::TVector<Toolbox::FString> Options;
	Options.PushBack("One");
	Options.PushBack("Two");
	Choice.Get()->SetOptions(Toolbox::Move(Options));
	Choice.Get()->SetAbsolutePosition({150, 0});
	Place(Root, Choice, 120, 40);
	LayoutRoot(Root);
	Click(Root, 170, 20);
	REQUIRE(Choice.Get()->GetSelectedIndex() == 1);
	REQUIRE(Choice.Get()->GetText() == "Two");
}

TEST("UI disabled front button blocks underlying input and overlay ordering is stable")
{
	FUiRoot Root;
	auto Back = Root.Create<DUiButton>("Back");
	auto Front = Root.Create<DUiButton>("Front");
	Place(Root, Back, 120, 40);
	Place(Root, Front, 120, 40);
	Front.Get()->SetEnabled(false);
	Toolbox::int32 Calls = 0;
	auto Sub = Back.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    ++Calls;
	    });
	LayoutRoot(Root);
	Click(Root, 10, 10);
	REQUIRE(Calls == 0);
	Front.Get()->SetVisibility(EUiVisibility::Collapsed);
	LayoutRoot(Root);
	Click(Root, 10, 10);
	REQUIRE(Calls == 1);
}

TEST("UI slider clamps snaps and captures outside without programmatic events")
{
	FUiRoot Root;
	auto Slider = Root.Create<DUiSlider>();
	Place(Root, Slider, 200, 30);
	Slider.Get()->SetRange(-10, 10, 2);
	Toolbox::int32 Changes = 0;
	auto Sub = Slider.Get()->OnValueChanged().Subscribe(
	    [&](Toolbox::f64)
	    {
		    ++Changes;
	    });
	Slider.Get()->SetValue(3.1);
	REQUIRE(Slider.Get()->GetValue() == 4);
	REQUIRE(Changes == 0);
	LayoutRoot(Root);
	REQUIRE(Root.ProcessInput(PointerFrame(100, 15, true, false)));
	REQUIRE(Root.GetCaptured() == Slider.Get());
	auto Outside = PointerFrame(300, 15, true, true);
	Outside.Pointer.bPresent = false;
	REQUIRE(Root.ProcessInput(Outside));
	REQUIRE(Slider.Get()->GetValue() == 10);
	Outside.Pointer.Down[0] = false;
	Outside.Pointer.Released[0] = true;
	REQUIRE(Root.ProcessInput(Outside));
	REQUIRE(Root.GetCaptured() == nullptr);
	REQUIRE(Changes >= 1);
	const Toolbox::f64 Before = Slider.Get()->GetValue();
	bool bThrew = false;
	try
	{
		Slider.Get()->SetRange(2, 1, 0);
	}
	catch (const Toolbox::FException&)
	{
		bThrew = true;
	}
	REQUIRE(bThrew);
	REQUIRE(Slider.Get()->GetValue() == Before);
}

TEST("UI nested scroll consumes only its capacity and bubbles remaining notches")
{
	FUiRoot Root;
	auto Outer = Root.Create<DUiScrollView>();
	Place(Root, Outer, 240, 100);
	auto Content = Root.Create<DUiPanel>();
	Content.Get()->SetHeight(FUiLength::Fixed(400));
	Outer.Get()->SetContent(Content.Cast<DUiElement>());
	auto Inner = Root.Create<DUiScrollView>();
	Inner.Get()->SetWidth(FUiLength::Fixed(150));
	Inner.Get()->SetHeight(FUiLength::Fixed(100));
	Inner.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	REQUIRE(Root.AddChild(Content.Cast<DUiElement>(), Inner.Cast<DUiElement>()));
	auto InnerContent = Root.Create<DUiPanel>();
	InnerContent.Get()->SetHeight(FUiLength::Fixed(150));
	Inner.Get()->SetContent(InnerContent.Cast<DUiElement>());
	LayoutRoot(Root);
	auto Frame = PointerFrame(20, 20, false, false);
	Frame.Pointer.WheelNotches = 4;
	REQUIRE(Root.ProcessInput(Frame));
	REQUIRE(Inner.Get()->GetScrollOffset() == 50);
	REQUIRE(Outer.Get()->GetScrollOffset() == 50);
}

TEST("UI scroll thumb uses root capture and clamps at bottom")
{
	FUiRoot Root;
	auto Scroll = Root.Create<DUiScrollView>();
	Place(Root, Scroll, 200, 100);
	auto Content = Root.Create<DUiPanel>();
	Content.Get()->SetHeight(FUiLength::Fixed(1000));
	Scroll.Get()->SetContent(Content.Cast<DUiElement>());
	LayoutRoot(Root);
	const auto Thumb = Scroll.Get()->GetScrollThumbRect();
	REQUIRE(Root.ProcessInput(PointerFrame(Thumb.X + 2, Thumb.Y + 2, true, false)));
	REQUIRE(Root.GetCaptured() == Scroll.Get());
	REQUIRE(Root.ProcessInput(PointerFrame(Thumb.X + 2, 200, true, true)));
	REQUIRE(Scroll.Get()->GetScrollOffset() == 900);
	REQUIRE(Root.ProcessInput(PointerFrame(Thumb.X + 2, 200, false, true)));
	REQUIRE(Root.GetCaptured() == nullptr);
}

TEST("UI list virtualizes ten thousand items and preserves stable selection")
{
	FUiRoot Root;
	auto List = Root.Create<DUiListView>();
	List.Get()->SetRowHeight(20);
	List.Get()->SetItems(Items(10000));
	Place(Root, List, 200, 200);
	LayoutRoot(Root);
	REQUIRE(List.Get()->GetMaterializedRowCount() <= 12);
	REQUIRE(Root.GetStats().Elements < 40);
	Toolbox::uint64 Chosen = 0;
	auto Sub = List.Get()->OnSelectionChanged().Subscribe(
	    [&](Toolbox::uint64 Key)
	    {
		    Chosen = Key;
	    });
	Click(Root, 20, 10);
	REQUIRE(Chosen == 1);
	List.Get()->SetScrollOffset(2000);
	LayoutRoot(Root);
	Click(Root, 20, 10);
	REQUIRE(Chosen == 101);
	REQUIRE(List.Get()->GetSelectedKey() && *List.Get()->GetSelectedKey() == 101);
	const auto Count = Root.GetStats().Elements;
	for (Toolbox::int32 Index = 0; Index < 30; ++Index)
	{
		List.Get()->SetScrollOffset(Index * 100);
		LayoutRoot(Root);
	}
	REQUIRE(Root.GetStats().Elements == Count);
	auto Reordered = Items(10000);
	Toolbox::Swap(Reordered[0], Reordered[100]);
	List.Get()->SetItems(Toolbox::Move(Reordered));
	List.Get()->SetScrollOffset(0);
	LayoutRoot(Root);
	REQUIRE(List.Get()->GetSelectedKey() && *List.Get()->GetSelectedKey() == 101);
	List.Get()->SetItems(Items(50));
	LayoutRoot(Root);
	REQUIRE(!List.Get()->GetSelectedKey());
}

TEST("UI recycled list row cannot activate a former captured item")
{
	FUiRoot Root;
	auto List = Root.Create<DUiListView>();
	List.Get()->SetRowHeight(20);
	List.Get()->SetItems(Items(1000));
	Place(Root, List, 200, 100);
	LayoutRoot(Root);
	Toolbox::int32 Calls = 0;
	auto Sub = List.Get()->OnSelectionChanged().Subscribe(
	    [&](Toolbox::uint64)
	    {
		    ++Calls;
	    });
	REQUIRE(Root.ProcessInput(PointerFrame(20, 10, true, false)));
	List.Get()->SetScrollOffset(1000);
	LayoutRoot(Root);
	REQUIRE(Root.GetCaptured() == nullptr);
	REQUIRE(Root.ProcessInput(PointerFrame(20, 10, false, true)));
	REQUIRE(Calls == 0);
}

TEST("UI list rejects duplicate keys without changing visible data")
{
	FUiRoot Root;
	auto List = Root.Create<DUiListView>();
	List.Get()->SetItems(Items(3));
	auto Bad = Items(4);
	Bad[3].Key = 1;
	bool bThrew = false;
	try
	{
		List.Get()->SetItems(Toolbox::Move(Bad));
	}
	catch (const Toolbox::FException&)
	{
		bThrew = true;
	}
	REQUIRE(bThrew);
	REQUIRE(List.Get()->GetItemCount() == 3);
}

TEST("UI modal closes on cancel and restores previous focus without click through")
{
	FUiRoot Root;
	auto Back = Root.Create<DUiButton>("Start");
	Place(Root, Back, 100, 40);
	LayoutRoot(Root);
	REQUIRE(Root.SetFocus(Back.Cast<DUiElement>()));
	auto Popup = Root.Create<DUiPopup>();
	REQUIRE(Popup.Get()->Open());
	LayoutRoot(Root);
	REQUIRE(Root.GetTopModal() == Popup.Get());
	Toolbox::int32 BackCalls = 0;
	auto Sub = Back.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    ++BackCalls;
	    });
	Click(Root, 20, 20);
	REQUIRE(BackCalls == 0);
	const auto Result = Root.ProcessInput(NavigationFrame(EUiNavigationCommand::Cancel, true, false));
	REQUIRE(Result);
	REQUIRE(Result.Value().bModal);
	REQUIRE(!Popup.Get()->IsOpen());
	REQUIRE(Root.GetFocused() == Back.Get());
	REQUIRE(Popup.Get()->Open());
	Popup.Get()->Close();
	REQUIRE(Root.GetTopModal() == nullptr);
}

TEST("UI progress draws a value but never claims input")
{
	FUiRoot Root;
	auto Progress = Root.Create<DUiProgressBar>();
	Place(Root, Progress, 200, 20);
	Progress.Get()->SetValue(0.5);
	LayoutRoot(Root);
	REQUIRE(Root.HitTest({10, 10}) == nullptr);
	FUiDrawList List;
	REQUIRE(Root.BuildDrawList(List));
	bool bHalf = false;
	for (const auto& Item : List.GetItems())
	{
		bHalf = bHalf || (Item.SourceId == Progress.Get()->GetId() && Item.Rect.Width == 100);
	}
	REQUIRE(bHalf);
}
