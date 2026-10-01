// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_MECHANISM_PANEL_H
#define DXF_SAMPLE_MECHANISM_PANEL_H
#include "MechanismController.h"
#include "Dxf/UiPanel.h"
#include "Dxf/UiScope.h"
namespace Dxf::GameplaySample
{
/**
 * 既存設定画面へ装置の操作部品を追加する。
 * Panelが部品、Scopeが購読を所有する。ControllerはScene内で両方より長く生存する。
 */
void BuildMechanismPanel(DUiPanel& Panel, FUiScope& Scope, FMechanismController& Controller);
} // namespace Dxf::GameplaySample
#endif
