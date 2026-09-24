// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CONTACT_2D_H
#define TOOLBOX_CONTACT_2D_H
#include "Toolbox/Collision2D.h"
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 中心・半辺長・回転角で表す矩形。角度は反時計回りが正のラジアン。
 */
struct FOrientedBox2D
{
	/**
	 * ワールド座標の中心。
	 */
	FVector2 Center;
	/**
	 * 各軸に沿う半分の長さ。有限な非負値。
	 */
	FVector2 HalfExtents{0.5f, 0.5f};
	/**
	 * ラジアン単位の回転角。
	 */
	f32 Angle = 0;
};
/**
 * 二形状の最近傍から作る単一接触。法線は二つ目から一つ目へ向く。
 */
struct FContactPoint2D
{
	/**
	 * 両表面の中点。メートル単位。
	 */
	FVector2 Position;
	/**
	 * 一つ目を押し出す方向の単位法線。
	 */
	FVector2 Normal{1, 0};
	/**
	 * 表面間の符号付き距離。負は貫通深度の大きさ。
	 */
	f32 Separation = 0;
	/**
	 * 箱側の面・頂点を区別する安定ID。円同士は0。
	 */
	uint32 FeatureId = 0;
};
/**
 * 矩形の座標と寸法が判定に使用できるか調べる。
 * @param Box 検査する矩形。
 */
bool IsValid(const FOrientedBox2D& Box) noexcept;
/**
 * 円と回転矩形が重なるか（接触を含む）を調べる。接触点・法線は作らない。
 * 中心差はf64で求め、FindContactと同じ反時計回りの角度・逆回転規約で矩形の局所座標へ移す。
 * 許容距離は既定値を持たず、呼出し側が明示する（0で許容なし）。不正形状・負や非有限の許容距離はFException。
 * @param Circle 対象の円。半径0は点。
 * @param Box 対象の矩形。半幅0は辺・点。
 * @param Tolerance 円の半径へ加える有限・非負の距離。
 */
bool Intersects(const FCircle2D& Circle, const FOrientedBox2D& Box, f32 Tolerance);
/**
 * 円同士の接触を求める。不正な入力はFExceptionで通知する。
 * @param A 一つ目の円。
 * @param B 二つ目の円。
 */
FContactPoint2D FindContact(const FCircle2D& A, const FCircle2D& B);
/**
 * 円と回転矩形の接触を求める。不正な入力はFExceptionで通知する。
 * @param Circle 対象の円。
 * @param Box 対象の矩形。
 */
FContactPoint2D FindContact(const FCircle2D& Circle, const FOrientedBox2D& Box);
/**
 * 矩形と円の順序を入れ替えた接触。法線だけが反転する。
 * @param Box 対象の矩形。
 * @param Circle 対象の円。
 */
FORCEINLINE FContactPoint2D FindContact(const FOrientedBox2D& Box, const FCircle2D& Circle)
{
	FContactPoint2D Hit = FindContact(Circle, Box);
	Hit.Normal = -Hit.Normal;
	return Hit;
}
} // namespace Toolbox
#endif
