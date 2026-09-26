// SPDX-License-Identifier: NOASSERTION
#include "UiLayoutEngine.h"
#include "UiRootState.h"
namespace Dxf::Detail
{
namespace
{
// 有限で0以上か。
FORCEINLINE bool IsLength_Internal(Toolbox::f32 Value) noexcept
{
	return Toolbox::IsFinite(Value) && Value >= 0;
}
// 余白の値が有限で0以上か。
bool IsThickness_Internal(const FUiThickness& Value) noexcept
{
	return IsLength_Internal(Value.Left) && IsLength_Internal(Value.Top) && IsLength_Internal(Value.Right) &&
	       IsLength_Internal(Value.Bottom);
}
// 長さの指定が有効か。
bool IsValidLength_Internal(const FUiLength& Length) noexcept
{
	switch (Length.Mode)
	{
	case EUiSizeMode::Content:
		return true;
	case EUiSizeMode::Fixed:
		return IsLength_Internal(Length.Value);
	case EUiSizeMode::Fill:
		return Toolbox::IsFinite(Length.Value) && Length.Value > 0;
	}
	return false;
}
// 上限なしを保った引き算（0未満は0）。
FORCEINLINE Toolbox::f32 Shrink_Internal(Toolbox::f32 Available, Toolbox::f32 Amount) noexcept
{
	if (Available >= UiUnbounded)
	{
		return UiUnbounded;
	}
	return Toolbox::Max(0.0f, Available - Amount);
}
// 下限と上限で挟む（上限はUiUnbounded以上なら無視）。
FORCEINLINE Toolbox::f32 ClampLength_Internal(Toolbox::f32 Value, Toolbox::f32 Min, Toolbox::f32 Max) noexcept
{
	Toolbox::f32 Result = Toolbox::Max(Value, Min);
	if (Max < UiUnbounded)
	{
		Result = Toolbox::Min(Result, Max);
	}
	return Result;
}
// 子が測定・配置の対象か（接続済み・破棄の要求なし・畳まれていない）。
FORCEINLINE bool Participates_Internal(const DUiElement* Child) noexcept
{
	return FUiRootState::IsLive(Child) && Child->GetVisibility() != EUiVisibility::Collapsed;
}
// 並べる軸で見た子の長さ（余白を含む）と、Fillの重み。
struct FAxisItem
{
	// 子。
	DUiElement* Child = nullptr;
	// 固定・内容の長さ（余白を含む）。
	Toolbox::f32 Length = 0;
	// Fillの重み（0はFillでない）。
	Toolbox::f32 Weight = 0;
};
} // namespace

// 子孫の寸法と見た目を無効にする。
void FUiLayoutEngine::InvalidateSubtree_Internal(DUiElement& Element) noexcept
{
	Element.m_bMeasureDirty = true;
	Element.m_bArrangeDirty = true;
	Element.m_bStyleDirty = true;
	Element.m_LastAvailable = {-1, -1};
	for (DUiElement* Child : Element.m_Children)
	{
		InvalidateSubtree_Internal(*Child);
	}
}
// 失敗を記録する。
void FUiLayoutEngine::RecordError_Internal(DUiElement& Element, const char* Message)
{
	Element.m_bLayoutInvalid = true;
	Toolbox::FString Text(Message);
	Text += " (";
	Text +=
	    Element.GetName().IsEmpty() ? Toolbox::FString("#") + Toolbox::ToString(Element.GetId()) : Element.GetName();
	Text += ")";
	if (m_Errors.Size() < 256)
	{
		m_Errors.PushBack(Toolbox::Move(Text));
	}
}
// 全重なり領域のレイアウト。
TResult<void> FUiLayoutEngine::Run(FUiRootState& State)
{
	if (!State.Surface.IsDisplayable())
	{
		return {};
	}
	// 論理寸法と倍率が同じなら、表示先の画素原点だけの変化は再測定しない。
	if (m_bForceFull || State.Surface.GetLogicalSize() != m_LastSurface.GetLogicalSize() ||
	    State.Surface.GetScale() != m_LastSurface.GetScale())
	{
		for (DUiElement* Layer : State.Layers)
		{
			InvalidateSubtree_Internal(*Layer);
		}
		m_LastSurface = State.Surface;
		m_bForceFull = false;
	}
	bool bDirty = false;
	for (DUiElement* Layer : State.Layers)
	{
		bDirty = bDirty || Layer->m_bMeasureDirty || Layer->m_bArrangeDirty;
	}
	if (!bDirty)
	{
		return {};
	}
	m_Errors.Clear();
	m_pState = &State;
	m_bWorked = false;
	State.bFrozen = true;
	const FUiSize Size = State.Surface.GetLogicalSize();
	FUiLayoutContext Context(*this, State.Surface, State.Settings.Text, m_Stats);
	try
	{
		for (DUiElement* Layer : State.Layers)
		{
			Layer->m_Layout.Width = FUiLength::Fixed(Size.Width);
			Layer->m_Layout.Height = FUiLength::Fixed(Size.Height);
			m_CurrentClip = {0, 0, Size.Width, Size.Height};
			(void)MeasureChild(Context, *Layer, Size);
			ArrangeChild(Context, *Layer, {0, 0, Size.Width, Size.Height});
		}
	}
	catch (...)
	{
		State.bFrozen = false;
		m_pState = nullptr;
		return TResult<void>::Failure(EErrorCode::UserException, "UI layout failed");
	}
	State.bFrozen = false;
	m_pState = nullptr;
	if (m_bWorked)
	{
		++m_Stats.Passes;
	}
	return {};
}
// 子の望む大きさ。
FUiSize FUiLayoutEngine::MeasureChild(FUiLayoutContext& Context, DUiElement& Element, FUiSize Available)
{
	if (!FUiRootState::IsLive(&Element))
	{
		return {0, 0};
	}
	if (Element.m_bStyleDirty && m_pState != nullptr)
	{
		m_pState->ResolveStyle(Element);
	}
	const FUiLayoutParams& Layout = Element.m_Layout;
	if (Element.m_Visibility == EUiVisibility::Collapsed)
	{
		Element.m_DesiredSize = {0, 0};
		Element.m_bMeasureDirty = false;
		Element.m_LastAvailable = Available;
		return {0, 0};
	}
	if (!Element.m_bMeasureDirty && Element.m_LastAvailable == Available)
	{
		return {Element.m_DesiredSize.Width + Layout.Margin.Horizontal(),
		        Element.m_DesiredSize.Height + Layout.Margin.Vertical()};
	}
	++m_Stats.Measures;
	m_bWorked = true;
	Element.m_bLayoutInvalid = false;
	Element.m_LastAvailable = Available;
	Element.m_bMeasureDirty = false;
	Element.m_bArrangeDirty = true;
	const FUiThickness Padding = Element.GetEffectivePadding();
	if (!IsValidLength_Internal(Layout.Width) || !IsValidLength_Internal(Layout.Height) ||
	    !IsThickness_Internal(Layout.Margin) || !IsThickness_Internal(Padding) ||
	    !IsLength_Internal(Layout.MinSize.Width) || !IsLength_Internal(Layout.MinSize.Height) ||
	    !(Layout.MaxSize.Width >= Layout.MinSize.Width) || !(Layout.MaxSize.Height >= Layout.MinSize.Height) ||
	    !Toolbox::IsFinite(Layout.Position.X) || !Toolbox::IsFinite(Layout.Position.Y) ||
	    !Toolbox::IsFinite(Available.Width) || !Toolbox::IsFinite(Available.Height) || Available.Width < 0 ||
	    Available.Height < 0)
	{
		RecordError_Internal(Element, "Invalid UI layout value (NaN, infinite, negative or min > max)");
		Element.m_DesiredSize = {0, 0};
		return {0, 0};
	}
	// 内容に使える大きさ。
	FUiSize Inner{Shrink_Internal(Available.Width, Layout.Margin.Horizontal() + Padding.Horizontal()),
	              Shrink_Internal(Available.Height, Layout.Margin.Vertical() + Padding.Vertical())};
	if (Layout.Width.Mode == EUiSizeMode::Fixed)
	{
		Inner.Width =
		    Toolbox::Max(0.0f, ClampLength_Internal(Layout.Width.Value, Layout.MinSize.Width, Layout.MaxSize.Width) -
		                           Padding.Horizontal());
	}
	else if (Layout.MaxSize.Width < UiUnbounded)
	{
		Inner.Width = Toolbox::Min(Inner.Width, Toolbox::Max(0.0f, Layout.MaxSize.Width - Padding.Horizontal()));
	}
	if (Layout.Height.Mode == EUiSizeMode::Fixed)
	{
		Inner.Height =
		    Toolbox::Max(0.0f, ClampLength_Internal(Layout.Height.Value, Layout.MinSize.Height, Layout.MaxSize.Height) -
		                           Padding.Vertical());
	}
	else if (Layout.MaxSize.Height < UiUnbounded)
	{
		Inner.Height = Toolbox::Min(Inner.Height, Toolbox::Max(0.0f, Layout.MaxSize.Height - Padding.Vertical()));
	}
	FUiSize Content;
	try
	{
		Content = Element.OnMeasure(Context, Inner);
	}
	catch (const Toolbox::FException& Error)
	{
		RecordError_Internal(Element, Error.What());
		Element.m_DesiredSize = {0, 0};
		return {0, 0};
	}
	if (!IsLength_Internal(Content.Width) || !IsLength_Internal(Content.Height))
	{
		RecordError_Internal(Element, "Invalid UI measured size (NaN, infinite or negative)");
		Element.m_DesiredSize = {0, 0};
		return {0, 0};
	}
	Toolbox::f32 Width =
	    Layout.Width.Mode == EUiSizeMode::Fixed ? Layout.Width.Value : Content.Width + Padding.Horizontal();
	Toolbox::f32 Height =
	    Layout.Height.Mode == EUiSizeMode::Fixed ? Layout.Height.Value : Content.Height + Padding.Vertical();
	Width = ClampLength_Internal(Width, Layout.MinSize.Width, Layout.MaxSize.Width);
	Height = ClampLength_Internal(Height, Layout.MinSize.Height, Layout.MaxSize.Height);
	Element.m_DesiredSize = {Width, Height};
	return {Width + Layout.Margin.Horizontal(), Height + Layout.Margin.Vertical()};
}
// 子を割当範囲へ配置する。
void FUiLayoutEngine::ArrangeChild(FUiLayoutContext& Context, DUiElement& Element, const FUiRect& Slot)
{
	if (!FUiRootState::IsLive(&Element))
	{
		return;
	}
	const FUiLayoutParams& Layout = Element.m_Layout;
	const FUiRect ParentClip = m_CurrentClip;
	if (Element.m_Visibility == EUiVisibility::Collapsed || Element.m_bLayoutInvalid)
	{
		Element.m_Rect = {Slot.X, Slot.Y, 0, 0};
		Element.m_ClipRect = {Slot.X, Slot.Y, 0, 0};
		Element.m_bArrangeDirty = false;
		return;
	}
	// 余白を除いた範囲。
	const FUiRect Inner{Slot.X + Layout.Margin.Left, Slot.Y + Layout.Margin.Top,
	                    Toolbox::Max(0.0f, Slot.Width - Layout.Margin.Horizontal()),
	                    Toolbox::Max(0.0f, Slot.Height - Layout.Margin.Vertical())};
	FUiRect Rect;
	if (Layout.bAbsolute)
	{
		Rect = {Slot.X + Layout.Position.X + Layout.Margin.Left, Slot.Y + Layout.Position.Y + Layout.Margin.Top,
		        Element.m_DesiredSize.Width, Element.m_DesiredSize.Height};
	}
	else
	{
		// 一つの軸の長さと位置。
		auto Axis = [](Toolbox::f32 Start, Toolbox::f32 Length, const FUiLength& Mode, EUiAlign Align,
		               Toolbox::f32 Desired, Toolbox::f32 Min, Toolbox::f32 Max, Toolbox::f32& OutStart,
		               Toolbox::f32& OutLength)
		{
			const bool bStretch =
			    Mode.Mode == EUiSizeMode::Fill || (Align == EUiAlign::Stretch && Mode.Mode == EUiSizeMode::Content);
			OutLength = bStretch ? ClampLength_Internal(Length, Min, Max) : Desired;
			switch (Align)
			{
			case EUiAlign::Center:
				OutStart = Start + (Length - OutLength) * 0.5f;
				break;
			case EUiAlign::End:
				OutStart = Start + Length - OutLength;
				break;
			default:
				OutStart = Start;
				break;
			}
		};
		Axis(Inner.X, Inner.Width, Layout.Width, Layout.HorizontalAlign, Element.m_DesiredSize.Width,
		     Layout.MinSize.Width, Layout.MaxSize.Width, Rect.X, Rect.Width);
		Axis(Inner.Y, Inner.Height, Layout.Height, Layout.VerticalAlign, Element.m_DesiredSize.Height,
		     Layout.MinSize.Height, Layout.MaxSize.Height, Rect.Y, Rect.Height);
	}
	if (!Element.m_bArrangeDirty && Rect == Element.m_Rect && ParentClip == Element.m_ClipRect)
	{
		return;
	}
	++m_Stats.Arranges;
	m_bWorked = true;
	Element.m_Rect = Rect;
	Element.m_ClipRect = ParentClip;
	Element.m_LastSlot = Slot;
	Element.m_bArrangeDirty = false;
	m_CurrentClip = Element.m_bClipChildren ? ParentClip.Intersect(Rect) : ParentClip;
	try
	{
		Element.OnArrange(Context, Element.GetContentRect());
	}
	catch (const Toolbox::FException& Error)
	{
		RecordError_Internal(Element, Error.What());
	}
	m_CurrentClip = ParentClip;
}
// 並べ方に従って子を測る。
FUiSize FUiLayoutEngine::MeasureStack(FUiLayoutContext& Context, DUiElement& Element, FUiSize Available)
{
	FUiSize Result;
	const EUiStackMode Mode = Element.GetStackMode();
	Toolbox::size_t Count = 0;
	for (Toolbox::size_t Index = 0; Index < Element.GetChildCount(); ++Index)
	{
		DUiElement* Child = Element.GetChild(Index);
		if (!Participates_Internal(Child))
		{
			continue;
		}
		if (Mode == EUiStackMode::Overlay)
		{
			const FUiSize Size = Context.MeasureChild(*Child, Available);
			if (!Child->GetLayout().bAbsolute)
			{
				Result.Width = Toolbox::Max(Result.Width, Size.Width);
				Result.Height = Toolbox::Max(Result.Height, Size.Height);
			}
		}
		else if (Mode == EUiStackMode::Vertical)
		{
			const FUiSize Size = Context.MeasureChild(*Child, {Available.Width, UiUnbounded});
			Result.Width = Toolbox::Max(Result.Width, Size.Width);
			Result.Height += Size.Height;
			++Count;
		}
		else
		{
			const FUiSize Size = Context.MeasureChild(*Child, {UiUnbounded, Available.Height});
			Result.Height = Toolbox::Max(Result.Height, Size.Height);
			Result.Width += Size.Width;
			++Count;
		}
	}
	if (Count > 1)
	{
		const Toolbox::f32 Gaps = Element.GetGap() * static_cast<Toolbox::f32>(Count - 1);
		if (Mode == EUiStackMode::Vertical)
		{
			Result.Height += Gaps;
		}
		else
		{
			Result.Width += Gaps;
		}
	}
	return Result;
}
// 並べ方に従って子を配置する。
void FUiLayoutEngine::ArrangeStack(FUiLayoutContext& Context, DUiElement& Element, const FUiRect& Content)
{
	const EUiStackMode Mode = Element.GetStackMode();
	if (Mode == EUiStackMode::Overlay)
	{
		for (Toolbox::size_t Index = 0; Index < Element.GetChildCount(); ++Index)
		{
			DUiElement* Child = Element.GetChild(Index);
			if (FUiRootState::IsLive(Child))
			{
				Context.ArrangeChild(*Child, Content);
			}
		}
		return;
	}
	const bool bVertical = Mode == EUiStackMode::Vertical;
	// 並べる子と、固定・内容の合計、Fillの重みの合計。
	Toolbox::TVector<FAxisItem> Items;
	Items.Reserve(Element.GetChildCount());
	Toolbox::f32 Fixed = 0;
	Toolbox::f32 Weights = 0;
	for (Toolbox::size_t Index = 0; Index < Element.GetChildCount(); ++Index)
	{
		DUiElement* Child = Element.GetChild(Index);
		if (!FUiRootState::IsLive(Child))
		{
			continue;
		}
		if (Child->GetVisibility() == EUiVisibility::Collapsed)
		{
			Context.ArrangeChild(*Child, {Content.X, Content.Y, 0, 0});
			continue;
		}
		const FUiLayoutParams& Layout = Child->GetLayout();
		const FUiLength& Length = bVertical ? Layout.Height : Layout.Width;
		FAxisItem Item;
		Item.Child = Child;
		if (Length.Mode == EUiSizeMode::Fill)
		{
			Item.Weight = Length.Value;
			Weights += Length.Value;
		}
		else
		{
			Item.Length = bVertical ? Child->GetDesiredSize().Height + Layout.Margin.Vertical()
			                        : Child->GetDesiredSize().Width + Layout.Margin.Horizontal();
			Fixed += Item.Length;
		}
		Items.PushBack(Item);
	}
	if (Items.IsEmpty())
	{
		return;
	}
	const Toolbox::f32 Gaps = Element.GetGap() * static_cast<Toolbox::f32>(Items.Size() - 1);
	const Toolbox::f32 Total = bVertical ? Content.Height : Content.Width;
	const Toolbox::f32 Remaining = Toolbox::Max(0.0f, Total - Fixed - Gaps);
	Toolbox::f32 Cursor = bVertical ? Content.Y : Content.X;
	for (const FAxisItem& Item : Items)
	{
		const Toolbox::f32 Length = Item.Weight > 0 ? Remaining * Item.Weight / Weights : Item.Length;
		const FUiRect Slot = bVertical ? FUiRect{Content.X, Cursor, Content.Width, Length}
		                               : FUiRect{Cursor, Content.Y, Length, Content.Height};
		Context.ArrangeChild(*Item.Child, Slot);
		Cursor += Length + Element.GetGap();
	}
}
} // namespace Dxf::Detail

