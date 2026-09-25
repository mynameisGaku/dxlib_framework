// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHAPE_CONTACT_3D_H
#define TOOLBOX_SHAPE_CONTACT_3D_H
#include "Toolbox/Optional.h"
#include "Toolbox/Vector3.h"
namespace Toolbox
{
/**
 * 球と静止した対象の現在位置での接触・分離。移動は含まない。
 */
struct FShapeContact3D
{
	/**
	 * 表面間の符号付き距離（距離単位）。正は隙間、0は境界だけの接触、負は重なり。
	 * 負の大きさは、法線方向へ中心を動かして接触状態（0）へ戻すのに必要な距離。
	 */
	f64 Separation = 0;
	/**
	 * 分離が最も速く増える単位方向（対象から球の中心へ向く向き）。球の中心が対象の外なら最近点から中心への方向、
	 * 対象の内部なら最も近い面の外向き法線。同心や、同じ距離の面が複数ある場合など、方向を一つに決められなければ空。
	 */
	TOptional<FVector3> Normal;
};
} // namespace Toolbox
#endif
