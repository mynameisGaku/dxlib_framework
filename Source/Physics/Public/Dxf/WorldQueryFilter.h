// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_QUERY_FILTER_H
#define DXF_WORLD_QUERY_FILTER_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 問い合わせ（線分のRaycastClosest・範囲のOverlapAll）で調べるColliderの種類を、呼出しごとに指定する値。2D／3D共通。
 * 問い合わせの候補だけを絞り込み、物理的な衝突・接触応答・Snapshotの観察対象は変えない。
 * 接触・Triggerの組を決める双方向の衝突フィルター（FColliderCollisionFilter）とは別の、片方向の判定。
 */
struct FWorldQueryFilter
{
	/**
	 * 対象にする問い合わせカテゴリのビット集合。ColliderのQueryCategoryと1ビットでも重なれば対象。
	 * 既定は全ビット（絞り込みなし）。0は正常な「対象なし」で、結果は空になる（入力・状態・除外IDの検査は行う）。
	 */
	Toolbox::uint32 IncludeCategories = 0xffffffffu;
	/**
	 * Solid（EColliderResponse::Solid）のColliderを対象にするか。既定は対象にする。
	 */
	bool bIncludeSolid = true;
	/**
	 * Sensor（EColliderResponse::Sensor）のColliderを対象にするか。既定は対象にする。
	 * キャラクター移動（StepCharacter等）は指定にかかわらずSensorを対象にしない。
	 */
	bool bIncludeSensors = true;
	/**
	 * 自己除外（ExcludedBody）とは別に、もう一つのBodyの全Colliderを対象から外すか。ExcludeSecondBodyで設定する。
	 * World・スロット・世代が一致するBodyだけを外し、削除済み・旧世代のBodyの指定は何も外さない（例外にしない）。
	 * キャラクター移動が、乗っている動く床の追従の移動で、その床自身を障害物にしないために使う。
	 */
	bool bExcludeSecondBody = false;
	/**
	 * 外すBodyのWorldの識別子。
	 */
	Toolbox::uint64 SecondBodyWorld = 0;
	/**
	 * 外すBodyのスロット番号。
	 */
	Toolbox::size_t SecondBodyIndex = 0;
	/**
	 * 外すBodyの世代。
	 */
	Toolbox::uint64 SecondBodyGeneration = 0;
};
/**
 * 問い合わせの条件へ、もう一つ外すBodyを設定する（2D／3DのBodyのIDに使える）。
 * @param Filter 設定する条件。
 * @param Body 外すBody。
 */
template <typename TBodyId> void ExcludeSecondBody(FWorldQueryFilter& Filter, const TBodyId& Body) noexcept
{
	Filter.bExcludeSecondBody = true;
	Filter.SecondBodyWorld = Body.World;
	Filter.SecondBodyIndex = Body.Index;
	Filter.SecondBodyGeneration = Body.Generation;
}
} // namespace Dxf
#endif
