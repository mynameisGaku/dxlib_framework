// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_BODY_TYPE_H
#define DXF_PHYSICS_BODY_TYPE_H
namespace Dxf
{
/**
 * 剛体の運動区分。質量・慣性の扱いと外力への応答を決める。
 */
enum class EBodyType
{
	/**
	 * 無限質量として扱い、位置も速度も物理更新しない。
	 */
	Static,
	/**
	 * 外力を受けず、指定速度どおりに動く。相手へ速度を伝える。
	 */
	Kinematic,
	/**
	 * 力・トルク・重力・Impulseで動く。
	 */
	Dynamic
};
/**
 * 線形CCDの対応状態。
 */
enum class EContinuousSupport
{
	/**
	 * 直線移動の最初接触を求められる。
	 */
	Supported,
	/**
	 * 形状の組み合わせが対象外。
	 */
	UnsupportedPair,
	/**
	 * 回転を含むため直線Sweepでは扱わない。
	 */
	UnsupportedRotation
};
} // namespace Dxf
#endif
