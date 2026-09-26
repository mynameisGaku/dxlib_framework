// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SAMPLE_SCENES_H
#define DXF_UI_SAMPLE_SCENES_H
#include "UiSampleState.h"
#include "Dxf/Scene.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf::UiSample
{
/**
 * @param State Scene間で共有する表示データ。
 */
Toolbox::TUniquePtr<DScene> MakeTitleScene(Toolbox::TSharedPtr<FUiSampleState> State);
/**
 * @param b3D 3Dの球か、2Dの円か。 @param State 表示データ。
 */
Toolbox::TUniquePtr<DScene> MakePlayScene(bool b3D, Toolbox::TSharedPtr<FUiSampleState> State);
} // namespace Dxf::UiSample
#endif
