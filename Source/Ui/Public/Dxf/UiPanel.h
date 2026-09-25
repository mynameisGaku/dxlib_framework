// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_PANEL_H
#define DXF_UI_PANEL_H
#include "Dxf/UiElement.h"
namespace Dxf
{
/**
 * 背景と枠を持つ入れ物（スタイルID "Panel"）。自身の矩形で入力を受けるため、背後の要素へクリックが抜けない。
 */
class DUiPanel : public DUiElement
{
public:
	/**
	 * @param Mode 子の並べ方。
	 * @param Gap 子の間隔（論理単位）。
	 */
	explicit DUiPanel(EUiStackMode Mode = EUiStackMode::Vertical, Toolbox::f32 Gap = 0)
	{
		SetStack(Mode, Gap);
		SetStyleId("Panel");
		SetHitTest(EUiHitTest::Self);
	}
	using DUiElement::AddChild;
	using DUiElement::CreateChild;
};
} // namespace Dxf
#endif
