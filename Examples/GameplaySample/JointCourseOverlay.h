// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_JOINT_COURSE_OVERLAY_H
#define DXF_SAMPLE_JOINT_COURSE_OVERLAY_H
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
 * 表示と同じ補間姿勢で重りとガイド線を描く。物理を更新しない。
 * @param Render 既存の2D描画窓口。
 * @param Scene 投影と同じSceneの仕掛け。
 * @param Side 全画面または分割の側。
 */
void DrawJointCourse2D(FRenderContext& Render, const DInteraction2DScene& Scene, Toolbox::int32 Side);
/**
 * 設定済みのViewで立体仕掛けを描く。物理を更新しない。
 * @param Render 既存の3D描画窓口。
 * @param Scene 同じSceneの仕掛け。
 */
void DrawJointCourse3D(FRenderContext& Render, const DInteraction3DScene& Scene);
/**
 * 接続状態と最後の成功Stepの距離を全画面UIへ描く。
 * @param Render 全画面の2D描画窓口。
 * @param Scene 2Dの観察先。
 * @param Font 既存Sceneが読み込んだ文字資源。
 */
void DrawJointStatus(FRenderContext& Render, const DInteraction2DScene& Scene, const FFont& Font);
/**
 * 接続状態と最後の成功Stepの距離を全画面UIへ描く。
 * @param Render 全画面の2D描画窓口。
 * @param Scene 3Dの観察先。
 * @param Font 既存Sceneが読み込んだ文字資源。
 */
void DrawJointStatus(FRenderContext& Render, const DInteraction3DScene& Scene, const FFont& Font);
} // namespace Dxf::GameplaySample
#endif
