// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SPACER_H
#define DXF_UI_SPACER_H
#include "Dxf/UiElement.h"
namespace Dxf
{
/**
 * 何も描かない間隔。固定の長さ、または余り領域を埋める（Fill）。入力は背後へ通す。
 */
class DUiSpacer final : public DUiElement
{
public:
	/**
	 * @param Width 幅の指定。
	 * @param Height 高さの指定。
	 */
	DUiSpacer(FUiLength Width = FUiLength::Fill(), FUiLength Height = FUiLength::Fill())
	{
		SetWidth(Width);
		SetHeight(Height);
		SetHitTest(EUiHitTest::None);
	}
};
} // namespace Dxf
#endif
