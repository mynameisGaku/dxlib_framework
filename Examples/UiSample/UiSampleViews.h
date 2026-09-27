// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SAMPLE_VIEWS_H
#define DXF_UI_SAMPLE_VIEWS_H
#include "Dxf/RenderView3D.h"
#include "Dxf/UiWorldPanel.h"
#include "Toolbox/Vector.h"
namespace Dxf::UiSample
{
/**
 * 入力と描画が共用する、当該フレームの明示ビュー。
 */
// Width・Heightはこのフレームの描画先の寸法（2画面では左右へ等分する）。
Toolbox::TVector<FRenderView3D> MakeSampleViews(bool bSplit, Toolbox::int32 Width = 1280, Toolbox::int32 Height = 720);
/**
 * 2Dゲームのワールド→画面変換。
 */
FUiWorldToScreen2D MakeSampleTransform2D(bool bSplit, Toolbox::int32 Index);
} // namespace Dxf::UiSample
#endif
