// SPDX-License-Identifier: NOASSERTION
#include "UiDrawBuilder.h"
#include "UiRootState.h"
namespace Dxf::Detail
{
// 要素と子孫を記録する。
void FUiDrawBuilder::DrawElement_Internal(FUiDrawContext& Context, const DUiElement& Element)
{
	if (!FUiRootState::IsLive(&Element) || Element.m_Visibility != EUiVisibility::Visible || Element.m_bLayoutInvalid)
	{
		return;
	}
	// 配置で決めたクリップ（祖先のクリップの共通部分）へ切り替える。
	const FUiRect Previous = Context.GetClip();
	Context.SetClip_Internal(Element.m_ClipRect);
	const Toolbox::f32 PreviousOpacity = Context.PushOpacity_Internal(Element.m_Style.Opacity);
	Context.SetSource_Internal(Element.m_Id);
	Element.Draw_Internal(Context);
	for (const DUiElement* Child : Element.m_Children)
	{
		DrawElement_Internal(Context, *Child);
	}
	Context.SetClip_Internal(Element.m_ClipRect);
	Context.SetSource_Internal(Element.m_Id);
	Element.DrawOverlay_Internal(Context);
	Context.PopOpacity_Internal(PreviousOpacity);
	Context.SetClip_Internal(Previous);
}
// 描画を記録する。
void FUiDrawBuilder::Build(FUiRootState& State, FUiDrawList& List)
{
	if (!State.Surface.IsDisplayable())
	{
		return;
	}
	FUiDrawContext Context(List, State.Surface, State.Settings.Text);
	for (const DUiElement* Layer : State.Layers)
	{
		DrawElement_Internal(Context, *Layer);
	}
}
} // namespace Dxf::Detail
