// SPDX-License-Identifier: NOASSERTION
// W4: 検査結果の出所（世代付きの要素の番号・木の位置）と、命令のクリップと要素のクリップの対応。
#include "Ui/UiTestSupport.h"
#include "Dxf/UiInspection.h"
#include "Dxf/UiPanel.h"
using namespace UiTest;

namespace
{
// 名前付きの固定寸法の色の部品。
TUiRef<DUiPanel> MakePanel(FUiRoot& Root, const char* Name, Toolbox::f32 Width, Toolbox::f32 Height)
{
	auto Panel = Root.Create<DUiPanel>(EUiStackMode::Overlay);
	Panel.Get()->SetName(Name);
	Panel.Get()->SetWidth(FUiLength::Fixed(Width));
	Panel.Get()->SetHeight(FUiLength::Fixed(Height));
	Panel.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	FUiStylePatch Style;
	Style.Background = FColor{10, 20, 30, 255};
	Panel.Get()->SetStyleOverride(Style);
	return Panel;
}

// ClipFitの問題だけを数える。
Toolbox::size_t CountClipFit(const FUiInspectionResult& Result)
{
	Toolbox::size_t Count = 0;
	for (const auto& Issue : Result.Issues)
	{
		Count += Issue.Kind == EUiInspectionKind::ClipFit ? 1 : 0;
	}
	return Count;
}
} // namespace

TEST("UI ClipFit maps each draw item to its source element clip and reports a generation-safe source")
{
	FUiRoot Root;
	auto Frame = MakePanel(Root, "Frame", 100, 60);
	Frame.Get()->SetClipChildren(true);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Frame.Cast<DUiElement>()));
	auto Wide = MakePanel(Root, "Wide", 300, 40);
	Wide.Get()->DeclareCheckAllowance(EUiCheckAllowance::Overlap, "fixture");
	REQUIRE(Root.AddChild(Frame.Cast<DUiElement>(), Wide.Cast<DUiElement>()));
	LayoutRoot(Root);
	FUiDrawList List;
	REQUIRE(Root.BuildDrawList(List));
	// 切り抜かれた子の命令は、親の矩形のクリップを持つ（正しい命令は問題にならない）。
	const auto Clean = InspectUiLayout(Root, &List);
	REQUIRE(CountClipFit(Clean) == 0);
	REQUIRE(Clean.Scale == 1 && Clean.LogicalSize.Width == 1280 && Clean.SurfacePixels.Right == 1280);
	// 子の命令のクリップを表示面全体へ広げる（クリップを無視した描画の再現）。
	Toolbox::size_t WideIndex = List.GetItems().Size();
	for (Toolbox::size_t Index = 0; Index < List.GetItems().Size(); ++Index)
	{
		if (List.GetItems()[Index].SourceId == Wide.Get()->GetId())
		{
			WideIndex = Index;
		}
	}
	REQUIRE(WideIndex < List.GetItems().Size());
	List.EditItems()[WideIndex].Clip = {0, 0, 1280, 720};
	const auto Broken = InspectUiLayout(Root, &List);
	REQUIRE(CountClipFit(Broken) == 1);
	const FUiInspectionIssue* Issue = nullptr;
	for (const auto& Each : Broken.Issues)
	{
		if (Each.Kind == EUiInspectionKind::ClipFit)
		{
			Issue = &Each;
		}
	}
	REQUIRE(Issue != nullptr && Issue->Severity == EUiInspectionSeverity::Error);
	REQUIRE(Issue->ElementId == Wide.Get()->GetId() && Issue->Source == Wide.GetId());
	REQUIRE(Issue->DrawItemIndex == WideIndex);
	REQUIRE(Issue->Expected == Frame.Get()->GetRect() && Issue->Actual.Width == 1280);
	REQUIRE(Issue->Path == Root.GetLayer(EUiLayer::Normal).GetName() + "/Frame[0]/Wide[0]");
	// 要素を破棄して同じ枠を再使用しても、以前の出所とは一致しない。
	const FObjectId Stale = Issue->Source;
	Root.Destroy(Wide.Cast<DUiElement>());
	LayoutRoot(Root);
	auto Reused = MakePanel(Root, "Reused", 10, 10);
	REQUIRE(!(Reused.GetId() == Stale));
	// 出所の要素がない命令は、要素へ対応させずに報告する。
	FUiDrawList Orphan;
	FUiDrawItem Item;
	Item.Clip = {0, 0, 10, 10};
	Item.SourceId = 999999;
	Orphan.Add(Item);
	const auto Unknown = InspectUiLayout(Root, &Orphan);
	REQUIRE(CountClipFit(Unknown) == 1 && Unknown.Issues[0].DrawItemIndex == 0);
	REQUIRE(Unknown.Issues[0].ElementId == 999999 && Unknown.Issues[0].Path == "DrawList");
}

TEST("UI inspection target clip and display id are carried into the result")
{
	FUiRoot Root;
	auto Box = MakePanel(Root, "Box", 200, 100);
	REQUIRE(Root.AddToLayer(EUiLayer::Normal, Box.Cast<DUiElement>()));
	LayoutRoot(Root);
	FUiDrawList List;
	REQUIRE(Root.BuildDrawList(List));
	FUiInspectionTarget Target;
	Target.DisplayId = 42;
	Target.DisplayClip = {0, 0, 50, 50};
	// 表示先のクリップを掛けていない命令は、表示先のクリップを越える。
	const auto Unclipped = InspectUiLayout(Root, &List, 4096, Target);
	REQUIRE(Unclipped.DisplayId == 42 && CountClipFit(Unclipped) >= 1 && Unclipped.Issues[0].DisplayId == 42);
	for (auto& Each : List.EditItems())
	{
		Each.Clip = Each.Clip.Intersect(Target.DisplayClip);
	}
	REQUIRE(CountClipFit(InspectUiLayout(Root, &List, 4096, Target)) == 0);
}
