// SPDX-License-Identifier: NOASSERTION
// U2: レイアウト（測定と配置の分離・変更部分だけの再計算）・表示面の倍率。
#include "Dxf/UiLabel.h"
#include "Dxf/UiStack.h"
#include "Ui/UiTestSupport.h"
using namespace UiTest;

namespace
{
// 内容の大きさを固定で返す要素。
class DContent final : public DUiElement
{
public:
	DContent(Toolbox::f32 Width, Toolbox::f32 Height) : m_Size{Width, Height}
	{
	}
	// 測った回数。
	Toolbox::int32 Measures = 0;
	// 内容の大きさを変える。
	void Resize(Toolbox::f32 Width, Toolbox::f32 Height)
	{
		m_Size = {Width, Height};
		InvalidateMeasure();
	}

protected:
	FUiSize OnMeasure(FUiLayoutContext&, FUiSize) override
	{
		++Measures;
		return m_Size;
	}

private:
	FUiSize m_Size;
};
// 矩形が一致するか（誤差1e-4）。
bool Near(const FUiRect& A, Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Width, Toolbox::f32 Height)
{
	auto Close = [](Toolbox::f32 P, Toolbox::f32 Q)
	{
		return Toolbox::Abs(P - Q) < 1e-4f;
	};
	return Close(A.X, X) && Close(A.Y, Y) && Close(A.Width, Width) && Close(A.Height, Height);
}
// 入れ物を作る。
TUiRef<DUiElement> Container(FUiRoot& Root, EUiStackMode Mode, Toolbox::f32 Gap = 0)
{
	return Root.Create<DUiStack>(Mode, Gap).Cast<DUiElement>();
}
} // namespace

TEST("UI vertical stack places fixed, content and fill children with gap, margin, padding and alignment")
{
	FUiRoot Root;
	auto Stack = Container(Root, EUiStackMode::Vertical, 10);
	Stack.Get()->SetPadding(FUiThickness::All(20));
	Stack.Get()->SetWidth(FUiLength::Fixed(400));
	Stack.Get()->SetHeight(FUiLength::Fixed(600));
	Stack.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	auto Fixed = Root.Create<DContent>(50.0f, 50.0f);
	Fixed.Get()->SetHeight(FUiLength::Fixed(100));
	Fixed.Get()->SetAlign(EUiAlign::Center, EUiAlign::Start);
	auto Content = Root.Create<DContent>(120.0f, 40.0f);
	Content.Get()->SetMargin({5, 6, 7, 8});
	Content.Get()->SetAlign(EUiAlign::End, EUiAlign::Start);
	auto Fill = Root.Create<DContent>(10.0f, 10.0f);
	Fill.Get()->SetHeight(FUiLength::Fill());
	REQUIRE(static_cast<bool>(Root.AddChild(Stack, Fixed.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddChild(Stack, Content.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddChild(Stack, Fill.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Stack)));
	LayoutRoot(Root);
	REQUIRE(Near(Stack.Get()->GetRect(), 0, 0, 400, 600));
	// 内容範囲は(20,20)-(380,580)。固定の子は幅50（内容）を中央に。
	REQUIRE(Near(Fixed.Get()->GetRect(), 20 + (360 - 50) * 0.5f, 20, 50, 100));
	// 内容の子は余白込みで高さ54、右寄せ（右余白7）。
	REQUIRE(Near(Content.Get()->GetRect(), 380 - 7 - 120, 20 + 100 + 10 + 6, 120, 40));
	// 残り: 560 - 100 - 54 - 20(間隔2つ) = 386 をFillへ。
	REQUIRE(Near(Fill.Get()->GetRect(), 20, 20 + 100 + 10 + 54 + 10, 360, 386));
}

TEST("UI horizontal stack shares the remaining space by fill weights and overlay aligns and places absolutely")
{
	FUiRoot Root;
	auto Row = Container(Root, EUiStackMode::Horizontal, 0);
	Row.Get()->SetWidth(FUiLength::Fixed(300));
	Row.Get()->SetHeight(FUiLength::Fixed(50));
	Row.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	auto A = Root.Create<DContent>(0.0f, 0.0f);
	A.Get()->SetWidth(FUiLength::Fill(1));
	auto B = Root.Create<DContent>(0.0f, 0.0f);
	B.Get()->SetWidth(FUiLength::Fill(2));
	REQUIRE(static_cast<bool>(Root.AddChild(Row, A.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddChild(Row, B.Cast<DUiElement>())));
	auto Over = Container(Root, EUiStackMode::Overlay);
	Over.Get()->SetWidth(FUiLength::Fixed(200));
	Over.Get()->SetHeight(FUiLength::Fixed(100));
	Over.Get()->SetAlign(EUiAlign::End, EUiAlign::End);
	auto Centered = Root.Create<DContent>(40.0f, 20.0f);
	Centered.Get()->SetAlign(EUiAlign::Center, EUiAlign::Center);
	auto Placed = Root.Create<DContent>(10.0f, 10.0f);
	Placed.Get()->SetAbsolutePosition({30, 40});
	REQUIRE(static_cast<bool>(Root.AddChild(Over, Centered.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddChild(Over, Placed.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Row)));
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Over)));
	LayoutRoot(Root);
	REQUIRE(Near(A.Get()->GetRect(), 0, 0, 100, 50));
	REQUIRE(Near(B.Get()->GetRect(), 100, 0, 200, 50));
	REQUIRE(Near(Over.Get()->GetRect(), 1080, 620, 200, 100));
	REQUIRE(Near(Centered.Get()->GetRect(), 1080 + 80, 620 + 40, 40, 20));
	REQUIRE(Near(Placed.Get()->GetRect(), 1110, 660, 10, 10));
}

TEST(
    "UI size limits clamp, hidden keeps space and collapsed removes it, content parents treat fill children as content")
{
	FUiRoot Root;
	auto Column = Container(Root, EUiStackMode::Vertical);
	Column.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	auto Big = Root.Create<DContent>(500.0f, 500.0f);
	Big.Get()->SetSizeLimits({0, 0}, {100, 80});
	auto Small = Root.Create<DContent>(5.0f, 5.0f);
	Small.Get()->SetSizeLimits({30, 30}, {UiUnbounded, UiUnbounded});
	auto Filler = Root.Create<DContent>(60.0f, 25.0f);
	Filler.Get()->SetHeight(FUiLength::Fill());
	REQUIRE(static_cast<bool>(Root.AddChild(Column, Big.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddChild(Column, Small.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddChild(Column, Filler.Cast<DUiElement>())));
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Column)));
	LayoutRoot(Root);
	REQUIRE(Big.Get()->GetRect().Width == 100 && Big.Get()->GetRect().Height == 80);
	REQUIRE(Small.Get()->GetRect().Height == 30);
	// 内容の大きさで決まる親の中のFillは、内容の大きさ（25）として扱う（循環しない決定規則）。
	REQUIRE(Column.Get()->GetRect().Height == 80 + 30 + 25);
	REQUIRE(Filler.Get()->GetRect().Height == 25);
	Small.Get()->SetVisibility(EUiVisibility::Hidden);
	LayoutRoot(Root);
	REQUIRE(Column.Get()->GetRect().Height == 80 + 30 + 25 && Filler.Get()->GetRect().Y == 110);
	Small.Get()->SetVisibility(EUiVisibility::Collapsed);
	LayoutRoot(Root);
	REQUIRE(Column.Get()->GetRect().Height == 80 + 25 && Filler.Get()->GetRect().Y == 80);
	REQUIRE(Small.Get()->GetRect().Width == 0);
}

