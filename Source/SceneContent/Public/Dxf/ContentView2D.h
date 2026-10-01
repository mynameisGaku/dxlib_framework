// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_VIEW_2D_H
#define DXF_CONTENT_VIEW_2D_H
#include "Dxf/MathTypes.h"
namespace Dxf
{
/**
 * 2D物理のm・Y上を、画素・Y下へ写す表示設定。物理寸法を変更しない。
 */
struct FContentView2D
{
	/**
	 * World原点を表示する画素位置。
	 */
	FVector2 Origin{400, 300};
	/**
	 * 1mに対応する正の画素数。
	 */
	Toolbox::f32 PixelsPerMeter = 50;
	/**
	 * 分割表示に使う画素矩形。
	 */
	FIntRect ClipRect;
	/**
	 * 矩形外へ描かないか。
	 */
	bool bClip = false;
};
/**
 * 有限の原点・正の表示倍率・空でない任意クリップを検査する。
 * @param View 物理状態を変更しない表示条件。
 */
bool IsValidContentView2D(const FContentView2D& View) noexcept;
} // namespace Dxf
#endif
