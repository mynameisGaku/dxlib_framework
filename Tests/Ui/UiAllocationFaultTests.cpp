// SPDX-License-Identifier: NOASSERTION
// 確保置換をこの実行ファイルだけに閉じ込める。成功試験の件数と故障注入位置の数を混同しない。
#include "UiTestSupport.h"
#include "Dxf/UiListView.h"
#include "Dxf/UiStyleResource.h"
#include "AllocationFault.h"
using namespace Dxf;
namespace
{
using Toolbox::Testing::SetAllocationFailureCountdown;
using Toolbox::Testing::WasAllocationFailureInjected;
} // namespace

TEST("UI style reload allocation failures preserve the complete previous style revision")
{
	FUiRoot Root;
	FUiStyleResource Resource;
	Resource.Attach(Root);
	const Toolbox::TVector<FUiStyleSource> Old = {{"old", "dxfui-style 1\nstyle Panel {\n background = #101020\n}\n"}};
	const Toolbox::TVector<FUiStyleSource> Next = {
	    {"next", "dxfui-style 1\nstyle Panel {\n background = #203010\n}\n"}};
	REQUIRE(Resource.Reload(Old));
	bool SeenFailure = false;
	for (Toolbox::int64 N = 0; N < 48; ++N)
	{
		const auto Revision = Resource.GetRevision();
		const auto Sheet = Resource.Get();
		SetAllocationFailureCountdown(N);
		const auto Result = Resource.Reload(Next);
		const bool Injected = WasAllocationFailureInjected();
		SetAllocationFailureCountdown(-1);
		if (Injected)
		{
			SeenFailure = true;
			REQUIRE(!Result);
			REQUIRE(Resource.GetRevision() == Revision);
			REQUIRE(Resource.Get().Get() == Sheet.Get());
		}
		else
		{
			REQUIRE(Result);
		}
	}
	REQUIRE(SeenFailure);
}

TEST("UI virtual row creation can recover after allocation failure without orphan row growth")
{
	for (Toolbox::int64 N = 0; N < 60; ++N)
	{
		FUiRoot Root;
		Root.SetSurface(UiTest::MakeSurface(320, 200, 200));
		auto List = Root.Create<DUiListView>();
		Toolbox::TVector<FUiListItem> Items;
		for (Toolbox::uint64 I = 0; I < 100; ++I)
		{
			Items.PushBack({I + 1, "row", ""});
		}
		List.Get()->SetItems(Toolbox::Move(Items));
		REQUIRE(Root.AddToLayer(EUiLayer::Normal, List.Cast<DUiElement>()));
		SetAllocationFailureCountdown(N);
		try
		{
			(void)Root.Layout();
		}
		catch (...)
		{
		}
		SetAllocationFailureCountdown(-1);
		REQUIRE(Root.Layout());
		REQUIRE(Root.Layout());
		const auto Rows = List.Get()->GetMaterializedRowCount();
		REQUIRE(List.Get()->GetChildCount() == Rows);
		if (Root.GetStats().Elements != 5 + Rows * 2)
		{
			Toolbox::Err << "allocation-position=" << N << " elements=" << Root.GetStats().Elements << " rows=" << Rows
			             << "\n";
		}
		REQUIRE(Root.GetStats().Elements == 5 + Rows * 2);
	}
}

TEST("UI creation allocation failure keeps registered element count and recovers")
{
	for (Toolbox::int64 N = 0; N < 12; ++N)
	{
		FUiRoot Root;
		const auto Before = Root.GetStats().Elements;
		TUiRef<DUiButton> Button;
		bool Threw = false;
		SetAllocationFailureCountdown(N);
		try
		{
			Button = Root.Create<DUiButton>("button");
		}
		catch (...)
		{
			Threw = true;
		}
		SetAllocationFailureCountdown(-1);
		if (Threw)
		{
			REQUIRE(Root.GetStats().Elements == Before);
		}
		else
		{
			REQUIRE(Button);
			REQUIRE(Root.Destroy(Button.Cast<DUiElement>()));
		}
		auto Later = Root.Create<DUiButton>();
		REQUIRE(Later);
	}
}
