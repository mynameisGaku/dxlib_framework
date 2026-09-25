// SPDX-License-Identifier: NOASSERTION
// U2: 文字の配置（折返し・省略・改行）と、型付きスタイル・スタイルの資源の読込。
#include "Dxf/UiDefaultStyles.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiStyleParser.h"
#include "Dxf/UiTextLayout.h"
#include "Ui/UiTestSupport.h"
using namespace UiTest;

namespace
{
// 失敗の文字列に含まれるか。
bool Contains(const Toolbox::FString& Text, const char* Part)
{
	const Toolbox::FString Needle(Part);
	if (Needle.Size() > Text.Size())
	{
		return false;
	}
	for (Toolbox::size_t Index = 0; Index + Needle.Size() <= Text.Size(); ++Index)
	{
		if (Text.Substr(Index, Needle.Size()) == Needle)
		{
			return true;
		}
	}
	return false;
}
// 一つの資源を読んだ失敗の文字列（成功なら空）。
Toolbox::FString ParseError(const char* Text)
{
	auto Result = ParseUiStyleSheet("Test.dxfui", Text);
	return Result ? Toolbox::FString() : Result.Error().Message;
}
} // namespace

TEST("UI text layout wraps, ellipsizes, keeps explicit lines and matches measured widths")
{
	FFixedWidthTextService Text;
	Text.LastPixelSize = 20;
	const FFont Font;
	// 改行は明示の行、幅の上限なしなら折り返さない。
	auto Plain = LayoutUiText(Text, Font, "Hello\nWorld!", {});
	REQUIRE(Plain && Plain.Value().Lines.Size() == 2 && Plain.Value().Width == 60 && Plain.Value().Height == 40);
	// ASCIIの語は空白で折り返す（幅100 = 10文字）。
	FUiTextLayoutRequest Wrap;
	Wrap.MaxWidth = 100;
	Wrap.Wrap = EUiTextWrap::Wrap;
	auto Words = LayoutUiText(Text, Font, "alpha beta gamma delta", Wrap);
	REQUIRE(Words);
	const auto& Lines = Words.Value().Lines;
	REQUIRE(Lines.Size() == 3 && Lines[0].Text == "alpha beta" && Lines[1].Text == "gamma" && Lines[2].Text == "delta");
	for (const auto& Line : Lines)
	{
		REQUIRE(Line.Width <= 100);
	}
	// 日本語は文字の境界で折り返す（全角20画素）。
	auto Japanese = LayoutUiText(Text, Font, "あいうえおかきくけこ", Wrap);
	REQUIRE(Japanese && Japanese.Value().Lines.Size() == 2 && Japanese.Value().Lines[0].Text == "あいうえお");
	// 幅を超える一語は文字で分ける。
	auto Long = LayoutUiText(Text, Font, "abcdefghijklmnopqrstuvwxyz", Wrap);
	REQUIRE(Long && Long.Value().Lines.Size() == 3 && Long.Value().Lines[0].Text == "abcdefghij");
	// 末尾の省略: 収まる最長の接頭辞＋省略記号（全角幅）。
	FUiTextLayoutRequest Ellipsis;
	Ellipsis.MaxWidth = 100;
	Ellipsis.Overflow = EUiTextOverflow::Ellipsis;
	auto Cut = LayoutUiText(Text, Font, "Settings and options", Ellipsis);
	REQUIRE(Cut && Cut.Value().bTruncated && !Cut.Value().bOverflowed);
	REQUIRE(Cut.Value().Lines[0].Text == "Settings\xE2\x80\xA6" && Cut.Value().Lines[0].Width == 100);
	// 行数の上限で切った最後の行には、収まっても省略記号を付ける。
	Wrap.Overflow = EUiTextOverflow::Ellipsis;
	Wrap.MaxLines = 2;
	auto Limited = LayoutUiText(Text, Font, "alpha beta gamma delta", Wrap);
	REQUIRE(Limited && Limited.Value().Lines.Size() == 2 && Limited.Value().bTruncated);
	REQUIRE(Limited.Value().Lines[1].Text == "gamma\xE2\x80\xA6");
	// 不正なUTF-8は失敗する。
	REQUIRE(!LayoutUiText(Text, Font, "\xFF\xFE", {}));
}

