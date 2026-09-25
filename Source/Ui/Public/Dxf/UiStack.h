// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_STACK_H
#define DXF_UI_STACK_H
#include "Dxf/UiElement.h"
namespace Dxf
{
/**
 * 背景のない入れ物。子を縦・横・重ね合わせで並べ、自身は入力を受けない（子だけが受ける）。
 */
class DUiStack : public DUiElement
{
public:
	/**
	 * @param Mode 子の並べ方。
	 * @param Gap 子の間隔（論理単位）。
	 */
	explicit DUiStack(EUiStackMode Mode = EUiStackMode::Vertical, Toolbox::f32 Gap = 0)
	{
		SetStack(Mode, Gap);
		SetHitTest(EUiHitTest::ChildrenOnly);
	}
	using DUiElement::AddChild;
	using DUiElement::CreateChild;
};
} // namespace Dxf
#endif
