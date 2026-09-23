// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PROJECTED_POINT_3D_H
#define DXF_PROJECTED_POINT_3D_H
#include "Dxf/MathTypes.h"
namespace Dxf
{
/**
 * ワールド位置を投影した描画先座標。遮蔽の有無は判定しない。
 */
struct FProjectedPoint3D
{
	/**
	 * 描画先左上原点の連続ピクセル座標。Viewport局所座標ではない。
	 */
	FVector2 Screen;
	/**
	 * 近接面0、遠方面1の投影深度。ワールド距離ではない。
	 */
	Toolbox::f64 Depth = 0;
	/**
	 * 矩形の半開区間と近接面～遠方面に入っているか。遮蔽とは無関係。
	 */
	bool bInsideView = false;
};
} // namespace Dxf
#endif