TEST("UI invalid layout values are reported and excluded from drawing and hit testing instead of shown as empty")
{
	FUiRoot Root;
	auto Bad = Root.Create<DProbe>(100.0f, 100.0f);
	Bad.Get()->SetWidth(FUiLength::Fixed(Toolbox::TNumericLimits<Toolbox::f32>::QuietNaN()));
	auto Negative = Root.Create<DProbe>(100.0f, 100.0f);
	Negative.Get()->SetPadding({-1, 0, 0, 0});
	auto Inverted = Root.Create<DProbe>(100.0f, 100.0f);
	Inverted.Get()->SetSizeLimits({50, 50}, {10, 10});
	auto Good = Root.Create<DProbe>(100.0f, 100.0f);
	DUiElement* Elements[] = {Bad.Get(), Negative.Get(), Inverted.Get(), Good.Get()};
	for (DUiElement* Element : Elements)
	{
		REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Element->GetRef())));
	}
	LayoutRoot(Root);
	REQUIRE(Root.GetLayoutErrors().Size() == 3);
	REQUIRE(Root.GetStats().LayoutErrors == 3);
	// 不正な要素は受けず、正常な要素（前面の最後）が受ける。
	REQUIRE(Root.HitTest({10, 10}) == Good.Get());
	Good.Get()->SetVisibility(EUiVisibility::Collapsed);
	LayoutRoot(Root);
	REQUIRE(Root.HitTest({10, 10}) == nullptr);
}

