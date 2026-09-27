// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_SOLVER_PAIRS_H
#define DXF_PRIVATE_PHYSICS_SOLVER_PAIRS_H
#include "QueryCandidates.h"
#include "QueryResultOrder.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 接触を調べるColliderの組の候補（スロットの小さい方が先）。
 */
struct FColliderPair
{
	/**
	 * 小さい方のColliderスロット。
	 */
	Toolbox::size_t First = 0;
	/**
	 * 大きい方のColliderスロット。
	 */
	Toolbox::size_t Second = 0;
};

/**
 * 問い合わせの索引（Worldが自動で保つAABB木。別の木は作らない）から、接触を調べる組の候補を、配列へ保存せず通知する。
 * 候補は「動く側」のColliderから、その葉の境界をMarginと丸めの余白だけ広げた範囲で探す（範囲の重なりは対称なので、
 * 両方が動く側の組はスロットの小さい方からだけ数え、同じ組を二度通知しない）。通知はスロットの小さい方を先に渡し、
 * 組の間の順は木の構造で決まる。同じBodyの組と、両方が動く側でない組は通知しない。区分などの絞り込みは呼出し側が行う。
 * 索引に入れられないColliderがある、または座標が大きすぎる場合はfalseを返し、何も通知しない（呼出し側は参照経路を使う）。
 * 索引は呼出し側が現在の姿勢へ合わせておくこと。確保はしない。
 * @param Index 現在の姿勢へ合わせた索引。
 * @param Colliders Colliderの登録（bAlive・Bodyを持つ）。
 * @param Margin 接触を調べる距離（有限・非負）。
 * @param IsSource スロットのColliderが動く側かを返す関数。
 * @param Visit 組のスロット二つ（小さい方が先）を受け取る関数。
 */
template <Toolbox::int32 Dimension, typename TRecord, typename TIsSource, typename TVisit>
bool VisitIndexedPairs_Internal(const TQueryIndex<Dimension>& Index, const Toolbox::TVector<TRecord>& Colliders,
                                Toolbox::f64 Margin, TIsSource&& IsSource, TVisit&& Visit)
{
	const Toolbox::f64 MaxAbs = Index.GetMaxAbs();
	if (Index.HasUnindexed() || !(MaxAbs <= QuerySafeLimit))
	{
		return false;
	}
	// 葉の境界へ足す距離（接触の距離と、f64の境界の丸めの余白）。
	const Toolbox::f64 Reach = Margin + QueryInflation_Internal(Index, MaxAbs);
	FQueryVisitCounters Counters;
	for (Toolbox::size_t Slot = 0; Slot < Colliders.Size(); ++Slot)
	{
		if (!Colliders[Slot].bAlive || !IsSource(Slot))
		{
			continue;
		}
		const Toolbox::int32 Leaf = Index.GetLeaf(Slot);
		if (Leaf == TQueryIndex<Dimension>::None)
		{
			continue;
		}
		const TQueryBounds<Dimension> Area = Expanded_Internal(Index.GetTree().GetBounds(Leaf), Reach);
		Index.GetTree().Query(
		    [&](const TQueryBounds<Dimension>& Bounds)
		    {
			    return Overlaps_Internal(Bounds, Area);
		    },
		    [&](Toolbox::uint32 Item)
		    {
			    const Toolbox::size_t Other = Item;
			    if (Other == Slot || Colliders[Other].Body == Colliders[Slot].Body)
			    {
				    return;
			    }
			    // 両方が動く側の組は、スロットの小さい方の探索だけで入れる。
			    if (IsSource(Other) && Other < Slot)
			    {
				    return;
			    }
			    Visit(Toolbox::Min(Slot, Other), Toolbox::Max(Slot, Other));
		    },
		    Counters);
	}
	return true;
}
/**
 * VisitIndexedPairs_Internalの組を集め、(First, Second)の昇順（総当たりの二重ループと同じ順）へ並べる。
 * 索引を使えない場合はfalseを返し、Pairsは空。Pairsの容量は呼出し側が使い回す（増やす場合だけ確保する）。
 * @param Index 現在の姿勢へ合わせた索引。
 * @param Colliders Colliderの登録。
 * @param Margin 接触を調べる距離。
 * @param IsSource スロットのColliderが動く側かを返す関数。
 * @param Pairs 結果の組。
 */
template <Toolbox::int32 Dimension, typename TRecord, typename TIsSource>
bool CollectIndexedPairs_Internal(const TQueryIndex<Dimension>& Index, const Toolbox::TVector<TRecord>& Colliders,
                                  Toolbox::f64 Margin, TIsSource&& IsSource, Toolbox::TVector<FColliderPair>& Pairs)
{
	Pairs.Clear();
	const bool bIndexed = VisitIndexedPairs_Internal(Index, Colliders, Margin, IsSource,
	                                                 [&](Toolbox::size_t First, Toolbox::size_t Second)
	                                                 {
		                                                 FColliderPair Pair;
		                                                 Pair.First = First;
		                                                 Pair.Second = Second;
		                                                 Pairs.PushBack(Pair);
	                                                 });
	if (!bIndexed)
	{
		return false;
	}
	HeapSort_Internal(Pairs.Data(), Pairs.Size(),
	                  [](const FColliderPair& A, const FColliderPair& B)
	                  {
		                  return A.First == B.First ? A.Second < B.Second : A.First < B.First;
	                  });
	return true;
}
} // namespace Dxf::PhysicsPrivate
#endif