TEST("UI label measures with the font pixel size of the surface and draws the same lines")
{
	FFixedWidthTextService Text;
	FUiRootSettings Settings;
	Settings.Text = &Text;
	FUiRoot Root(Settings);
	auto Label = Root.Create<DUiLabel>("Hello UI");
	Label.Get()->SetAlign(EUiAlign::Start, EUiAlign::Start);
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Label.Cast<DUiElement>())));
	// 倍率1: 字の大きさ20 → 画素20、幅8文字×10=80。
	LayoutRoot(Root, MakeSurface(1280, 720, 720));
	REQUIRE(Text.LastPixelSize == 20 && Label.Get()->GetRect().Width == 80 && Label.Get()->GetRect().Height == 20);
	// 倍率2: 画素40で測り、論理単位では同じ大きさ。
	LayoutRoot(Root, MakeSurface(2560, 1440, 720));
	REQUIRE(Text.LastPixelSize == 40 && Label.Get()->GetRect().Width == 80 && Label.Get()->GetRect().Height == 20);
	FUiDrawList List;
	REQUIRE(static_cast<bool>(Root.BuildDrawList(List)));
	REQUIRE(List.GetItems().Size() == 1 && List.GetItems()[0].Kind == EUiDrawKind::Text);
	REQUIRE(List.GetItems()[0].Text == "Hello UI" && List.GetItems()[0].Rect.Width == 80);
	// 文字の変更は測り直しを要求する。
	Label.Get()->SetText("Hi");
	LayoutRoot(Root, MakeSurface(2560, 1440, 720));
	REQUIRE(Label.Get()->GetRect().Width == 20);
	// 割当の幅が狭ければ、その幅で折り返して描く。
	Label.Get()->SetText("alpha beta gamma");
	Label.Get()->SetTextLayout(EUiTextWrap::Wrap, EUiTextOverflow::Visible);
	Label.Get()->SetWidth(FUiLength::Fixed(60));
	LayoutRoot(Root, MakeSurface(1280, 720, 720));
	REQUIRE(Label.Get()->GetTextLayout().Lines.Size() == 3 && Label.Get()->GetRect().Height == 60);
}

TEST("UI style states resolve in order and element overrides apply last")
{
	FUiStyleSet Set;
	Set.Base.Background = {1, 1, 1, 255};
	Set.Hover.Background = FColor{2, 2, 2, 255};
	Set.Pressed.Background = FColor{3, 3, 3, 255};
	Set.Focus.BorderWidth = 2.0f;
	Set.Disabled.Background = FColor{4, 4, 4, 255};
	REQUIRE(Set.Resolve(EUiStyleState::Hover).Background.R == 2);
	REQUIRE(Set.Resolve(EUiStyleState::Hover | EUiStyleState::Pressed).Background.R == 3);
	REQUIRE(Set.Resolve(EUiStyleState::Hover | EUiStyleState::Disabled).Background.R == 4);
	REQUIRE(Set.Resolve(EUiStyleState::Focus).BorderWidth == 2);
	FUiRoot Root;
	auto Probe = Root.Create<DProbe>(10.0f, 10.0f);
	Probe.Get()->SetStyleId("Button");
	REQUIRE(static_cast<bool>(Root.AddToLayer(EUiLayer::Normal, Probe.Cast<DUiElement>())));
	LayoutRoot(Root);
	const FColor Raised = Root.GetStyleSheet().GetColorToken("color.surface.raised", {});
	REQUIRE(IsSameUiColor(Probe.Get()->GetStyle().Background, Raised));
	// ホバーで見た目が変わる（入力の仲介が状態を変える）。
	REQUIRE(static_cast<bool>(Root.ProcessInput(PointerFrame(5, 5, false, false))));
	REQUIRE(IsSameUiColor(Probe.Get()->GetStyle().Background,
	                      Root.GetStyleSheet().GetColorToken("color.surface.hover", {})));
	// 要素だけの上書きは最後に効く。
	FUiStylePatch Patch;
	Patch.Background = FColor{9, 8, 7, 255};
	Probe.Get()->SetStyleOverride(Patch);
	LayoutRoot(Root);
	REQUIRE(Probe.Get()->GetStyle().Background.R == 9);
	// 未知のスタイルIDは既定の定義を使う。
	Probe.Get()->SetStyleId("No.Such.Style");
	Probe.Get()->SetStyleOverride({});
	LayoutRoot(Root);
	REQUIRE(Probe.Get()->GetStyle().Background.A == 0);
}