TEST("UI layout recomputes only changed parts and skips a static tree")
{
	FUiRoot Root;
	auto Column = Container(Root, EUiStackMode::Vertical);
	Toolbox::TVector<TUiRef<DContent>> Items;
	for (Toolbox::int32 Index = 0; Index < 50; ++Index)
	{
		auto Item = Root.Create<DContent>(100.0f, 20.0f);
		REQUIRE(static_cast<bool>(Root.AddChild(Column, Item.Cast<DUiElement>())));
		Items.PushBack(Item);
	}
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Column)));
	LayoutRoot(Root);
	const auto First = Root.GetStats().Layout;
	REQUIRE(First.Passes == 1);
	// 変更がなければ何もしない。
	for (Toolbox::int32 Frame = 0; Frame < 10; ++Frame)
	{
		LayoutRoot(Root);
	}
	REQUIRE(Root.GetStats().Layout.Measures == First.Measures && Root.GetStats().Layout.Passes == 1);
	// 一つの要素の変更は、その要素と祖先だけを測り直す。
	Items[20].Get()->Resize(100, 30);
	LayoutRoot(Root);
	const auto After = Root.GetStats().Layout;
	REQUIRE(After.Passes == 2);
	REQUIRE(After.Measures - First.Measures == 1 + 1 + 1);
	REQUIRE(Items[20].Get()->Measures == 2 && Items[19].Get()->Measures == 1);
	// 後ろの要素は位置だけ変わる。
	REQUIRE(Items[21].Get()->GetRect().Y == 20 * 20 + 30);
	// 表示面が変わると全体を計算し直す。
	LayoutRoot(Root, MakeSurface(1920, 1080, 720));
	REQUIRE(Items[19].Get()->Measures == 2);
}

TEST("UI surface scale follows the reference height, keeps pixel edges contiguous and handles unusual aspect ratios")
{
	// 高さ基準: 基準720で2560x1440なら倍率2、論理1280x720。
	const FUiSurface Double = MakeSurface(2560, 1440, 720);
	REQUIRE(Double.IsDisplayable() && Double.GetScale() == 2 && Double.GetLogicalSize().Width == 1280);
	// 16:10・21:9は横の論理幅が変わる。
	REQUIRE(MakeSurface(1280, 800, 800).GetLogicalSize().Width == 1280);
	REQUIRE(Toolbox::Abs(MakeSurface(2560, 1080, 720).GetLogicalSize().Width - 2560.0f / 1.5f) < 1e-3f);
	REQUIRE(Toolbox::Abs(MakeSurface(1920, 1200, 720).GetLogicalSize().Width - 1152) < 1e-3f);
	// 奇数幅・端数の倍率でも、隣り合う矩形の画素は重ならず隙間もない（半開区間・切り捨て）。
	const FUiSurface Odd = MakeSurface(1279, 719, 720);
	const Toolbox::f32 Third = Odd.GetLogicalSize().Width / 3;
	const auto Left = Odd.ToPixel(FUiRect{0, 0, Third, 10});
	const auto Middle = Odd.ToPixel(FUiRect{Third, 0, Third, 10});
	const auto Right = Odd.ToPixel(FUiRect{Third * 2, 0, Odd.GetLogicalSize().Width - Third * 2, 10});
	REQUIRE(Left.Right == Middle.Left && Middle.Right == Right.Left && Right.Right <= 1279);
	// Viewport（原点が画面の途中）では同じ規則で、領域の原点へずらす。
	FUiScaleSettings Settings;
	const FUiSurface Viewport({640, 0, 1280, 360}, Settings);
	REQUIRE(Viewport.GetScale() == 0.5f && Viewport.GetLogicalSize().Width == 1280);
	REQUIRE(Viewport.ToPixel(FVector2{100, 100}).X == 690 && Viewport.ToLogical({690, 50}).Y == 100);
	// 固定ピクセル: 倍率は利用者倍率そのもの。
	Settings.Mode = EUiScaleMode::FixedPixel;
	Settings.UserScale = 1.5f;
	const FUiSurface Fixed({0, 0, 300, 150}, Settings);
	REQUIRE(Fixed.GetScale() == 1.5f && Fixed.GetLogicalSize().Width == 200);
	// 大きさ0の期間は表示できない（レイアウト・入力・描画をしない）。
	const FUiSurface Empty({0, 0, 0, 720}, FUiScaleSettings{});
	REQUIRE(!Empty.IsDisplayable());
	FUiRoot Root;
	auto Probe = Root.Create<DProbe>(10.0f, 10.0f);
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Probe.Cast<DUiElement>())));
	LayoutRoot(Root, Empty);
	REQUIRE(Root.GetStats().Layout.Passes == 0 && Root.HitTest({1, 1}) == nullptr);
	FUiDrawList List;
	REQUIRE(static_cast<bool>(Root.BuildDrawList(List)) && List.GetItems().IsEmpty());
	// 狭いViewportで表示できるようになったら通常どおり配置する。
	LayoutRoot(Root, MakeSurface(200, 100, 720));
	REQUIRE(Root.GetStats().Layout.Passes == 1 && Root.HitTest({5, 5}) == Probe.Get());
}
