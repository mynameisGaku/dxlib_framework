// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_GROUND_STATE_H
#define DXF_CHARACTER_GROUND_STATE_H
namespace Dxf
{
/**
 * 足元の支持の状態。
 */
enum class ECharacterGroundState
{
	/**
	 * 接地距離の範囲に、上向きの支持面がない。
	 */
	Airborne,
	/**
	 * 歩ける傾斜（MaxSlopeAngle以下）の床に立っている。
	 */
	Walkable,
	/**
	 * 上向きだが急すぎる斜面に接している。立てず、重力で滑り落ちる。
	 */
	Steep,
};
} // namespace Dxf
#endif
