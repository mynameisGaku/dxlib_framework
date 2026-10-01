// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_CONTENT_COURSE_PANEL_H
#define DXF_SAMPLE_CONTENT_COURSE_PANEL_H
#include "ContentCourseControls.h"
#include "Dxf/UiPanel.h"
#include "Dxf/ContentParameterValue.h"
#include "Dxf/UiScope.h"
namespace Dxf::GameplaySample
{
/**
 * 既存UIから操作値を更新する。Worldを直接操作しない。
 * @param Panel 部品の所有先。
 * @param Scope 購読の寿命。
 * @param Controls Sceneが所有する操作値。
 */
/**
 * @param Controls 次の生成だけに使う型付き公開値。共有定義を変更しない。
 */
Toolbox::TVector<FContentParameterValue> MakeContentCourseOverrides(const FContentCourseControls& Controls);
void BuildContentCoursePanel(DUiPanel& Panel, FUiScope& Scope, FContentCourseControls& Controls);
} // namespace Dxf::GameplaySample
#endif
