// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_LIST_ITEM_H
#define DXF_UI_LIST_ITEM_H
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * リストの値。一意なKeyは並べ替えでも維持し、0は予約値として拒否する。
 */
struct FUiListItem
{
	Toolbox::uint64 Key = 0;
	Toolbox::FString Text;
	Toolbox::FString Tooltip;
};
} // namespace Dxf
// namespace Dxf
#endif
