// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_RECOVERY_STATUS_H
#define DXF_CHARACTER_RECOVERY_STATUS_H
namespace Dxf
{
/**
 * 初期重なりの解消の結果。解消できなかった場合、中心は元の位置のまま返す。
 */
enum class ECharacterRecoveryStatus
{
	/**
	 * 重なり（負の符号付き距離）はなかった。境界だけの接触は重なりではない。
	 */
	NoOverlap,
	/**
	 * すべての重なりを解消した。中心は各重なりの法線方向へ、接触余裕を保つ位置まで動いた。
	 */
	Resolved,
	/**
	 * 重なりの解消方向を一つに決められない（同心、同じ距離の面が複数など）。
	 */
	Ambiguous,
	/**
	 * 解消に必要な合計距離がMaxRecoveryDistanceを超える。
	 */
	TooDeep,
	/**
	 * 解消の移動経路が別のColliderに当たる（挟み込みなど）。
	 */
	Blocked,
	/**
	 * 反復上限までに重なりが残った（挟み込み・複数の重なり）。
	 */
	IterationLimit,
	/**
	 * 接触が結果の容量を超え、全件を確認できない。
	 */
	ContactLimit,
	/**
	 * 問い合わせの上限に達した。
	 */
	QueryLimit,
};
} // namespace Dxf
#endif
