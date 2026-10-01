// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_MECHANISM_OVERLAY_H
#define DXF_SAMPLE_MECHANISM_OVERLAY_H
#include "Toolbox/Utility.h"
#include "Dxf/Font.h"
namespace Dxf
{
class FRenderContext;
}
namespace Dxf::GameplaySample
{
class DInteraction2DScene;
class DInteraction3DScene;
/**
 * 補間Bodyと取付Frameを同じ2D投影で描く。Sideは左0または右1。
 */
void DrawMechanismCourse2D(FRenderContext& Render, const DInteraction2DScene& Scene, Toolbox::int32 Side);
/**
 * 既に設定された3Dビューへ装置を描く。更新・要求送信は行わない。
 */
void DrawMechanismCourse3D(FRenderContext& Render, const DInteraction3DScene& Scene);
/**
 * 成功した物理観察を全画面文字へ表示する。描画は観察値を変更しない。
 * @param Render 2Dの描画窓口。
 * @param Scene 表示中の2Dコース。
 * @param Font Sceneが所有する文字資源。
 */
void DrawMechanismStatus(FRenderContext& Render, const DInteraction2DScene& Scene, const FFont& Font);
/**
 * 同じ観察文字を3Dコースへ表示する。
 * @param Render 全画面の2D描画窓口。
 * @param Scene 表示中の3Dコース。
 * @param Font Sceneが所有する文字資源。
 */
void DrawMechanismStatus(FRenderContext& Render, const DInteraction3DScene& Scene, const FFont& Font);
} // namespace Dxf::GameplaySample
#endif
