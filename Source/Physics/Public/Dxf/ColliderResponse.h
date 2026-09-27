// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_COLLIDER_RESPONSE_H
#define DXF_PHYSICS_COLLIDER_RESPONSE_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * Colliderが重なりにどう応じるか。2D／3D共通。
 */
enum class EColliderResponse : Toolbox::uint8
{
	/**
	 * 物理的に押し返す形状（既定）。Solid同士の組だけが反発・摩擦・位置補正・連続衝突の停止を受ける。
	 */
	Solid,
	/**
	 * 重なりを観測するだけの形状。Sensorを含む組は拘束・反発・摩擦・位置補正・連続衝突の停止を行わず、Bodyを起こさない。
	 * キャラクター移動の問い合わせは常にSensorを障害物にしない。
	 */
	Sensor
};
/**
 * 接触・Triggerの組を調べるかを決める、Colliderの衝突カテゴリと相手のマスク。2D／3D共通。
 * 問い合わせ用のQueryCategoryとは独立で、問い合わせの対象には影響しない。
 * 組は、互いのカテゴリが相手のマスクに1ビットでも含まれる場合だけ調べる（両側の許可が必要な対称な規則）。
 */
struct FColliderCollisionFilter
{
	/**
	 * このColliderの衝突カテゴリのビット集合。0はどの組にもならない。
	 */
	Toolbox::uint32 Category = 1u;
	/**
	 * 組を作る相手のカテゴリのビット集合。既定は全ビット。
	 */
	Toolbox::uint32 Mask = 0xffffffffu;
};
/**
 * 二つのColliderの衝突フィルターが互いに組を許すか。
 * @param A 一つ目のColliderのフィルター。
 * @param B 二つ目のColliderのフィルター。
 */
FORCEINLINE constexpr bool AllowsCollisionPair(const FColliderCollisionFilter& A,
                                               const FColliderCollisionFilter& B) noexcept
{
	return (A.Category & B.Mask) != 0 && (B.Category & A.Mask) != 0;
}
} // namespace Dxf
#endif
