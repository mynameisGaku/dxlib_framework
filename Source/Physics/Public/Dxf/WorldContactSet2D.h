// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_CONTACT_SET_2D_H
#define DXF_WORLD_CONTACT_SET_2D_H
#include "Dxf/WorldContact2D.h"
#include "Toolbox/Array.h"
namespace Dxf
{
/**
 * QueryContactsの結果。容量が固定で、問い合わせで配列を確保しない。Worldや内部配列を参照しない値。
 */
struct FWorldContactSet2D
{
	/**
	 * 保持できる接触の最大数。
	 */
	static constexpr Toolbox::uint32 Capacity = 32;
	/**
	 * 保持した接触（Colliderスロット昇順の先頭Count件）。
	 */
	Toolbox::TArray<FWorldContact2D, Capacity> Items;
	/**
	 * Itemsのうち有効な件数（Capacity以下）。
	 */
	Toolbox::uint32 Count = 0;
	/**
	 * 条件に合った全件数。Countより大きければ、容量を超えた接触を保持できていない。
	 */
	Toolbox::uint32 TotalFound = 0;
	/**
	 * 条件に合った全件を保持できたか。falseなら結果は不完全で、全件の制約として使えない。
	 */
	FORCEINLINE bool IsComplete() const noexcept
	{
		return TotalFound == Count;
	}
};
} // namespace Dxf
#endif
