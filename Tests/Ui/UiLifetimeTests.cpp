// SPDX-License-Identifier: NOASSERTION
// U1: 要素の木・型付きの参照・寿命・購読のスコープ。
#include "Dxf/UiBinding.h"
#include "Dxf/UiSignal.h"
#include "Toolbox/Thread.h"
#include "Ui/UiTestSupport.h"
using namespace UiTest;

TEST("UI element create, add, remove and destroy keep the tree and handles consistent")
{
	FUiRoot Root;
	auto Parent = Root.Create<DProbe>();
	auto Child = Root.Create<DProbe>();
	REQUIRE(Parent.Get() != nullptr && Child.Get() != nullptr);
	REQUIRE(!Parent.Get()->IsAttached());
	REQUIRE(static_cast<bool>(Root.AddChild(Parent.Cast<DUiElement>(), Child.Cast<DUiElement>())));
	// 未接続の親に加えても接続しない。
	REQUIRE(Child.Get()->Attaches == 0 && !Child.Get()->IsAttached());
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Parent.Cast<DUiElement>())));
	REQUIRE(Parent.Get()->IsAttached() && Child.Get()->IsAttached());
	REQUIRE(Parent.Get()->FirstAttaches == 1 && Child.Get()->FirstAttaches == 1);
	// 取り外しは破棄ではない。
	REQUIRE(static_cast<bool>(Root.Remove(Parent.Cast<DUiElement>())));
	REQUIRE(Parent.Get() != nullptr && !Parent.Get()->IsAttached() && Child.Get()->Detaches == 1);
	REQUIRE(Parent.Get()->GetChildCount() == 1);
	const auto Before = Root.GetStats().Elements;
	auto* ChildRaw = Child.Get();
	REQUIRE(Root.Destroy(Parent.Cast<DUiElement>()));
	// 参照はすぐに解決しなくなり、解放は境界で行う。
	REQUIRE(Parent.Get() == nullptr && Child.Get() == nullptr);
	REQUIRE(Root.GetStats().PendingDestroy == 1 && Root.GetStats().Elements == Before);
	(void)ChildRaw;
	LayoutRoot(Root);
	REQUIRE(Root.GetStats().Elements == Before - 2 && Root.GetStats().PendingDestroy == 0);
}

