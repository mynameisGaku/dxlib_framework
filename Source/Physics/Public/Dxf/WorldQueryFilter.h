// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_QUERY_FILTER_H
#define DXF_WORLD_QUERY_FILTER_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 線分問い合わせで調べるColliderの種類を、呼出しごとに指定する値。2D／3D共通。
 * 問い合わせの候補だけを絞り込み、物理的な衝突・接触応答・Snapshotの観察対象は変えない。
 * 既存の双方向の衝突フィルター（Toolbox::FCollisionFilter）とは別の、片方向の判定。
 */
struct FWorldQueryFilter
{
	/**
	 * 対象にする問い合わせカテゴリのビット集合。ColliderのQueryCategoryと1ビットでも重なれば対象。
	 * 既定は全ビット（絞り込みなし）。0は正常な「対象なし」で、結果は空になる。
	 */
	Toolbox::uint32 IncludeCategories = 0xffffffffu;
};
} // namespace Dxf
#endif
