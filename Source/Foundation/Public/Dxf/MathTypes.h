#pragma once
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 2次元の座標または方向を表す。
 */
struct FVector2
{
	/**
	 * X座標。
	 */
	Toolbox::f32 X = 0.0f;
	/**
	 * Y座標。
	 */
	Toolbox::f32 Y = 0.0f;
};
/**
 * RGBA各8ビットの描画色を表す。
 */
struct FColor
{
	/**
	 * 赤成分。
	 */
	Toolbox::uint8 R = 255;
	/**
	 * 緑成分。
	 */
	Toolbox::uint8 G = 255;
	/**
	 * 青成分。
	 */
	Toolbox::uint8 B = 255;
	/**
	 * アルファ成分。
	 */
	Toolbox::uint8 A = 255;
};
/**
 * 整数座標の矩形領域を表す。
 */
struct FIntRect
{
	/**
	 * 左端の座標。
	 */
	Toolbox::int32 Left = 0;
	/**
	 * 上端の座標。
	 */
	Toolbox::int32 Top = 0;
	/**
	 * 右端の座標。
	 */
	Toolbox::int32 Right = 0;
	/**
	 * 下端の座標。
	 */
	Toolbox::int32 Bottom = 0;
};
} // namespace Dxf
