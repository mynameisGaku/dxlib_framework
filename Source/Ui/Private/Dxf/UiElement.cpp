// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiElement.h"
#include "Dxf/UiDrawContext.h"
#include "Dxf/UiRoot.h"
#include "UiRootState.h"
namespace Dxf
{
// 接続期間の購読を解放する（格納領域が解放するときには切断済み）。
DUiElement::~DUiElement()
{
	m_AttachScope.Clear();
}
// 参照で解決できるか。
bool DUiElement::IsHandleAccessible_Internal() const noexcept
{
	return !m_bDestroyRequested && m_pRoot != nullptr;
}
// スタイルIDを設定する。
void DUiElement::SetStyleId(Toolbox::FString StyleId)
{
	if (StyleId == m_StyleId)
	{
		return;
	}
	m_StyleId = Toolbox::Move(StyleId);
	InvalidateStyle();
}
// 要素だけの見た目の上書き。
void DUiElement::SetStyleOverride(FUiStylePatch Patch)
{
	m_StyleOverride = Toolbox::Move(Patch);
	InvalidateStyle();
}
// 現在の状態。
EUiStyleState DUiElement::GetStyleState() const noexcept
{
	EUiStyleState State = EUiStyleState::Normal;
	if (m_bHovered)
	{
		State = State | EUiStyleState::Hover;
	}
	if (m_bPressed)
	{
		State = State | EUiStyleState::Pressed;
	}
	if (m_bFocused)
	{
		State = State | EUiStyleState::Focus;
	}
	if (!IsEnabledInTree())
	{
		State = State | EUiStyleState::Disabled;
	}
	return State;
}
// レイアウトの指定を置き換える。
void DUiElement::SetLayout(const FUiLayoutParams& Layout)
{
	if (Layout == m_Layout && m_bPaddingExplicit)
	{
		return;
	}
	m_Layout = Layout;
	m_bPaddingExplicit = true;
	InvalidateMeasure();
}
// 幅の決め方。
void DUiElement::SetWidth(FUiLength Width)
{
	if (Width == m_Layout.Width)
	{
		return;
	}
	m_Layout.Width = Width;
	InvalidateMeasure();
}
// 高さの決め方。
void DUiElement::SetHeight(FUiLength Height)
{
	if (Height == m_Layout.Height)
	{
		return;
	}
	m_Layout.Height = Height;
	InvalidateMeasure();
}
// 大きさの下限と上限。
void DUiElement::SetSizeLimits(FUiSize Min, FUiSize Max)
{
	if (Min == m_Layout.MinSize && Max == m_Layout.MaxSize)
	{
		return;
	}
	m_Layout.MinSize = Min;
	m_Layout.MaxSize = Max;
	InvalidateMeasure();
}
// 外側の余白。
void DUiElement::SetMargin(FUiThickness Margin)
{
	if (Margin == m_Layout.Margin)
	{
		return;
	}
	m_Layout.Margin = Margin;
	InvalidateMeasure();
}
// 内側の余白。
void DUiElement::SetPadding(FUiThickness Padding)
{
	if (Padding == m_Layout.Padding && m_bPaddingExplicit)
	{
		return;
	}
	m_Layout.Padding = Padding;
	m_bPaddingExplicit = true;
	InvalidateMeasure();
}
// 親の範囲の中での配置。
void DUiElement::SetAlign(EUiAlign Horizontal, EUiAlign Vertical)
{
	if (Horizontal == m_Layout.HorizontalAlign && Vertical == m_Layout.VerticalAlign)
	{
		return;
	}
	m_Layout.HorizontalAlign = Horizontal;
	m_Layout.VerticalAlign = Vertical;
	InvalidateMeasure();
}
// 親の内容範囲の左上からの位置で置く。
void DUiElement::SetAbsolutePosition(FVector2 Position)
{
	if (m_Layout.bAbsolute && m_Layout.Position.X == Position.X && m_Layout.Position.Y == Position.Y)
	{
		return;
	}
	m_Layout.bAbsolute = true;
	m_Layout.Position = Position;
	InvalidateMeasure();
}
// 子の並べ方。
void DUiElement::SetStack(EUiStackMode Mode, Toolbox::f32 Gap)
{
	if (Mode == m_StackMode && Gap == m_Gap)
	{
		return;
	}
	m_StackMode = Mode;
	m_Gap = Gap;
	InvalidateMeasure();
}
// 実際に使う内側の余白。
FUiThickness DUiElement::GetEffectivePadding() const noexcept
{
	return m_bPaddingExplicit ? m_Layout.Padding : m_Style.Padding;
}
// 内容範囲。
FUiRect DUiElement::GetContentRect() const noexcept
{
	const FUiThickness Padding = GetEffectivePadding();
	return {m_Rect.X + Padding.Left, m_Rect.Y + Padding.Top, Toolbox::Max(0.0f, m_Rect.Width - Padding.Horizontal()),
	        Toolbox::Max(0.0f, m_Rect.Height - Padding.Vertical())};
}
// 子を自身の矩形でクリップするか。
void DUiElement::SetClipChildren(bool bClip)
{
	if (bClip == m_bClipChildren)
	{
		return;
	}
	m_bClipChildren = bClip;
	InvalidateArrange();
}
// 表示の状態。
void DUiElement::SetVisibility(EUiVisibility Visibility)
{
	if (Visibility == m_Visibility)
	{
		return;
	}
	const bool bLayoutChange = Visibility == EUiVisibility::Collapsed || m_Visibility == EUiVisibility::Collapsed;
	m_Visibility = Visibility;
	if (bLayoutChange)
	{
		InvalidateMeasure();
	}
	if (m_pRoot != nullptr && Visibility != EUiVisibility::Visible)
	{
		// 見えなくなった要素（とその子孫）のキャプチャ・フォーカスを外す。
		auto& State = m_pRoot->GetState_Internal();
		State.Input.ValidateCapture(State);
		State.Focus.Validate(State);
	}
}
// 自身と祖先がすべて表示されているか。
bool DUiElement::IsVisibleInTree() const noexcept
{
	for (const DUiElement* Current = this; Current != nullptr; Current = Current->m_pParent)
	{
		if (Current->m_Visibility != EUiVisibility::Visible)
		{
			return false;
		}
	}
	return true;
}
// 有効・無効。
void DUiElement::SetEnabled(bool bEnabled)
{
	if (bEnabled == m_bEnabled)
	{
		return;
	}
	m_bEnabled = bEnabled;
	if (m_pRoot != nullptr)
	{
		auto& State = m_pRoot->GetState_Internal();
		if (!bEnabled)
		{
			State.Input.ValidateCapture(State);
			State.Focus.Validate(State);
		}
		Detail::FUiRootState::NotifyStateChanged(*this);
	}
}
// 自身と祖先がすべて有効か。
bool DUiElement::IsEnabledInTree() const noexcept
{
	for (const DUiElement* Current = this; Current != nullptr; Current = Current->m_pParent)
	{
		if (!Current->m_bEnabled)
		{
			return false;
		}
	}
	return true;
}
// フォーカスを受けるか。
void DUiElement::SetFocusable(bool bFocusable)
{
	m_bFocusable = bFocusable;
	if (!bFocusable && m_bFocused && m_pRoot != nullptr)
	{
		auto& State = m_pRoot->GetState_Internal();
		State.Focus.Validate(State);
	}
}
// ツールチップの文字。
void DUiElement::SetTooltip(Toolbox::FString Tooltip)
{
	m_Tooltip = Toolbox::Move(Tooltip);
}
// 検査の例外を宣言する。
void DUiElement::DeclareCheckAllowance(EUiCheckAllowance Allowance, Toolbox::FString Reason)
{
	m_CheckAllowances = static_cast<Toolbox::uint8>(m_CheckAllowances | static_cast<Toolbox::uint8>(Allowance));
	if (!m_CheckReason.IsEmpty())
	{
		m_CheckReason += "; ";
	}
	m_CheckReason += Reason;
}
// 寸法の再計算を要求する。
void DUiElement::InvalidateMeasure() noexcept
{
	for (DUiElement* Current = this; Current != nullptr; Current = Current->m_pParent)
	{
		Current->m_bMeasureDirty = true;
		Current->m_bArrangeDirty = true;
	}
}
// 配置のやり直しを要求する。
void DUiElement::InvalidateArrange() noexcept
{
	for (DUiElement* Current = this; Current != nullptr; Current = Current->m_pParent)
	{
		Current->m_bArrangeDirty = true;
	}
}
// 見た目の解決のやり直しを要求する。
void DUiElement::InvalidateStyle() noexcept
{
	m_bStyleDirty = true;
	InvalidateMeasure();
}
// 毎フレームの更新を受けるか。
void DUiElement::SetWantsUpdate(bool bWants)
{
	if (bWants == m_bWantsUpdate)
	{
		return;
	}
	m_bWantsUpdate = bWants;
	if (bWants && IsAttached() && m_pRoot != nullptr)
	{
		m_pRoot->GetState_Internal().Updating.PushBack(m_Self);
	}
}
// 内容の大きさ（既定は子を並べ方に従って測る）。
FUiSize DUiElement::OnMeasure(FUiLayoutContext& Context, FUiSize Available)
{
	return Detail::FUiLayoutEngine::MeasureStack(Context, *this, Available);
}
// 子の配置（既定は並べ方に従う）。
void DUiElement::OnArrange(FUiLayoutContext& Context, const FUiRect& Content)
{
	Detail::FUiLayoutEngine::ArrangeStack(Context, *this, Content);
}
// 背景と枠を描く。
void DUiElement::OnDraw(FUiDrawContext& Context) const
{
	Context.FillRect(m_Rect, m_Style.Background);
	Context.DrawBorder(m_Rect, m_Style.BorderColor, m_Style.BorderWidth);
}
// 子の後の描画（既定はなし）。
void DUiElement::OnDrawOverlay(FUiDrawContext& Context) const
{
	(void)Context;
}
// 作成済みの要素を子として末尾へ加える。
TResult<void> DUiElement::AddChild(const TUiRef<DUiElement>& Child)
{
	if (m_pRoot == nullptr)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "UI element is not registered");
	}
	return m_pRoot->AddChild(m_Self, Child);
}
// 押下の状態を設定する。
void DUiElement::SetPressed_Internal(bool bPressed)
{
	if (bPressed == m_bPressed)
	{
		return;
	}
	m_bPressed = bPressed;
	Detail::FUiRootState::NotifyStateChanged(*this);
}
// ポインターをこの要素へ固定する。
bool DUiElement::CapturePointer()
{
	if (m_pRoot == nullptr)
	{
		return false;
	}
	auto& State = m_pRoot->GetState_Internal();
	return State.Input.Capture(State, *this);
}
// ポインターの固定を解く。
void DUiElement::ReleasePointer() noexcept
{
	if (m_pRoot == nullptr)
	{
		return;
	}
	auto& State = m_pRoot->GetState_Internal();
	State.Input.Release(State, *this);
}
// ポインターを固定しているか。
bool DUiElement::HasPointerCapture() const noexcept
{
	return m_pRoot != nullptr && m_pRoot->GetCaptured() == this;
}
} // namespace Dxf
