// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_SHAPE_H
#define DXF_CHARACTER_SHAPE_H
namespace Dxf
{
/**
 * キャラクター移動の問い合わせに使う形状。
 */
enum class ECharacterShape
{
	/**
	 * 中心と半径の円／球（既定。従来の挙動）。
	 */
	Round,
	/**
	 * 中心線がUpに沿うカプセル。中心は中心線の中点で、両端は中心からUpの向きに±HalfHeight。
	 */
	Capsule,
};
} // namespace Dxf
#endif
