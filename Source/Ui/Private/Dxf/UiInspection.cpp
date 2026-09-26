// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiInspection.h"
#include "Dxf/UiLabel.h"
#include "Dxf/UiImage.h"
#include "Dxf/UiDefaultStyles.h"
namespace Dxf
{
namespace
{
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
	Issue.Path = Path;
	Issue.Expected = Expected;
	Issue.Actual = Actual;
	Issue.Reason = Reason;
	Issue.Severity =
	    Kind == EUiInspectionKind::InvalidLayout ? EUiInspectionSeverity::Error : EUiInspectionSeverity::Warning;
	Result.Issues.PushBack(Toolbox::Move(Issue));
}
// 検査はコールバックを呼ばず、実配置を値として読む。
void Visit_Internal(FUiRoot& Root, const DUiElement& Element, const Toolbox::FString& Path, FUiInspectionResult& Result,
                    Toolbox::size_t Limit)
{
	if (!Element.IsAttached() || Element.IsDestroyRequested() || !Element.IsVisibleInTree())
	{
		return;
	}
	++Result.Elements;
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
		Visit_Internal(Root, *Child, ChildPath, Result, Limit);
	}
}
} // namespace

FUiInspectionResult InspectUiLayout(FUiRoot& Root, const FUiDrawList* DrawList, Toolbox::size_t MaxIssues)
{
	if (MaxIssues == 0)
	{
		throw Toolbox::FException("UI inspection requires positive issue limit");
	}
	FUiInspectionResult Result;
	for (Toolbox::size_t Layer = 0; Layer < static_cast<Toolbox::size_t>(EUiLayer::Count); ++Layer)
	{
		const auto& Element = Root.GetLayer(static_cast<EUiLayer>(Layer));
		Visit_Internal(Root, Element, Element.GetName(), Result, MaxIssues);
	}
	for (const auto& Error : Root.GetLayoutErrors())
	{
		Add_Internal(Result, Root.GetLayer(EUiLayer::Normal), "UiRoot", EUiInspectionKind::InvalidLayout, {}, {},
		             Error.CStr(), MaxIssues);
	}
	// ClipFitは論理木だけでなく、実際に生成された命令の有効なクリップを検査する。
	if (DrawList != nullptr)
	{
		for (const auto& Item : DrawList->GetItems())
		{
			if (!Contains_Internal(Root.GetLayer(EUiLayer::Normal).GetRect(), Item.Clip))
			{
				Add_Internal(Result, Root.GetLayer(EUiLayer::Normal), "DrawList", EUiInspectionKind::ClipFit,
				             Root.GetLayer(EUiLayer::Normal).GetRect(), Item.Clip, "draw clip exceeds surface",
				             MaxIssues);
			}
		}
	}
	return Result;
}
} // namespace Dxf
