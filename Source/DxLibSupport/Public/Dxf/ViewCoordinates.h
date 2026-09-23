// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_VIEW_COORDINATES_H
#define DXF_VIEW_COORDINATES_H
#include "Dxf/ProjectedPoint3D.h"
#include "Dxf/RenderGeometry3D.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * 指定ビューだけを使って投影する。Native状態、照明、遮蔽は参照しない。
 * 透視の眼平面・背後、不正なカメラ・寸法・矩形、非有限化は失敗する。
 * @param View 描画と同時点のビュー。 @param Width 描画先の幅。 @param Height 描画先の高さ。
 * @param World 投影するワールド位置。視野外の有限な結果はbInsideView=false。
 */
TResult<FProjectedPoint3D> ProjectWorldToScreen(const FRenderView3D& View, Toolbox::int32 Width, Toolbox::int32 Height, Toolbox::FVector3 World);
/**
 * 近接面上のStartから遠方面上のEndまでの線分を作る。始点はカメラ位置ではない。
 * 正常なViewport外は成功した空Optional。不正値や表現できない結果は失敗する。
 * @param View 描画と同時点のビュー。 @param Width 描画先の幅。 @param Height 描画先の高さ。
 * @param Screen 描画先全体の連続座標。整数マウス座標へ半画素の補正を加えない。
 */
TResult<Toolbox::TOptional<FLine3D>> MakeViewPickSegment(const FRenderView3D& View, Toolbox::int32 Width, Toolbox::int32 Height, FVector2 Screen);
} // namespace Dxf
#endif
