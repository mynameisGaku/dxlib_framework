// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_MODEL_VIEWER_PICKING_EXAMPLE_H
#define DXF_MODEL_VIEWER_PICKING_EXAMPLE_H
#include "Dxf/ViewCoordinates.h"
namespace Dxf::ModelViewer
{
/**
 * 指定ビューで球・箱の近い方を選ぶ例。0が球、1が箱、-1は領域外または非交差。
 * 同距離は先に検査する球を選ぶ。SceneやNativeの状態は変更しない。
 * @param View 表示と同時点のビュー。 @param Width 描画先の幅。 @param Height 描画先の高さ。
 * @param Screen 描画先全体のクリック位置。 @param Sphere 球の表示形状。 @param Box 箱の表示形状。
 */
TResult<Toolbox::int32> PickExampleShapes(const FRenderView3D& View, Toolbox::int32 Width, Toolbox::int32 Height, FVector2 Screen, const Toolbox::FSphere& Sphere, const Toolbox::FOBB& Box);
} // namespace Dxf::ModelViewer
#endif
