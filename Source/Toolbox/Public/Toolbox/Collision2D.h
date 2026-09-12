// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_COLLISION_2D_H
#define TOOLBOX_COLLISION_2D_H
#include "Toolbox/Vector2.h"
namespace Toolbox
{
/**
 * 内部を含む円。半径ゼロは点として扱う。
 */
struct FCircle2D
{
	/**
	 * ワールド座標の中心。
	 */
	FVector2 Center;
	/**
	 * 有限かつ非負の半径。
	 */
	f32 Radius = 0.5f;
};
/**
 * 軸に平行な矩形。辺長ゼロを許容する。
 */
struct FAABB2D
{
	/**
	 * 二軸の最小座標。
	 */
	FVector2 Min;
	/**
	 * 二軸の最大座標。
	 */
	FVector2 Max;
};
/**
 * 半径と座標が判定に使用できるか調べる。
 * @param Circle 検査する円。
 */
bool IsValid(const FCircle2D& Circle) noexcept;
/**
 * 座標が有限で、最小値と最大値の順序が正しいか調べる。
 * @param Box 検査する矩形。
 */
bool IsValid(const FAABB2D& Box) noexcept;
/**
 * 円同士の接触を含む交差。Toleranceは有限・非負のユークリッド距離。
 * 不正な入力はFExceptionで通知する。
 * @param A 一つ目の円。
 * @param B 二つ目の円。
 * @param Tolerance 許容する隙間の幅。
 */
bool Intersects(const FCircle2D& A, const FCircle2D& B, f32 Tolerance = 1e-5f);
/**
 * 円と矩形の最近点距離に基づく交差。
 * @param Circle 対象の円。
 * @param Box 対象の矩形。
 * @param Tolerance 有限・非負の許容距離。
 */
bool Intersects(const FCircle2D& Circle, const FAABB2D& Box, f32 Tolerance = 1e-5f);
/**
 * 矩形と円の順序を入れ替えた交差。
 * @param Box 対象の矩形。
 * @param Circle 対象の円。
 * @param Tolerance 有限・非負の許容距離。
 */
FORCEINLINE bool Intersects(const FAABB2D& Box, const FCircle2D& Circle, f32 Tolerance = 1e-5f)
{
	return Intersects(Circle, Box, Tolerance);
}
/**
 * 矩形同士の最短距離に基づく交差。斜めの隙間もユークリッド距離で比較する。
 * @param A 一つ目の矩形。
 * @param B 二つ目の矩形。
 * @param Tolerance 有限・非負の許容距離。
 */
bool Intersects(const FAABB2D& A, const FAABB2D& B, f32 Tolerance = 1e-5f);
} // namespace Toolbox
#endif