TEST("UI style resources parse tokens and states and report source, line and key for every error kind")
{
	REQUIRE(GetBuiltInUiStyleSheet()->GetStyleCount() > 10 &&
	        GetBuiltInUiStyleSheet()->FindToken("color.accent") != nullptr);
	auto Parsed = ParseUiStyleSheet("Test.dxfui", "dxfui-style 1\n"
	                                              "# comment\n"
	                                              "token color.base = #102030\n"
	                                              "token color.alias = @color.base\n"
	                                              "token size.pad = 6\n"
	                                              "style Card {\n"
	                                              "    background = @color.alias\n"
	                                              "    padding = @size.pad 2\n"
	                                              "    font-family = \"Yu Gothic\"\n"
	                                              "    hover.background = #FFFFFF80\n"
	                                              "}\n");
	REQUIRE(Parsed);
	const FUiStyleSet* Card = Parsed.Value().Find("Card");
	REQUIRE(Card != nullptr && Card->Base.Background.G == 0x20 && Card->Base.Padding.Left == 6 &&
	        Card->Base.Padding.Top == 2);
	REQUIRE(Card->Base.FontFamily == "Yu Gothic" && Card->Hover.Background && Card->Hover.Background->A == 0x80);
	// 失敗の種類ごとに、資源名・行・キーを含む。
	REQUIRE(Contains(ParseError("style A {\n}\n"), "Test.dxfui:1: missing 'dxfui-style 1' header"));
	REQUIRE(Contains(ParseError("dxfui-style 2\n"), "unsupported style format version"));
	REQUIRE(Contains(ParseError("dxfui-style 1\nstyle A {\n}\nstyle A {\n}\n"),
	                 "Test.dxfui:4: key 'A': duplicate style id"));
	REQUIRE(
	    Contains(ParseError("dxfui-style 1\ntoken t = 1\ntoken t = 2\n"), "Test.dxfui:3: key 't': duplicate token"));
	REQUIRE(Contains(ParseError("dxfui-style 1\nstyle A {\n    background = @missing\n}\n"),
	                 "key 'missing': undefined token"));
	REQUIRE(Contains(ParseError("dxfui-style 1\ntoken a = @b\ntoken b = @a\n"), "token reference cycle"));
	REQUIRE(Contains(ParseError("dxfui-style 1\ntoken n = 4\nstyle A {\n    background = @n\n}\n"),
	                 "Test.dxfui:4: key 'background': token type mismatch"));
	REQUIRE(Contains(ParseError("dxfui-style 1\nstyle A {\n    shadow = 1\n}\n"),
	                 "Test.dxfui:3: key 'shadow': unknown key"));
	REQUIRE(
	    Contains(ParseError("dxfui-style 1\nstyle A {\n    font-size = 1e3\n}\n"), "key 'font-size': invalid number"));
	REQUIRE(Contains(ParseError("dxfui-style 1\nstyle A {\n    opacity = 2\n}\n"), "opacity must be between 0 and 1"));
	REQUIRE(Contains(ParseError("dxfui-style 1\nstyle A {\n    background = #12\n}\n"), "invalid color"));
	REQUIRE(
	    Contains(ParseError("dxfui-style 1\nstyle A {\n    padding = 1 2 3\n}\n"), "padding takes 1, 2 or 4 numbers"));
	REQUIRE(Contains(ParseError("dxfui-style 1\nstyle A {\n"), "block is not closed"));
	FUiStyleLimits Limits;
	Limits.MaxSourceBytes = 16;
	auto TooLarge = ParseUiStyleSheet("Big.dxfui", "dxfui-style 1\n# padding padding padding\n", Limits);
	REQUIRE(!TooLarge && Contains(TooLarge.Error().Message, "exceeds the size limit"));
	// 集約済みの資源は、元の資源名と行を示す。
	auto Bundle = ParseUiStyleSheet("Bundle.dxfui", "dxfui-style 1\n"
	                                                "source \"Tokens.dxfui\"\n"
	                                                "token c = #000000\n"
	                                                "source \"Button.dxfui\"\n"
	                                                "style B {\n"
	                                                "    border-width = -1\n"
	                                                "}\n");
	REQUIRE(!Bundle && Contains(Bundle.Error().Message, "Button.dxfui:2: key 'border-width'"));
	// 複数の資源をまとめて読み、IDとトークンは全体で一意であること。
	Toolbox::TVector<FUiStyleSource> Sources;
	Sources.PushBack({"A.dxfui", "dxfui-style 1\ntoken shared = 3\nstyle A {\n    border-width = @shared\n}\n"});
	Sources.PushBack({"B.dxfui", "dxfui-style 1\nstyle B {\n    border-width = @shared\n}\n"});
	auto Both = ParseUiStyleSheet(Sources);
	REQUIRE(Both && Both.Value().Find("B")->Base.BorderWidth == 3);
	Sources.PushBack({"C.dxfui", "dxfui-style 1\nstyle A {\n}\n"});
	auto Duplicate = ParseUiStyleSheet(Sources);
	REQUIRE(!Duplicate && Contains(Duplicate.Error().Message, "C.dxfui:2: key 'A': duplicate style id"));
}