TEST("UI tree rejects cycles, double parents, layers as children and excessive depth")
{
	FUiRootSettings Settings;
	Settings.Limits.MaxDepth = 4;
	FUiRoot Root(Settings);
	auto A = Root.Create<DProbe>();
	auto B = Root.Create<DProbe>();
	auto C = Root.Create<DProbe>();
	REQUIRE(static_cast<bool>(Root.AddChild(A.Cast<DUiElement>(), B.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddChild(B.Cast<DUiElement>(), C.Cast<DUiElement>())));
	// 循環・自分自身・二重の親。
	REQUIRE(!Root.AddChild(C.Cast<DUiElement>(), A.Cast<DUiElement>()));
	REQUIRE(!Root.AddChild(A.Cast<DUiElement>(), A.Cast<DUiElement>()));
	REQUIRE(!Root.AddChild(C.Cast<DUiElement>(), B.Cast<DUiElement>()));
	REQUIRE(!Root.AddChild(A.Cast<DUiElement>(), Root.GetLayer(EUiLayer::Popup).GetRef()));
	// 深さ: 領域(0)→A(1)→B(2)→C(3)、さらに2段で5になり上限4を超える。
	auto D = Root.Create<DProbe>();
	auto E = Root.Create<DProbe>();
	REQUIRE(static_cast<bool>(Root.AddChild(D.Cast<DUiElement>(), E.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, A.Cast<DUiElement>())));
	REQUIRE(!Root.AddChild(C.Cast<DUiElement>(), D.Cast<DUiElement>()));
	REQUIRE(D.Get()->GetParent() == nullptr && C.Get()->GetChildCount() == 0);
	// 別のルートの要素。
	FUiRoot Other;
	auto Foreign = Other.Create<DProbe>();
	REQUIRE(!Root.AddChild(A.Cast<DUiElement>(), Foreign.Cast<DUiElement>()));
	// 上限を超える要素数は例外で失敗する（正常な空表示と区別する）。
	FUiRootSettings Small;
	Small.Limits.MaxElements = 6;
	FUiRoot Limited(Small);
	(void)Limited.Create<DProbe>();
	(void)Limited.Create<DProbe>();
	bool bThrew = false;
	try
	{
		(void)Limited.Create<DProbe>();
	}
	catch (const Toolbox::FException&)
	{
		bThrew = true;
	}
	REQUIRE(bThrew);
}

TEST("UI attach and detach 100 times call hooks in order without duplicating subscriptions or children")
{
	FUiRoot Root;
	TUiProperty<Toolbox::int32> Value(1);
	auto Element = Root.Create<DProbe>();
	Toolbox::int32 Reflected = 0;
	Element.Get()->OnAttachHook = [&](DProbe& Self)
	{
		// 接続のたびに購読し、現在値を反映する。
		Reflected = Value.Get();
		Self.GetAttachScope().Add(Value.Subscribe(
		    [&](const Toolbox::int32& Next)
		    {
			    Reflected = Next;
		    }));
		if (Self.GetChildCount() == 0)
		{
			(void)Self.CreateChild<DProbe>();
		}
	};
	for (Toolbox::int32 Cycle = 0; Cycle < 100; ++Cycle)
	{
		REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Element.Cast<DUiElement>())));
		REQUIRE(Value.GetSubscriberCount() == 1);
		Value.Set(Cycle + 10);
		REQUIRE(Reflected == Cycle + 10);
		REQUIRE(static_cast<bool>(Root.Remove(Element.Cast<DUiElement>())));
		REQUIRE(Value.GetSubscriberCount() == 0);
		// 切断中の変更は反映しない。次の接続で現在値を反映する。
		Value.Set(-Cycle);
		REQUIRE(Reflected == Cycle + 10);
	}
	REQUIRE(Element.Get()->FirstAttaches == 1 && Element.Get()->Attaches == 100 && Element.Get()->Detaches == 100);
	REQUIRE(Element.Get()->GetChildCount() == 1);
	auto* Child = dynamic_cast<DProbe*>(Element.Get()->GetChild(0));
	REQUIRE(Child != nullptr && Child->Attaches == 100 && Child->Detaches == 100 && Child->FirstAttaches == 1);
}

TEST("UI attach failure rolls back the hooks and subscriptions of that attach and leaves the tree unchanged")
{
	FUiRoot Root;
	TUiProperty<Toolbox::int32> Value(0);
	auto Parent = Root.Create<DProbe>();
	auto Good = Root.Create<DProbe>();
	auto Bad = Root.Create<DProbe>();
	Good.Get()->OnAttachHook = [&](DProbe& Self)
	{
		Self.GetAttachScope().Add(Value.Subscribe(
		    [](const Toolbox::int32&)
		    {
		    }));
	};
	Bad.Get()->bThrowOnAttach = true;
	REQUIRE(static_cast<bool>(Root.AddChild(Parent.Cast<DUiElement>(), Good.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddChild(Parent.Cast<DUiElement>(), Bad.Cast<DUiElement>())));
	auto Added = Root.AddToLayer(EUiLayer::Normal, Parent.Cast<DUiElement>());
	REQUIRE(!Added);
	REQUIRE(Parent.Get()->GetParent() == nullptr && !Parent.Get()->IsAttached());
	REQUIRE(!Good.Get()->IsAttached() && Good.Get()->Detaches == 1 && Value.GetSubscriberCount() == 0);
	REQUIRE(!Bad.Get()->IsAttached() && Bad.Get()->GetAttachScope().IsEmpty());
	REQUIRE(Root.GetLayer(EUiLayer::Normal).GetChildCount() == 0);
	// 原因を取り除けば接続できる（初回のフックは一度だけ）。
	Bad.Get()->bThrowOnAttach = false;
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Parent.Cast<DUiElement>())));
	REQUIRE(Parent.Get()->FirstAttaches == 1 && Good.Get()->FirstAttaches == 1 && Value.GetSubscriberCount() == 1);
}

