// SPDX-License-Identifier: NOASSERTION
// 再配置されたUI層だけで部品・購読・仮想化を使う。Nativeの字体計測は別の構成で確認する。
#include "Dxf/UiRoot.h"
#include "Toolbox/Platform.h"
#include "Dxf/UiToggle.h"
#include "Dxf/UiListView.h"
#include "Dxf/UiProperty.h"
#include "Dxf/UiInspection.h"
int main()
{
	using namespace Dxf;
	FUiRoot Root;
	Root.SetSurface(FUiSurface({0, 0, 640, 480}, {}));
	auto Toggle = Root.Create<DUiToggle>("choice");
	Toggle.Get()->SetWidth(FUiLength::Fixed(200));
	Toggle.Get()->SetHeight(FUiLength::Fixed(40));
	if (!Root.AddToLayer(EUiLayer::Normal, Toggle.Cast<DUiElement>()))
	{
		return 1;
	}
	Toolbox::int32 Changes = 0;
	auto Subscription = Toggle.Get()->OnValueChanged().Subscribe(
	    [&](bool)
	    {
		    ++Changes;
	    });
	Toggle.Get()->SetValue(true);
	if (Changes != 0 || !Toggle.Get()->GetValue())
	{
		return 2;
	}
	auto List = Root.Create<DUiListView>();
	Toolbox::TVector<FUiListItem> Items;
	for (Toolbox::uint64 I = 0; I < 10000; ++I)
	{
		Items.PushBack({I + 1, "row", "details"});
	}
	List.Get()->SetItems(Toolbox::Move(Items));
	List.Get()->SetAbsolutePosition({220, 0});
	List.Get()->SetWidth(FUiLength::Fixed(300));
	List.Get()->SetHeight(FUiLength::Fixed(200));
	if (!Root.AddToLayer(EUiLayer::Normal, List.Cast<DUiElement>()) || !Root.Layout())
	{
		return 3;
	}
	List.Get()->SetSelectedKey(Toolbox::uint64(123));
	if (List.Get()->GetMaterializedRowCount() > 20 || *List.Get()->GetSelectedKey() != 123)
	{
		return 4;
	}
	const auto Before = Root.GetStats().Layout.Measures;
	if (!Root.Layout() || Root.GetStats().Layout.Measures != Before)
	{
		return 5;
	}
	if (!Root.Destroy(Toggle.Cast<DUiElement>()) || Toggle)
	{
		return 6;
	}
	Toolbox::Out << "UI_CONSUMER_PASSED\n";
	return 0;
}
