// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_DEFAULT_STYLES_H
#define DXF_UI_DEFAULT_STYLES_H
#include "Dxf/UiStyleSheet.h"
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
/**
 * 組込みの既定スタイルの資源（スタイルの書式の文字列）。外部の資源がなくても起動できるよう、ライブラリに含める。
 * 共通トークン（color.*・size.*）もここで定義し、C++側はFUiStyleSheet::GetColorToken等で同じ値を読む。
 */
const char* GetBuiltInUiStyleText() noexcept;
/**
 * 組込みの既定スタイル（初回に上の資源を読み、以後は同じ表を共有する）。読めなければ例外。
 */
Toolbox::TSharedPtr<const FUiStyleSheet> GetBuiltInUiStyleSheet();
} // namespace Dxf
#endif