TEST("UI self and parent destruction during pointer dispatch stops delivery and frees at the boundary")
{
	FUiRoot Root;
	auto Panel = Root.Create<DProbe>(400.0f, 300.0f);
	auto Button = Root.Create<DProbe>(100.0f, 50.0f);
	REQUIRE(static_cast<bool>(Root.AddChild(Panel.Cast<DUiElement>(), Button.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Panel.Cast<DUiElement>())));
	LayoutRoot(Root);
	// ボタンは押下を処理済みにせず（親へ泡立つはずの出来事）、処理の中で親を破棄する。親へは届かない。
	Toolbox::int32 PanelDowns = 0;
	Panel.Get()->OnDown = [&](DProbe&)
	{
		++PanelDowns;
	};
	Button.Get()->bHandleDown = false;
	Button.Get()->OnDown = [&](DProbe& Self)
	{
		Root.Destroy(Self.GetParent()->GetRef());
		// 破棄の要求中も、この処理を終えるまで自身のメモリは有効。
		Self.SetName("still alive during the handler");
	};
	Click(Root, 10, 10);
	REQUIRE(Panel.Get() == nullptr && Button.Get() == nullptr);
	REQUIRE(PanelDowns == 0);
	REQUIRE(Root.GetStats().PendingDestroy == 0);
	REQUIRE(Root.GetCaptured() == nullptr && Root.GetHovered() == nullptr);
	// 自己破棄: 押下の処理で自分を破棄しても、以後の出来事は届かない。
	auto Self = Root.Create<DProbe>(100.0f, 50.0f);
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Self.Cast<DUiElement>())));
	LayoutRoot(Root);
	Self.Get()->OnDown = [&](DProbe& Target)
	{
		Root.Destroy(Target.GetRef());
	};
	Click(Root, 10, 10);
	REQUIRE(Self.Get() == nullptr);
	REQUIRE(Root.GetLayer(EUiLayer::Normal).GetChildCount() == 0);
}

TEST("UI stale handles do not resolve after the slot is reused")
{
	FUiRoot Root;
	auto First = Root.Create<DProbe>();
	const auto Stale = First;
	Root.Destroy(First.Cast<DUiElement>());
	LayoutRoot(Root);
	auto Second = Root.Create<DProbe>();
	REQUIRE(Second.Get() != nullptr);
	REQUIRE(Stale.Get() == nullptr);
	REQUIRE(Stale.GetId().Index == Second.GetId().Index && !(Stale.GetId() == Second.GetId()));
	// 旧世代の参照での操作は失敗する。
	REQUIRE(!Root.AddToLayer(EUiLayer::Normal, Stale.Cast<DUiElement>()));
	REQUIRE(!Root.Destroy(Stale.Cast<DUiElement>()));
}

TEST("UI root destruction detaches attached elements and releases subscriptions")
{
	TUiProperty<Toolbox::int32> Value(0);
	TUiRef<DProbe> Kept;
	Toolbox::int32 Detaches = 0;
	{
		FUiRoot Root;
		auto Element = Root.Create<DProbe>();
		Kept = Element;
		Element.Get()->OnAttachHook = [&](DProbe& Self)
		{
			Self.GetAttachScope().Add(Value.Subscribe(
			    [](const Toolbox::int32&)
			    {
			    }));
		};
		Element.Get()->GetAttachScope().AddCleanup(
		    [&]
		    {
			    ++Detaches;
		    });
		REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Element.Cast<DUiElement>())));
		REQUIRE(Value.GetSubscriberCount() == 1);
	}
	REQUIRE(Value.GetSubscriberCount() == 0);
	REQUIRE(Detaches == 1);
	REQUIRE(Kept.Get() == nullptr);
	// 発行元が先に破棄されても、購読トークンの解除は安全。
	FUiSubscription Orphan;
	{
		TUiProperty<Toolbox::int32> Short(0);
		Orphan = Short.Subscribe(
		    [](const Toolbox::int32&)
		    {
		    });
	}
	REQUIRE(Orphan.IsActive());
	Orphan.Reset();
	REQUIRE(!Orphan.IsActive());
}

