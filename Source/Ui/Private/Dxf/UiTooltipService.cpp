// SPDX-License-Identifier: NOASSERTION
#include "UiTooltipService.h"
#include "UiRootState.h"
namespace Dxf::Detail
{
// ツールチップの表示。入力は受けず、折り返して表示する。
DUiTooltipView::DUiTooltipView()
{
	SetName("Ui.Tooltip");
	SetStyleId("Tooltip");
	SetHitTest(EUiHitTest::None);
	SetTextLayout(EUiTextWrap::Wrap, EUiTextOverflow::Visible);
	SetAlign(EUiAlign::Start, EUiAlign::Start);
	DeclareCheckAllowance(EUiCheckAllowance::Overlap, "Tooltip floats above other elements");
}
// ツールチップ領域。
DUiTooltipLayer::DUiTooltipLayer()
{
	SetHitTest(EUiHitTest::None);
}
// 基準点から位置を決める。
void DUiTooltipLayer::OnArrange(FUiLayoutContext& Context, const FUiRect& Content)
{
	constexpr Toolbox::f32 OffsetX = 14;
	constexpr Toolbox::f32 OffsetY = 18;
	for (Toolbox::size_t Index = 0; Index < GetChildCount(); ++Index)
	{
		DUiElement* Child = GetChild(Index);
		auto* View = dynamic_cast<DUiTooltipView*>(Child);
		if (View == nullptr || !FUiRootState::IsLive(Child))
		{
			if (Child != nullptr)
			{
				Context.ArrangeChild(*Child, Content);
			}
			continue;
		}
		const FUiSize Size = View->GetDesiredSize();
		Toolbox::f32 X = View->Anchor.X + OffsetX;
		Toolbox::f32 Y = View->Anchor.Y + OffsetY;
		// 右端を越えるなら左へ寄せる。下端を越えるなら基準点の上へ出す。
		if (X + Size.Width > Content.Right())
		{
			X = Content.Right() - Size.Width;
		}
		if (Y + Size.Height > Content.Bottom())
		{
			Y = View->Anchor.Y - Size.Height - 4;
		}
		X = Toolbox::Max(X, Content.X);
		Y = Toolbox::Max(Y, Content.Y);
		Context.ArrangeChild(*Child, {X, Y, Size.Width, Size.Height});
	}
}
// ホバー中の最も近い、ツールチップを持つ要素。
DUiElement* FUiTooltipService::FindCandidate_Internal(FUiRootState& State) noexcept
{
	for (DUiElement* Current = State.Input.GetHovered(); Current != nullptr; Current = Current->GetParent())
	{
		if (!Current->GetTooltip().IsEmpty())
		{
			return Current;
		}
	}
	return nullptr;
}
// 消す。
void FUiTooltipService::Hide(FUiRootState& State) noexcept
{
	(void)State;
	if (m_bShown)
	{
		if (DUiTooltipView* View = m_View.Get())
		{
			View->SetVisibility(EUiVisibility::Collapsed);
		}
	}
	m_bShown = false;
	m_bSuppressed = m_Target.Get() != nullptr;
	m_Hover = 0;
}
// 切断した要素が対象なら消す。
void FUiTooltipService::OnElementDetached(FUiRootState& State, DUiElement& Element) noexcept
{
	if (m_Target.GetId() == Element.GetRef().GetId())
	{
		Hide(State);
		m_Target = {};
		m_bSuppressed = false;
	}
	if (m_View.GetId() == Element.GetRef().GetId())
	{
		m_View = {};
		m_bShown = false;
	}
}
// 一フレームの更新。
void FUiTooltipService::Update(FUiRootState& State, Toolbox::f64 DeltaSeconds)
{
	DUiElement* Candidate = FindCandidate_Internal(State);
	// キャプチャ中（押下・ドラッグ中）は出さない。
	if (State.Input.GetCaptured() != nullptr)
	{
		Candidate = nullptr;
	}
	if (Candidate == nullptr || Candidate->GetRef().GetId() != m_Target.GetId())
	{
		if (m_bShown)
		{
			Hide(State);
		}
		m_Target = Candidate != nullptr ? Candidate->GetRef() : TUiRef<DUiElement>{};
		m_Hover = 0;
		m_bSuppressed = false;
	}
	DUiElement* Target = m_Target.Get();
	if (Target == nullptr || !FUiRootState::IsLive(Target) || !Target->IsVisibleInTree())
	{
		if (m_bShown)
		{
			Hide(State);
		}
		m_Target = {};
		m_bSuppressed = false;
		return;
	}
	m_Hover += Toolbox::Max(0.0, DeltaSeconds);
	DUiTooltipView* View = m_View.Get();
	if (m_bShown && View != nullptr)
	{
		// 表示中に文字が変わったら追随する。
		View->SetText(Target->GetTooltip());
		return;
	}
	if (m_bSuppressed || m_Hover < State.Settings.Navigation.TooltipDelay)
	{
		return;
	}
	if (View == nullptr)
	{
		m_View = State.pOwner->Create<DUiTooltipView>();
		auto Added = State.pOwner->AddToLayer(EUiLayer::Tooltip, m_View.Cast<DUiElement>());
		if (!Added)
		{
			State.pOwner->Destroy(m_View.Cast<DUiElement>());
			m_View = {};
			return;
		}
		View = m_View.Get();
	}
	// 表示面の幅に応じて折り返す幅を決める（端での折返し）。
	const Toolbox::f32 MaxWidth = Toolbox::Max(40.0f, Toolbox::Min(360.0f, State.Surface.GetLogicalSize().Width - 16));
	View->SetSizeLimits({0, 0}, {MaxWidth, UiUnbounded});
	View->SetText(Target->GetTooltip());
	View->Anchor = State.Input.GetLastPosition();
	View->SetVisibility(EUiVisibility::Visible);
	View->InvalidateArrange();
	m_bShown = true;
}
} // namespace Dxf::Detail