namespace Dxf
{
// 子の望む大きさ。
FUiSize FUiLayoutContext::MeasureChild(DUiElement& Child, FUiSize Available)
{
	return m_pEngine->MeasureChild(*this, Child, Available);
}
// 子を配置する。
void FUiLayoutContext::ArrangeChild(DUiElement& Child, const FUiRect& Slot)
{
	m_pEngine->ArrangeChild(*this, Child, Slot);
}
// 字体を解決する。
TResult<FFont> FUiLayoutContext::ResolveFont(const Toolbox::FString& Family, Toolbox::f32 LogicalSize)
{
	if (m_pText == nullptr)
	{
		return TResult<FFont>::Failure(EErrorCode::InvalidState, "UI text service is not set");
	}
	return m_pText->ResolveFont({Family, m_pSurface->ToFontPixelSize(LogicalSize)});
}
// 文字を配置する。
TResult<FUiTextLayoutResult> FUiLayoutContext::LayoutText(const FFont& Font, const Toolbox::FString& Text,
                                                          const FUiTextLayoutRequest& Request)
{
	if (m_pText == nullptr)
	{
		return TResult<FUiTextLayoutResult>::Failure(EErrorCode::InvalidState, "UI text service is not set");
	}
	auto Result = LayoutUiText(*m_pText, Font, Text, Request);
	++m_pStats->TextLayouts;
	if (Result)
	{
		m_pStats->TextMeasurements += Result.Value().Measurements;
	}
	return Result;
}
// 論理単位の長さを画素へ。
Toolbox::int32 FUiLayoutContext::ToPixels(Toolbox::f32 Logical) const noexcept
{
	if (Logical >= UiUnbounded || !Toolbox::IsFinite(Logical))
	{
		return -1;
	}
	const Toolbox::f64 Pixels = Toolbox::Floor(static_cast<Toolbox::f64>(Logical) * m_pSurface->GetScale() + 1e-4);
	return static_cast<Toolbox::int32>(Toolbox::Clamp<Toolbox::f64>(Pixels, 0, 1.0e9));
}
} // namespace Dxf
