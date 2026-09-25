// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_QUERY_INDEX_H
#define DXF_PRIVATE_PHYSICS_QUERY_INDEX_H
#include "QueryTree.h"
#include "Dxf/WorldQueryDiagnostics.h"
#include "Toolbox/Vector.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 余裕を持たせた境界の、形状の境界からの広がり（World距離単位）。
 */
constexpr Toolbox::f64 QueryFatMargin = 0.1;
/**
 * 入れ直すときに移動の向きへ広げる距離の上限（World距離単位、各軸）。瞬間移動で境界が過大にならないようにする。
 */
constexpr Toolbox::f64 QueryMaxPrediction = 2.0;

/**
 * World問い合わせの索引。Colliderのスロットごとに葉・索引外の状態・所有Bodyの一覧（スロット昇順）を持つ。
 * Worldの形状・Bodyの所有者はWorldのままで、ここは検索用の派生情報だけを持つ。変更系は事前に予約した領域だけを使う。
 */
template <Toolbox::int32 Dimension> class TQueryIndex
{
public:
	/**
	 * 無効な番号。
	 */
	static constexpr Toolbox::int32 None = -1;
	/**
	 * Bodyのスロット数の分だけ一覧の先頭を用意する（倍々に広げ、未使用の分は空の一覧）。確保失敗は例外で、状態は変わらない。
	 * @param BodySlots 必要なBodyのスロット数。
	 */
	void ReserveBodies(Toolbox::size_t BodySlots)
	{
		if (BodySlots > m_BodyFirst.Size())
		{
			m_BodyFirst.Resize(Grow_Internal(m_BodyFirst.Size(), BodySlots));
		}
	}
	/**
	 * 新しいBodyのスロットを空の一覧にする。ReserveBodiesで用意済みであること。
	 * @param BodySlot Bodyのスロット。
	 */
	void ResetBody(Toolbox::size_t BodySlot) noexcept
	{
		m_BodyFirst[BodySlot].First = None;
	}
	/**
	 * Colliderのスロット数と生存数の分だけ予約する。確保失敗は例外で、状態は変わらない。
	 * @param ColliderSlots 必要なColliderのスロット数。
	 * @param AliveColliders 同時に生存するColliderの数。
	 */
	void ReserveColliders(Toolbox::size_t ColliderSlots, Toolbox::size_t AliveColliders)
	{
		m_Tree.Reserve(AliveColliders);
		if (ColliderSlots > m_Slots.Size())
		{
			m_Slots.Resize(Grow_Internal(m_Slots.Size(), ColliderSlots));
		}
	}
	/**
	 * 追加したColliderを所有Bodyの一覧と索引へ入れる。ReserveCollidersで予約済みであること。
	 * @param Slot Colliderのスロット。
	 * @param BodySlot 所有Bodyのスロット。
	 * @param Shape 現在の姿勢での境界。
	 */
	void Attach(Toolbox::size_t Slot, Toolbox::size_t BodySlot, const TQueryShapeBounds<Dimension>& Shape) noexcept
	{
		FSlot& Entry = m_Slots[Slot];
		Entry = FSlot{};
		Entry.Body = static_cast<Toolbox::int32>(BodySlot);
		// 所有Bodyの一覧へ、スロット昇順を保って入れる。
		Toolbox::int32 Previous = None;
		Toolbox::int32 Next = m_BodyFirst[BodySlot].First;
		while (Next != None && Next < static_cast<Toolbox::int32>(Slot))
		{
			Previous = Next;
			Next = m_Slots[static_cast<Toolbox::size_t>(Next)].BodyNext;
		}
		Entry.BodyPrevious = Previous;
		Entry.BodyNext = Next;
		if (Previous == None)
		{
			m_BodyFirst[BodySlot].First = static_cast<Toolbox::int32>(Slot);
		}
		else
		{
			m_Slots[static_cast<Toolbox::size_t>(Previous)].BodyNext = static_cast<Toolbox::int32>(Slot);
		}
		if (Next != None)
		{
			m_Slots[static_cast<Toolbox::size_t>(Next)].BodyPrevious = static_cast<Toolbox::int32>(Slot);
		}
		Place_Internal(Slot, Shape);
		++m_Inserts;
	}
	/**
	 * 削除したColliderを所有Bodyの一覧と索引から外す。
	 * @param Slot Colliderのスロット。
	 */
	void Detach(Toolbox::size_t Slot) noexcept
	{
		FSlot& Entry = m_Slots[Slot];
		if (Entry.BodyPrevious == None)
		{
			m_BodyFirst[static_cast<Toolbox::size_t>(Entry.Body)].First = Entry.BodyNext;
		}
		else
		{
			m_Slots[static_cast<Toolbox::size_t>(Entry.BodyPrevious)].BodyNext = Entry.BodyNext;
		}
		if (Entry.BodyNext != None)
		{
			m_Slots[static_cast<Toolbox::size_t>(Entry.BodyNext)].BodyPrevious = Entry.BodyPrevious;
		}
		Unplace_Internal(Slot);
		Entry = FSlot{};
		++m_Removals;
	}
	/**
	 * 姿勢が変わったColliderの境界を確認し、余裕を持たせた境界から出ていれば入れ直す。
	 * @param Slot Colliderのスロット。
	 * @param Shape 現在の姿勢での境界。
	 */
	void Refresh(Toolbox::size_t Slot, const TQueryShapeBounds<Dimension>& Shape) noexcept
	{
		++m_Refreshes;
		FSlot& Entry = m_Slots[Slot];
		if (Entry.Leaf != None && Shape.bIndexable)
		{
			const TQueryBounds<Dimension>& Old = m_Tree.GetBounds(Entry.Leaf);
			if (Contains_Internal(Old, Shape.Tight))
			{
				return;
			}
			m_Tree.Replace(Entry.Leaf, Predicted_Internal(Old, Shape.Tight));
			++m_Reinserts;
			return;
		}
		// 索引の内外が変わる場合は、外してから置き直す（ノード数は予約済みの範囲内）。
		Unplace_Internal(Slot);
		Place_Internal(Slot, Shape);
		++m_Reinserts;
	}
	/**
	 * Bodyの一覧の先頭のColliderスロットを返す（なければNone）。
	 * @param BodySlot Bodyのスロット。
	 */
	FORCEINLINE Toolbox::int32 GetFirstCollider(Toolbox::size_t BodySlot) const noexcept
	{
		return m_BodyFirst[BodySlot].First;
	}
	/**
	 * 同じBodyの次のColliderスロットを返す（なければNone）。
	 * @param Slot Colliderのスロット。
	 */
	FORCEINLINE Toolbox::int32 GetNextCollider(Toolbox::size_t Slot) const noexcept
	{
		return m_Slots[Slot].BodyNext;
	}
	/**
	 * 索引に入れられないColliderを順に渡し、関数がtrueを返したらtrueを返す。
	 * @param Test Colliderのスロットを受け取る関数。
	 */
	template <typename TTest> bool AnyUnindexed(TTest&& Test) const
	{
		Toolbox::int32 Current = m_UnindexedFirst;
		while (Current != None)
		{
			if (Test(static_cast<Toolbox::size_t>(Current)))
			{
				return true;
			}
			Current = m_Slots[static_cast<Toolbox::size_t>(Current)].UnindexedNext;
		}
		return false;
	}
	/**
	 * 索引に入れられないColliderがあるかを返す。
	 */
	FORCEINLINE bool HasUnindexed() const noexcept
	{
		return m_UnindexedFirst != None;
	}
	/**
	 * 索引全体の座標の絶対値の最大を返す（空は0）。
	 */
	FORCEINLINE Toolbox::f64 GetMaxAbs() const noexcept
	{
		const Toolbox::int32 Root = m_Tree.GetRoot();
		return Root == TQueryTree<Dimension>::NullNode ? 0 : MaxAbs_Internal(m_Tree.GetBounds(Root));
	}
	/**
	 * 木を返す（走査・試験用）。
	 */
	FORCEINLINE const TQueryTree<Dimension>& GetTree() const noexcept
	{
		return m_Tree;
	}
	/**
	 * Colliderの葉を返す（索引外はNone、試験用）。
	 * @param Slot Colliderのスロット。
	 */
	FORCEINLINE Toolbox::int32 GetLeaf(Toolbox::size_t Slot) const noexcept
	{
		return Slot < m_Slots.Size() ? m_Slots[Slot].Leaf : None;
	}
	/**
	 * 状態と更新の累計を診断へ書き出す。
	 * @param Diagnostics 書き出し先。
	 * @param ColliderSlots Colliderのスロット数。
	 * @param AliveColliders 生存しているColliderの数。
	 */
	void Describe(FWorldQueryDiagnostics& Diagnostics, Toolbox::size_t ColliderSlots,
	              Toolbox::size_t AliveColliders) const noexcept
	{
		Diagnostics.ColliderSlots = ColliderSlots;
		Diagnostics.AliveColliders = AliveColliders;
		Diagnostics.IndexedColliders = m_Tree.GetLeafCount();
		Diagnostics.UnindexedColliders = m_UnindexedCount;
		Diagnostics.IndexNodes = m_Tree.GetNodeCount();
		Diagnostics.IndexHeight = static_cast<Toolbox::uint64>(m_Tree.GetHeight());
		Diagnostics.IndexMemoryBytes =
		    m_Tree.GetMemoryBytes() + m_Slots.Size() * sizeof(FSlot) + m_BodyFirst.Size() * sizeof(FBodyHead);
		Diagnostics.IndexInserts = m_Inserts;
		Diagnostics.IndexRemovals = m_Removals;
		Diagnostics.IndexRefreshes = m_Refreshes;
		Diagnostics.IndexReinserts = m_Reinserts;
	}
	/**
	 * 更新の累計を0へ戻す。
	 */
	void ResetCounters() noexcept
	{
		m_Inserts = 0;
		m_Removals = 0;
		m_Refreshes = 0;
		m_Reinserts = 0;
	}

private:
	// Colliderスロットごとの派生情報。
	struct FSlot
	{
		// 所有Bodyのスロット。
		Toolbox::int32 Body = None;
		// 同じBodyの前のCollider（スロット昇順）。
		Toolbox::int32 BodyPrevious = None;
		// 同じBodyの次のCollider。
		Toolbox::int32 BodyNext = None;
		// 索引の葉（索引外はNone）。
		Toolbox::int32 Leaf = None;
		// 索引に入れられないか。
		bool bUnindexed = false;
		// 索引に入れられない一覧の前。
		Toolbox::int32 UnindexedPrevious = None;
		// 索引に入れられない一覧の次。
		Toolbox::int32 UnindexedNext = None;
	};
	// Bodyスロットごとの一覧の先頭。
	struct FBodyHead
	{
		// 先頭のColliderスロット（空はNone）。
		Toolbox::int32 First = None;
	};
	// 予約する容量（倍々に増やす）。
	static Toolbox::size_t Grow_Internal(Toolbox::size_t Current, Toolbox::size_t Needed) noexcept
	{
		Toolbox::size_t Target = Current * 2;
		if (Target < Needed)
		{
			Target = Needed;
		}
		return Target < 16 ? 16 : Target;
	}
	// 入れ直す葉の余裕を持たせた境界。前の境界の中心から動いた向きへ、その2倍（各軸QueryMaxPrediction以下）だけ広げる。
	// 同じ向きへ動き続ける形状（落下・歩行）の入れ直しを減らす。広げるのは候補の範囲だけで、結果は変わらない。
	static TQueryBounds<Dimension> Predicted_Internal(const TQueryBounds<Dimension>& Old,
	                                                  const TQueryBounds<Dimension>& Tight) noexcept
	{
		TQueryBounds<Dimension> Fat = Expanded_Internal(Tight, QueryFatMargin);
		for (Toolbox::int32 Axis = 0; Axis < Dimension; ++Axis)
		{
			const Toolbox::f64 Moved = ((Tight.Min[Axis] + Tight.Max[Axis]) - (Old.Min[Axis] + Old.Max[Axis])) * 0.5;
			const Toolbox::f64 Extension = Toolbox::Min(Toolbox::Abs(Moved) * 2, QueryMaxPrediction);
			if (Moved < 0)
			{
				Fat.Min[Axis] -= Extension;
			}
			else
			{
				Fat.Max[Axis] += Extension;
			}
		}
		return Fat;
	}
	// 境界に応じて木または索引外の一覧へ入れる。
	void Place_Internal(Toolbox::size_t Slot, const TQueryShapeBounds<Dimension>& Shape) noexcept
	{
		FSlot& Entry = m_Slots[Slot];
		if (Shape.bIndexable)
		{
			Entry.Leaf =
			    m_Tree.Insert(Expanded_Internal(Shape.Tight, QueryFatMargin), static_cast<Toolbox::uint32>(Slot));
			return;
		}
		Entry.bUnindexed = true;
		Entry.UnindexedPrevious = None;
		Entry.UnindexedNext = m_UnindexedFirst;
		if (m_UnindexedFirst != None)
		{
			m_Slots[static_cast<Toolbox::size_t>(m_UnindexedFirst)].UnindexedPrevious =
			    static_cast<Toolbox::int32>(Slot);
		}
		m_UnindexedFirst = static_cast<Toolbox::int32>(Slot);
		++m_UnindexedCount;
	}
	// 木または索引外の一覧から外す。
	void Unplace_Internal(Toolbox::size_t Slot) noexcept
	{
		FSlot& Entry = m_Slots[Slot];
		if (Entry.Leaf != None)
		{
			m_Tree.Remove(Entry.Leaf);
			Entry.Leaf = None;
		}
		if (Entry.bUnindexed)
		{
			if (Entry.UnindexedPrevious == None)
			{
				m_UnindexedFirst = Entry.UnindexedNext;
			}
			else
			{
				m_Slots[static_cast<Toolbox::size_t>(Entry.UnindexedPrevious)].UnindexedNext = Entry.UnindexedNext;
			}
			if (Entry.UnindexedNext != None)
			{
				m_Slots[static_cast<Toolbox::size_t>(Entry.UnindexedNext)].UnindexedPrevious = Entry.UnindexedPrevious;
			}
			Entry.bUnindexed = false;
			Entry.UnindexedPrevious = None;
			Entry.UnindexedNext = None;
			--m_UnindexedCount;
		}
	}
	// 木。
	TQueryTree<Dimension> m_Tree;
	// Colliderスロットごとの派生情報。
	Toolbox::TVector<FSlot> m_Slots;
	// Bodyスロットごとの一覧の先頭。
	Toolbox::TVector<FBodyHead> m_BodyFirst;
	// 索引に入れられない一覧の先頭。
	Toolbox::int32 m_UnindexedFirst = None;
	// 索引に入れられないColliderの数。
	Toolbox::size_t m_UnindexedCount = 0;
	// 追加の累計。
	Toolbox::uint64 m_Inserts = 0;
	// 削除の累計。
	Toolbox::uint64 m_Removals = 0;
	// 境界の確認の累計。
	Toolbox::uint64 m_Refreshes = 0;
	// 入れ直しの累計。
	Toolbox::uint64 m_Reinserts = 0;
};
} // namespace Dxf::PhysicsPrivate
#endif
