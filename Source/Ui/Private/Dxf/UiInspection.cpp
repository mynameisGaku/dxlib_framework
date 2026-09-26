// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiInspection.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiImage.h"
#include "Dxf/UiDefaultStyles.h"
#include "Toolbox/Map.h"
namespace Dxf
{
namespace
{
// 描画項目の出所と照らし合わせる、要素ごとの期待するクリップ。
struct FElementClip
{
	const DUiElement* Element = nullptr;
	Toolbox::FString Path;
	FUiRect Clip;
};

// 検査の途中の状態。
struct FInspectState
{
	FUiInspectionResult& Result;
	Toolbox::size_t Limit;
	Toolbox::TMap<Toolbox::uint64, FElementClip>& Clips;
};

// 正の面積を持つ内側の矩形か。f32のレイアウト丸めとして小さい余裕を持つ。
bool Contains_Internal(const FUiRect& Outer, const FUiRect& Inner)
{
	const Toolbox::f64 Tolerance = 1e-4;
	return Inner.X >= Outer.X - Tolerance && Inner.Y >= Outer.Y - Tolerance &&
	       static_cast<Toolbox::f64>(Inner.X) + Inner.Width <=
	           static_cast<Toolbox::f64>(Outer.X) + Outer.Width + Tolerance &&
	       static_cast<Toolbox::f64>(Inner.Y) + Inner.Height <=
	           static_cast<Toolbox::f64>(Outer.Y) + Outer.Height + Tolerance;
}
// 警告も理由付き免除を記録する。上限超過は通常の成功としない。
void Add_Internal(FUiInspectionResult& Result, const DUiElement& Element, const Toolbox::FString& Path,
                  EUiInspectionKind Kind, FUiRect Expected, FUiRect Actual, const char* Reason, Toolbox::size_t Limit)
{
	EUiCheckAllowance Allow = EUiCheckAllowance::None;
	if (Kind == EUiInspectionKind::TextFit)
	{
		Allow = EUiCheckAllowance::TextOverflow;
	}
	if (Kind == EUiInspectionKind::LayoutFit || Kind == EUiInspectionKind::ClipFit)
	{
		Allow = EUiCheckAllowance::LayoutOverflow;
	}
	if (Kind == EUiInspectionKind::Overlap)
	{
		Allow = EUiCheckAllowance::Overlap;
	}
	if (Kind == EUiInspectionKind::ImageResolution)
	{
		Allow = EUiCheckAllowance::ImageResolution;
	}
	if (Allow != EUiCheckAllowance::None && Element.HasCheckAllowance(Allow) &&
	    !Element.GetCheckAllowanceReason().IsEmpty())
	{
		++Result.DeclaredExemptions;
		return;
	}
	if (Result.Issues.Size() >= Limit)
	{
		throw Toolbox::FException("UI inspection issue limit exceeded");
	}
	FUiInspectionIssue Issue;
	Issue.Kind = Kind;
	Issue.ElementId = Element.GetId();
	Issue.Source = Element.GetRef().GetId();
	Issue.DisplayId = Result.DisplayId;
	Issue.Path = Path;
	Issue.Expected = Expected;
	Issue.Actual = Actual;
	Issue.Reason = Reason;
	// 計算できない配置と、命令が要素のクリップを越える描画の誤りはエラー。
	Issue.Severity = Kind == EUiInspectionKind::InvalidLayout || Kind == EUiInspectionKind::ClipFit
	                     ? EUiInspectionSeverity::Error
	                     : EUiInspectionSeverity::Warning;
	Result.Issues.PushBack(Toolbox::Move(Issue));
}
// 検査はコールバックを呼ばず、実配置を値として読む。
// Clipは祖先から求めた、この要素の命令に掛かるはずのクリップ。
void Visit_Internal(FUiRoot& Root, const DUiElement& Element, const Toolbox::FString& Path, const FUiRect& Clip,
                    FInspectState& Inspect)
{
	if (!Element.IsAttached() || Element.IsDestroyRequested() || !Element.IsVisibleInTree())
	{
		return;
	}
	FUiInspectionResult& Result = Inspect.Result;
	const Toolbox::size_t Limit = Inspect.Limit;
	++Result.Elements;
	Inspect.Clips[Element.GetId()] = FElementClip{&Element, Path, Clip};
	// 子へは、子を切り抜く要素なら自身の矩形との共通部分を渡す。
	const FUiRect ChildClip = Element.IsClippingChildren() ? Clip.Intersect(Element.GetRect()) : Clip;
	const auto& Rect = Element.GetRect();
	const auto* Parent = Element.GetParent();
	if (Parent != nullptr)
	{
		// スクロールなど意図的に切り抜く親は、子全体が見えるとは要求しない。
		if (!Parent->IsClippingChildren() && !Contains_Internal(Parent->GetContentRect(), Rect))
		{
			Add_Internal(Result, Element, Path, EUiInspectionKind::LayoutFit, Parent->GetContentRect(), Rect,
			             "child outside parent without scrolling or clipping", Limit);
		}
	}
	if (!Element.GetStyleId().IsEmpty() && Root.GetStyleSheet().Find(Element.GetStyleId()) == nullptr &&
	    GetBuiltInUiStyleSheet()->Find(Element.GetStyleId()) == nullptr)
	{
		Add_Internal(Result, Element, Path, EUiInspectionKind::UnknownStyle, {}, Rect, "unknown style ID", Limit);
	}
	if (const auto* Label = dynamic_cast<const DUiLabel*>(&Element))
	{
		const auto& Text = Label->GetTextLayout();
		const auto Content = Label->GetContentRect();
		const Toolbox::f32 Scale = Label->GetLayoutScale();
		if (Scale > 0)
		{
			const FUiRect TextRect{Content.X, Content.Y, static_cast<Toolbox::f32>(Text.Width) / Scale,
			                       static_cast<Toolbox::f32>(Text.Height) / Scale};
			if (Text.bOverflowed || !Contains_Internal(Content, TextRect))
			{
				Add_Internal(Result, Element, Path, EUiInspectionKind::TextFit, Content, TextRect,
				             "measured text does not fit", Limit);
			}
		}
	}
	if (const auto* Image = dynamic_cast<const DUiImage*>(&Element))
	{
		const auto& Texture = Image->GetTexture();
		const auto Content = Image->GetImageRect();
		const Toolbox::f32 Scale = Root.GetSurface().GetScale() * Image->GetInspectionPeakScale();
		if (Texture.IsValid() && (static_cast<Toolbox::f64>(Content.Width) * Scale > Texture.GetWidth() ||
		                          static_cast<Toolbox::f64>(Content.Height) * Scale > Texture.GetHeight()))
		{
			Add_Internal(Result, Element, Path, EUiInspectionKind::ImageResolution,
			             {Content.X, Content.Y, static_cast<Toolbox::f32>(Texture.GetWidth()) / Scale,
			              static_cast<Toolbox::f32>(Texture.GetHeight()) / Scale},
			             Content, "image pixels below peak display size", Limit);
		}
	}
	for (Toolbox::size_t I = 0; I < Element.GetChildCount(); ++I)
	{
		const auto* Child = Element.GetChild(I);
		if (Child == nullptr)
		{
			continue;
		}
		const auto ChildPath = Path + "/" + Child->GetName() + "[" + Toolbox::ToString(I) + "]";
		// 親の重なり許可（Popup/Overlay背景）を明示した場合だけ兄弟比較を省く。
		if (!Element.HasCheckAllowance(EUiCheckAllowance::Overlap) && Child->IsVisibleInTree())
		{
			for (Toolbox::size_t J = 0; J < I; ++J)
			{
				const auto* Other = Element.GetChild(J);
				if (Other != nullptr && Other->IsVisibleInTree() &&
				    !Child->GetRect().Intersect(Other->GetRect()).IsEmpty())
				{
					Add_Internal(Result, *Child, ChildPath, EUiInspectionKind::Overlap, Other->GetRect(),
					             Child->GetRect(), "sibling rectangles overlap without declaration", Limit);
					break;
				}
			}
		}
		Visit_Internal(Root, *Child, ChildPath, ChildClip, Inspect);
	}
}
} // namespace

FUiInspectionResult InspectUiLayout(FUiRoot& Root, const FUiDrawList* DrawList, Toolbox::size_t MaxIssues,
                                    const FUiInspectionTarget& Target)
{
	if (MaxIssues == 0)
	{
		throw Toolbox::FException("UI inspection requires positive issue limit");
	}
	FUiInspectionResult Result;
	const FUiSurface& Surface = Root.GetSurface();
	Result.DisplayId = Target.DisplayId;
	Result.SurfacePixels = Surface.GetPixelRect();
	Result.Scale = Surface.GetScale();
	Result.LogicalSize = Surface.GetLogicalSize();
	// 最も外側のクリップは表示面の論理範囲と、表示先が追加で掛けるクリップの共通部分。
	FUiRect SurfaceClip{0, 0, Result.LogicalSize.Width, Result.LogicalSize.Height};
	if (!Target.DisplayClip.IsEmpty())
	{
		SurfaceClip = SurfaceClip.Intersect(Target.DisplayClip);
	}
	Toolbox::TMap<Toolbox::uint64, FElementClip> Clips;
	FInspectState Inspect{Result, MaxIssues, Clips};
	for (Toolbox::size_t Layer = 0; Layer < static_cast<Toolbox::size_t>(EUiLayer::Count); ++Layer)
	{
		const auto& Element = Root.GetLayer(static_cast<EUiLayer>(Layer));
		Visit_Internal(Root, Element, Element.GetName(), SurfaceClip, Inspect);
	}
	for (const auto& Error : Root.GetLayoutErrors())
	{
		Add_Internal(Result, Root.GetLayer(EUiLayer::Normal), "UiRoot", EUiInspectionKind::InvalidLayout, {}, {},
		             Error.CStr(), MaxIssues);
	}
	// ClipFitは実際に生成された命令のクリップを、命令を出した要素の期待するクリップと比べる。
	if (DrawList != nullptr)
	{
		const auto& Items = DrawList->GetItems();
		for (Toolbox::size_t Index = 0; Index < Items.Size(); ++Index)
		{
			const FUiDrawItem& Item = Items[Index];
			const auto Found = Clips.Find(Item.SourceId);
			const bool bKnown = Found != Clips.End();
			const FUiRect Expected = bKnown ? Found->Second.Clip : SurfaceClip;
			if (bKnown && Contains_Internal(Expected, Item.Clip))
			{
				continue;
			}
			const DUiElement& Element = bKnown ? *Found->Second.Element : Root.GetLayer(EUiLayer::Normal);
			const Toolbox::size_t Before = Result.Issues.Size();
			Add_Internal(Result, Element, bKnown ? Found->Second.Path : Toolbox::FString("DrawList"),
			             EUiInspectionKind::ClipFit, Expected, Item.Clip,
			             bKnown ? "draw clip exceeds the clip of its source element"
			                    : "draw item has no visible source element",
			             MaxIssues);
			if (Result.Issues.Size() > Before)
			{
				FUiInspectionIssue& Issue = Result.Issues[Result.Issues.Size() - 1];
				Issue.DrawItemIndex = Index;
				if (!bKnown)
				{
					// 出所のない命令は要素へ対応させない。
					Issue.ElementId = Item.SourceId;
					Issue.Source = {};
				}
			}
		}
	}
	return Result;
}
} // namespace Dxf
