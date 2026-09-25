// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_DRAW_BUILDER_H
#define DXF_UI_DRAW_BUILDER_H
#include "Dxf/UiDrawContext.h"
#include "Dxf/UiElement.h"
namespace Dxf::Detail
{
struct FUiRootState;

/**
 * 重なり領域の順（通常→追加パネル→Popup→ツールチップ）に、要素の順で描画を記録する。
 * 各要素は配置で決めたクリップの中へ描き、非表示の要素と子は描かない。
 */
class FUiDrawBuilder
{
public:
	/**
	 * 描画を記録する。
	 * @param State ルートの状態。
	 * @param List 記録先（追記）。
	 */
	static void Build(FUiRootState& State, FUiDrawList& List);

private:
	/**
	 * 要素と子孫を記録する。
	 * @param Context 窓口。
	 * @param Element 要素。
	 */
	static void DrawElement_Internal(FUiDrawContext& Context, const DUiElement& Element);
};
} // namespace Dxf::Detail
#endif
