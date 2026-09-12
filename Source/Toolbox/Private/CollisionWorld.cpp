// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/CollisionWorld.h"
#include "Toolbox/Atomic.h"
namespace Toolbox
{
/**
 * 登録スロット、形状ごとの境界箱、自動構築される空間分割を保持する。
 */
struct FCollisionWorld::FImpl
{
	/**
	 * 一つの登録を保持する再利用可能なスロット。
	 */
	struct FEntry
	{
		/**
		 * スロットが所有する衝突形状。
		 */
		TOptional<FCollisionShape> Shape;
		/**
		 * 形状またはノードを囲む境界箱。
		 */
		FAABB Box;
		/**
		 * 相手カテゴリの選別条件。
		 */
		FCollisionFilter Filter;
		/**
		 * スロット再使用を区別する世代。
		 */
		uint64 Generation = 1;
	};
	/**
	 * 分割領域と、この領域をまたぐ登録を保持するノード。
	 */
	struct FNode
	{
		/**
		 * 形状またはノードを囲む境界箱。
		 */
		FAABB Box;
		/**
		 * このノードに直接保持する登録番号。
		 */
		TVector<size_t> Items;
		/**
		 * 子ノード番号。最大値は未使用。
		 */
		TArray<size_t, 8> Children;
		FNode()
		{
			Children.Fill(TNumericLimits<size_t>::Max());
		}
	};
	/**
	 * 世代付きの衝突形状スロット。
	 */
	TVector<FEntry> Entries;
	/**
	 * 再帰分割で構築したノード配列。
	 */
	TVector<FNode> Nodes;
	/**
	 * 直前の探索と分割の統計。
	 */
	FCollisionStats Stats;
	/**
	 * このワールド固有の識別子。
	 */
	uint64 Domain = 0;
	/**
	 * 登録変更により再構築が必要か。
	 */
	bool Dirty = true;
	/**
	 * 他のワールドで発行されたIDと、削除済みのIDを拒否する。
	 */
	bool Valid(FColliderId Id) const noexcept
	{
		return Id.World == Domain && Id.Index < Entries.Size() && Entries[Id.Index].Shape &&
		       Entries[Id.Index].Generation == Id.Generation;
	}
	/**
	 * スロット番号から現在のIDを作る。
	 */
	FColliderId Id(size_t Index) const noexcept
	{
		return {Domain, Index, Entries[Index].Generation};
	}
	/**
	 * 一つの子領域に完全に収まる登録だけを再帰的に振り分ける。
	 */
	size_t BuildNode(FAABB Box, const TVector<size_t>& Items, int32 Depth)
	{
		/**
		 * 現在の要素または作成先の番号。
		 */
		const size_t Index = Nodes.Size();
		Nodes.EmplaceBack();
		Nodes[Index].Box = Box;
		if (Items.Size() <= 8 || Depth >= 10 || Stats.Index == ESpatialIndex::Direct)
		{
			Nodes[Index].Items = Items;
			return Index;
		}
		/**
		 * XY平面での四分割を選ぶか。
		 */
		const bool Planar = Stats.Index == ESpatialIndex::Quadtree;
		/**
		 * 分割方式に応じた子領域数。
		 */
		const size_t ChildCount = Planar ? 4 : 8;
		/**
		 * 分割面が交わる境界箱の中心。
		 */
		const FVector3 Middle = Box.Center();
		/**
		 * 分割後の各子領域の境界箱。
		 */
		TArray<FAABB, 8> Boxes;
		TArray<TVector<size_t>, 8> Groups;
		for (size_t Child = 0; Child < ChildCount; ++Child)
		{
			Boxes[Child] = {{Child & 1 ? Middle.X : Box.Min.X, Child & 2 ? Middle.Y : Box.Min.Y,
			                 !Planar && (Child & 4) ? Middle.Z : Box.Min.Z},
			                {Child & 1 ? Box.Max.X : Middle.X, Child & 2 ? Box.Max.Y : Middle.Y,
			                 Planar || (Child & 4) ? Box.Max.Z : Middle.Z}};
		}
		for (size_t Item : Items)
		{
			/**
			 * 一つの子領域に完全に収まったか。
			 */
			bool Assigned = false;
			for (size_t Child = 0; Child < ChildCount; ++Child)
			{
				if (Boxes[Child].Contains(Entries[Item].Box))
				{
					Groups[Child].PushBack(Item);
					Assigned = true;
					break;
				}
			}
			if (!Assigned)
			{
				Nodes[Index].Items.PushBack(Item);
			}
		}
		for (size_t Child = 0; Child < ChildCount; ++Child)
		{
			if (!Groups[Child].IsEmpty())
			{
				/**
				 * 再帰構築した子ノード番号。
				 */
				const size_t ChildIndex = BuildNode(Boxes[Child], Groups[Child], Depth + 1);
				Nodes[Index].Children[Child] = ChildIndex;
			}
		}
		return Index;
	}
	/**
	 * 変更がある時だけ、全登録を収めるルートと分割を再構築する。
	 */
	void Rebuild()
	{
		if (!Dirty)
		{
			return;
		}
		Nodes.Clear();
		/**
		 * 形状が存在する登録番号。
		 */
		TVector<size_t> Active;
		/**
		 * 形状またはノードを囲む境界箱。
		 */
		FAABB Box{};
		for (size_t I = 0; I < Entries.Size(); ++I)
		{
			if (Entries[I].Shape)
			{
				Box = Active.IsEmpty() ? Entries[I].Box : Box.Merged(Entries[I].Box);
				Active.PushBack(I);
			}
		}
		Stats.Index = ESpatialIndex::Direct;
		if (Active.Size() > 8)
		{
			/**
			 * 領域の三軸の幅。
			 */
			const FVector3 Extents = Box.Max - Box.Min;
			Stats.Index =
			    Extents.Z <= (Extents.X + Extents.Y) * 0.001f ? ESpatialIndex::Quadtree : ESpatialIndex::Octree;
		}
		if (!Active.IsEmpty())
		{
			BuildNode(Box, Active, 0);
		}
		Stats.Nodes = Nodes.Size();
		Dirty = false;
	}
	/**
	 * 交差するノードだけを訪れ、境界箱の一致する候補を集める。
	 */
	void Candidates(size_t NodeIndex, const FAABB& Box, TVector<size_t>& Result) const
	{
		/**
		 * 現在探索する分割ノード。
		 */
		const auto& Node = Nodes[NodeIndex];
		if (!Node.Box.Intersects(Box))
		{
			return;
		}
		for (size_t Item : Node.Items)
		{
			if (Entries[Item].Box.Intersects(Box))
			{
				Result.PushBack(Item);
			}
		}
		for (size_t Child : Node.Children)
		{
			if (Child != TNumericLimits<size_t>::Max())
			{
				Candidates(Child, Box, Result);
			}
		}
	}
	/**
	 * 問い合わせ統計をリセットし、古い空間分割を更新する。
	 */
	void Prepare()
	{
		Rebuild();
		Stats.Candidates = 0;
		Stats.NarrowTests = 0;
	}
	/**
	 * 接触許容誤差ぶんだけ検索範囲を広げる。
	 */
	FAABB Expanded(FAABB Box) const noexcept
	{
		/**
		 * 接触許容誤差を各軸へ広げた幅。
		 */
		const FVector3 Margin{1e-5f, 1e-5f, 1e-5f};
		return {Box.Min - Margin, Box.Max + Margin};
	}
	/**
	 * 双方向のフィルターが相手を許可しているか調べる。
	 */
	bool Matches(FCollisionFilter A, FCollisionFilter B) const noexcept
	{
		return (A.Mask & B.Category) && (B.Mask & A.Category);
	}
};
FCollisionWorld::FCollisionWorld() : m_pImpl(MakeUnique<FImpl>())
{
	static FAtomicCounter Next(1);
	m_pImpl->Domain = Next.FetchAdd(1);
	if (!m_pImpl->Domain)
	{
		throw FException("Collision world IDs exhausted");
	}
}
FCollisionWorld::~FCollisionWorld() = default;
FColliderId FCollisionWorld::Add(FCollisionShape Shape, FCollisionFilter Filter)
{
	/**
	 * 形状またはノードを囲む境界箱。
	 */
	const FAABB Box = Bounds(Shape);
	/**
	 * 現在の要素または作成先の番号。
	 */
	size_t Index = 0;
	for (; Index < m_pImpl->Entries.Size(); ++Index)
	{
		if (!m_pImpl->Entries[Index].Shape && m_pImpl->Entries[Index].Generation != TNumericLimits<uint64>::Max())
		{
			break;
		}
	}
	if (Index == m_pImpl->Entries.Size())
	{
		m_pImpl->Entries.EmplaceBack();
	}
	/**
	 * 処理する登録スロット。
	 */
	auto& Entry = m_pImpl->Entries[Index];
	Entry.Shape.Emplace(Move(Shape));
	Entry.Box = Box;
	Entry.Filter = Filter;
	m_pImpl->Dirty = true;
	return m_pImpl->Id(Index);
}
bool FCollisionWorld::Update(FColliderId Id, FCollisionShape Shape, FCollisionFilter Filter)
{
	if (!m_pImpl->Valid(Id))
	{
		return false;
	}
	/**
	 * 形状またはノードを囲む境界箱。
	 */
	const FAABB Box = Bounds(Shape);
	/**
	 * 処理する登録スロット。
	 */
	auto& Entry = m_pImpl->Entries[Id.Index];
	Entry.Shape = Move(Shape);
	Entry.Box = Box;
	Entry.Filter = Filter;
	m_pImpl->Dirty = true;
	return true;
}
bool FCollisionWorld::Remove(FColliderId Id) noexcept
{
	if (!m_pImpl->Valid(Id))
	{
		return false;
	}
	/**
	 * 処理する登録スロット。
	 */
	auto& Entry = m_pImpl->Entries[Id.Index];
	Entry.Shape.Reset();
	++Entry.Generation;
	m_pImpl->Dirty = true;
	return true;
}
TVector<FColliderId> FCollisionWorld::Query(const FCollisionShape& Shape, FCollisionFilter Filter)
{
	/**
	 * 形状またはノードを囲む境界箱。
	 */
	const FAABB Box = m_pImpl->Expanded(Bounds(Shape));
	m_pImpl->Prepare();
	/**
	 * 境界箱の比較を通過した候補。
	 */
	TVector<size_t> Candidates;
	/**
	 * 計算または検索の結果。
	 */
	TVector<FColliderId> Result;
	if (!m_pImpl->Nodes.IsEmpty())
	{
		m_pImpl->Candidates(0, Box, Candidates);
	}
	m_pImpl->Stats.Candidates = Candidates.Size();
	for (size_t Index : Candidates)
	{
		/**
		 * 処理する登録スロット。
		 */
		const auto& Entry = m_pImpl->Entries[Index];
		if (!m_pImpl->Matches(Filter, Entry.Filter))
		{
			continue;
		}
		++m_pImpl->Stats.NarrowTests;
		if (Intersects(Shape, *Entry.Shape))
		{
			Result.PushBack(m_pImpl->Id(Index));
		}
	}
	return Result;
}
TVector<FCollisionPair> FCollisionWorld::FindPairs()
{
	m_pImpl->Prepare();
	/**
	 * 計算または検索の結果。
	 */
	TVector<FCollisionPair> Result;
	/**
	 * 境界箱の比較を通過した候補。
	 */
	TVector<size_t> Candidates;
	for (size_t I = 0; I < m_pImpl->Entries.Size(); ++I)
	{
		/**
		 * 処理する登録スロット。
		 */
		const auto& Entry = m_pImpl->Entries[I];
		if (!Entry.Shape)
		{
			continue;
		}
		Candidates.Clear();
		m_pImpl->Candidates(0, m_pImpl->Expanded(Entry.Box), Candidates);
		for (size_t J : Candidates)
		{
			if (J <= I)
			{
				continue;
			}
			++m_pImpl->Stats.Candidates;
			/**
			 * 比較対象となる値。
			 */
			const auto& Other = m_pImpl->Entries[J];
			if (!m_pImpl->Matches(Entry.Filter, Other.Filter))
			{
				continue;
			}
			++m_pImpl->Stats.NarrowTests;
			if (Intersects(*Entry.Shape, *Other.Shape))
			{
				Result.PushBack({m_pImpl->Id(I), m_pImpl->Id(J)});
			}
		}
	}
	return Result;
}
const FCollisionStats& FCollisionWorld::GetStats() const noexcept
{
	return m_pImpl->Stats;
}
} // namespace Toolbox
