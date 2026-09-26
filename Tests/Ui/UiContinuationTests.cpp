// SPDX-License-Identifier: NOASSERTION
#include "UiTestSupport.h"
#include "Dxf/UiButton.h"
#include "Toolbox/UniquePtr.h"
using namespace Dxf;
using namespace UiTest;
TEST("UI compiled button activates exactly once and survives reconnect")
{
	FUiRoot Root;
	auto Button = Root.Create<DUiButton>("Start");
	Button.Get()->SetWidth(FUiLength::Fixed(120));
	Button.Get()->SetHeight(FUiLength::Fixed(40));
	Button.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	Toolbox::int32 Calls = 0;
	auto Sub = Button.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    ++Calls;
	    });
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Button.Cast<DUiElement>()));
	LayoutRoot(Root);
	Click(Root, 20, 20);
	REQUIRE(Calls == 1);
	REQUIRE(Root.Remove(Button.Cast<DUiElement>()));
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Button.Cast<DUiElement>()));
	LayoutRoot(Root);
	Click(Root, 20, 20);
	REQUIRE(Calls == 2);
}

TEST("UI property publishes coherent value for each reentrant notification wave")
{
	TUiProperty<Toolbox::int32> Value(0);
	Toolbox::TVector<Toolbox::int32> Seen;
	auto First = Value.Subscribe(
	    [&](const Toolbox::int32& Number)
	    {
		    if (Number == 1)
		    {
			    Value.Set(2);
		    }
	    });
	auto Second = Value.Subscribe(
	    [&](const Toolbox::int32& Number)
	    {
		    Seen.PushBack(Number);
	    });
	Value.Set(1);
	REQUIRE(Seen.Size() == 2);
	REQUIRE(Seen[0] == 1);
	REQUIRE(Seen[1] == 2);
}

TEST("UI scope clears subscriptions before user cleanup may destroy the scope")
{
	TUiProperty<Toolbox::int32> Value(0);
	auto Scope = Toolbox::MakeUnique<FUiScope>();
	Toolbox::int32 Calls = 0;
	Scope->Add(Value.Subscribe(
	    [&](const Toolbox::int32&)
	    {
		    ++Calls;
	    }));
	Scope->AddCleanup(
	    [&]()
	    {
		    Value.Set(1);
		    // Clear must not access its owner after user cleanup.
		    Scope.Reset();
	    });
	Scope->Clear();
	REQUIRE(!Scope);
	REQUIRE(Calls == 0);
	REQUIRE(Value.GetSubscriberCount() == 0);
}

TEST("UI root may be destroyed from a button callback")
{
	auto Root = Toolbox::MakeUnique<FUiRoot>();
	const auto Life = Root->GetHandle();
	auto Button = Root->Create<DUiButton>("Close");
	Button.Get()->SetWidth(FUiLength::Fixed(120));
	Button.Get()->SetHeight(FUiLength::Fixed(40));
	Button.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	auto Sub = Button.Get()->OnClicked().Subscribe(
	    [&]()
	    {
		    Root.Reset();
	    });
	REQUIRE(Root->AddToLayer(EUiLayer::Normal, Button.Cast<DUiElement>()));
	LayoutRoot(*Root);
	REQUIRE(Root->ProcessInput(PointerFrame(20, 20, true, false)));
	const auto Result = Root->ProcessInput(PointerFrame(20, 20, false, true));
	REQUIRE(Result);
	REQUIRE(!Root);
	REQUIRE(Life.Get() == nullptr);
	REQUIRE(Button.Get() == nullptr);
}