TEST("UI property notification tolerates unsubscribe, subscribe and owner changes during notification")
{
	TUiProperty<Toolbox::int32> Value(0);
	Toolbox::int32 Calls = 0;
	FUiSubscription First;
	FUiSubscription Late;
	First = Value.Subscribe(
	    [&](const Toolbox::int32&)
	    {
		    ++Calls;
		    // 通知中に自分を解除し、新しい購読を加える（新しい購読にはこの通知を送らない）。
		    First.Reset();
		    Late = Value.Subscribe(
		        [&](const Toolbox::int32&)
		        {
			        Calls += 100;
		        });
	    });
	auto Second = Value.Subscribe(
	    [&](const Toolbox::int32&)
	    {
		    ++Calls;
	    });
	Value.Set(1);
	REQUIRE(Calls == 2);
	REQUIRE(Value.GetSubscriberCount() == 2);
	Value.Set(2);
	REQUIRE(Calls == 103);
	// 同じ値の代入では通知しない。
	Value.Set(2);
	REQUIRE(Calls == 103);
	// 通知中の値の変更は、この通知の後に続けて通知する（再入しない）。
	TUiProperty<Toolbox::int32> Chain(0);
	Toolbox::int32 Depth = 0;
	Toolbox::int32 MaxDepth = 0;
	Toolbox::int32 Seen = 0;
	auto Loop = Chain.Subscribe(
	    [&](const Toolbox::int32& Next)
	    {
		    ++Depth;
		    MaxDepth = Toolbox::Max(MaxDepth, Depth);
		    Seen = Next;
		    if (Next < 3)
		    {
			    Chain.Set(Next + 1);
		    }
		    --Depth;
	    });
	Chain.Set(1);
	REQUIRE(Seen == 3 && MaxDepth == 1);
	// 通知で発行元の所有者を破棄しても安全。
	auto* Owned = new TUiProperty<Toolbox::int32>(0);
	auto Killer = Owned->Subscribe(
	    [&](const Toolbox::int32&)
	    {
		    delete Owned;
		    Owned = nullptr;
	    });
	Owned->Set(5);
	REQUIRE(Owned == nullptr);
}

TEST("UI signal emission tolerates unsubscribe and destruction during emission")
{
	TUiSignal<Toolbox::int32> Signal;
	Toolbox::int32 Sum = 0;
	FUiSubscription A;
	A = Signal.Subscribe(
	    [&](Toolbox::int32 Value)
	    {
		    Sum += Value;
		    A.Reset();
	    });
	auto B = Signal.Subscribe(
	    [&](Toolbox::int32 Value)
	    {
		    Sum += Value * 10;
	    });
	Signal.Emit(1);
	Signal.Emit(2);
	REQUIRE(Sum == 1 + 10 + 20);
	REQUIRE(Signal.GetSubscriberCount() == 1);
}

TEST("UI post runs on the owner thread at the update boundary and is dropped after the root is destroyed")
{
	FUiPostHandle Handle;
	Toolbox::int32 Ran = 0;
	{
		FUiRoot Root;
		Handle = Root.GetPostHandle();
		// 別のスレッドから投函する。
		struct FContext
		{
			FUiPostHandle* pHandle;
			Toolbox::int32* pRan;
			bool bPosted;
		};
		FContext Context{&Handle, &Ran, false};
		Toolbox::FThread Worker;
		REQUIRE(Worker.Start(
		    [](void* Pointer)
		    {
			    auto* Data = static_cast<FContext*>(Pointer);
			    Toolbox::int32* Counter = Data->pRan;
			    Data->bPosted = Data->pHandle->Post(
			        [Counter]
			        {
				        ++*Counter;
			        });
		    },
		    &Context));
		Worker.Join();
		REQUIRE(Context.bPosted);
		REQUIRE(Ran == 0);
		REQUIRE(static_cast<bool>(Root.Update(0.016)));
		REQUIRE(Ran == 1);
		// 破棄済みの要素を対象とする処理は、参照で確かめて何もしない。
		auto Target = Root.Create<DProbe>();
		Root.Destroy(Target.Cast<DUiElement>());
		REQUIRE(Handle.Post(
		    [&, Target]
		    {
			    Ran += Target.Get() == nullptr ? 10 : 1000;
		    }));
		REQUIRE(static_cast<bool>(Root.Update(0.016)));
		REQUIRE(Ran == 11);
		REQUIRE(Handle.Post(
		    [&]
		    {
			    Ran += 1000;
		    }));
	}
	// ルートの破棄で積まれた処理は実行せずに捨て、以後は積めない。
	REQUIRE(Ran == 11);
	REQUIRE(!Handle.Post(
	    [&]
	    {
		    Ran += 1000;
	    }));
}
