// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_QUERY_RESULT_ORDER_H
#define DXF_PRIVATE_PHYSICS_QUERY_RESULT_ORDER_H
#include "Toolbox/Utility.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 並べ替え（ヒープソート）。確保せず、要素の比較だけを使う。同じ値の順序は保たない。
 * @param Items 先頭。
 * @param Count 要素数。
 * @param Less 左を先に並べる場合にtrueを返す比較。
 */
template <typename T, typename TLess> void HeapSort_Internal(T* Items, Toolbox::size_t Count, TLess&& Less)
{
	// 根から下へ、子の大きい方と入れ替えて並びを保つ。
	auto SiftDown = [&](Toolbox::size_t Root, Toolbox::size_t End)
	{
		while (true)
		{
			Toolbox::size_t Child = Root * 2 + 1;
			if (Child >= End)
			{
				return;
			}
			if (Child + 1 < End && Less(Items[Child], Items[Child + 1]))
			{
				++Child;
			}
			if (!Less(Items[Root], Items[Child]))
			{
				return;
			}
			T Swapped = Items[Root];
			Items[Root] = Items[Child];
			Items[Child] = Swapped;
			Root = Child;
		}
	};
	if (Count < 2)
	{
		return;
	}
	for (Toolbox::size_t Start = Count / 2; Start > 0; --Start)
	{
		SiftDown(Start - 1, Count);
	}
	for (Toolbox::size_t End = Count - 1; End > 0; --End)
	{
		T Swapped = Items[0];
		Items[0] = Items[End];
		Items[End] = Swapped;
		SiftDown(0, End);
	}
}
/**
 * 最短候補を置き換えるかを返す。割合が小さい方、同じ割合ならスロットの小さい方を採用する
 * （候補の訪問順によらず、総当たりの昇順走査で最初に見つかる最短と同じ結果になる）。
 * @param bHasBest 既に候補があるか。
 * @param BestFraction 現在の候補の割合。
 * @param BestSlot 現在の候補のスロット。
 * @param Fraction 新しい候補の割合。
 * @param Slot 新しい候補のスロット。
 */
FORCEINLINE bool IsCloserHit_Internal(bool bHasBest, Toolbox::f64 BestFraction, Toolbox::size_t BestSlot,
                                      Toolbox::f64 Fraction, Toolbox::size_t Slot) noexcept
{
	if (!bHasBest)
	{
		return true;
	}
	return Fraction < BestFraction || (Fraction == BestFraction && Slot < BestSlot);
}
/**
 * 固定容量の接触集合へ、スロット昇順の先頭Capacity件を保って入れる。全件数TotalFoundも数える。
 * 満杯なら、より大きいスロットの末尾を押し出す（訪問順によらず、昇順走査の先頭Capacity件と同じ結果）。
 * @param Result 接触集合（Items・Count・TotalFound・Capacityを持つ）。
 * @param Item 入れる接触（Collider.Indexがスロット）。
 */
template <typename TSet, typename TItem> void InsertContactBySlot_Internal(TSet& Result, const TItem& Item)
{
	++Result.TotalFound;
	const Toolbox::size_t Slot = Item.Collider.Index;
	Toolbox::uint32 Position = Result.Count;
	if (Result.Count == TSet::Capacity)
	{
		if (Result.Items[TSet::Capacity - 1].Collider.Index < Slot)
		{
			return;
		}
		Position = TSet::Capacity - 1;
	}
	else
	{
		++Result.Count;
	}
	while (Position > 0 && Result.Items[Position - 1].Collider.Index > Slot)
	{
		Result.Items[Position] = Result.Items[Position - 1];
		--Position;
	}
	Result.Items[Position] = Item;
}
} // namespace Dxf::PhysicsPrivate
#endif
