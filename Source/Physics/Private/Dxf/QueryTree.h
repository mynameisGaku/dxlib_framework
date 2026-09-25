// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PRIVATE_PHYSICS_QUERY_TREE_H
#define DXF_PRIVATE_PHYSICS_QUERY_TREE_H
#include "QueryBounds.h"
#include "Toolbox/Vector.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 1回の走査の集計（呼出し側のローカル値）。
 */
struct FQueryVisitCounters
{
	/**
	 * 訪問したノード数。
	 */
	Toolbox::uint64 Nodes = 0;
	/**
	 * 候補として返した葉の数。
	 */
	Toolbox::uint64 Leaves = 0;
};

/**
 * 更新可能な軸平行境界の階層（動的AABB木）。葉は余裕を持たせた境界を持ち、形状がそこから出たときだけ入れ直す。
 * 挿入先は費用の増加が小さい枝を選び、高さの差をAVLと同じ回転で抑える。ノードは予約した領域から取り、
 * 追加・削除・入れ直し・走査では確保しない。走査は親の番号をたどるため、深さに上限のあるスタックを使わない。
 */
template <Toolbox::int32 Dimension> class TQueryTree
{
public:
	/**
	 * 無効なノード番号。
	 */
	static constexpr Toolbox::int32 NullNode = -1;
	/**
	 * 木の一つのノード。
	 */
	struct FNode
	{
		/**
		 * 葉は余裕を持たせた形状の境界、内部ノードは子の境界の和。
		 */
		TQueryBounds<Dimension> Bounds;
		/**
		 * 親（根はNullNode）。空きノードでは次の空きノード。
		 */
		Toolbox::int32 Parent = NullNode;
		/**
		 * 一つ目の子（葉はNullNode）。
		 */
		Toolbox::int32 Child1 = NullNode;
		/**
		 * 二つ目の子（葉はNullNode）。
		 */
		Toolbox::int32 Child2 = NullNode;
		/**
		 * 葉を0とする高さ。空きノードは-1。
		 */
		Toolbox::int32 Height = -1;
		/**
		 * 葉が指す要素（Colliderのスロット番号）。
		 */
		Toolbox::uint32 Item = 0;
	};
	/**
	 * 指定した数の葉を持てるようにノードを予約する。確保に失敗した場合は例外で、既存の木は変わらない。
	 * @param LeafCount 同時に持つ葉の数の上限。
	 */
	void Reserve(Toolbox::size_t LeafCount)
	{
		// 葉n個の木の全ノード数は2n-1。
		const Toolbox::size_t Needed = LeafCount * 2;
		if (Needed <= m_Nodes.Size())
		{
			return;
		}
		Toolbox::size_t Target = m_Nodes.Size() * 2;
		if (Target < Needed)
		{
			Target = Needed;
		}
		if (Target < 16)
		{
			Target = 16;
		}
		if (Target > static_cast<Toolbox::size_t>(Toolbox::TNumericLimits<Toolbox::int32>::Max()))
		{
			throw Toolbox::FException("Query index capacity overflow");
		}
		// 確保だけを先に行う（ここで失敗しても木は変わらない）。
		m_Nodes.Reserve(Target);
		const Toolbox::size_t Old = m_Nodes.Size();
		m_Nodes.Resize(Target);
		// 追加したノードを空きリストの先頭へつなぐ。
		for (Toolbox::size_t Index = Target; Index > Old; --Index)
		{
			FNode& Node = m_Nodes[Index - 1];
			Node.Height = -1;
			Node.Parent = m_FreeList;
			m_FreeList = static_cast<Toolbox::int32>(Index - 1);
		}
	}
	/**
	 * 葉を追加する。事前にReserveで容量を確保しておくこと。
	 * @param Bounds 余裕を持たせた境界。
	 * @param Item 葉が指す要素。
	 */
	Toolbox::int32 Insert(const TQueryBounds<Dimension>& Bounds, Toolbox::uint32 Item) noexcept
	{
		const Toolbox::int32 Leaf = Allocate_Internal();
		FNode& Node = m_Nodes[static_cast<Toolbox::size_t>(Leaf)];
		Node.Bounds = Bounds;
		Node.Item = Item;
		Node.Height = 0;
		Node.Child1 = NullNode;
		Node.Child2 = NullNode;
		InsertLeaf_Internal(Leaf);
		++m_LeafCount;
		return Leaf;
	}
	/**
	 * 葉を削除する。
	 * @param Leaf Insertが返した葉。
	 */
	void Remove(Toolbox::int32 Leaf) noexcept
	{
		RemoveLeaf_Internal(Leaf);
		Free_Internal(Leaf);
		--m_LeafCount;
	}
	/**
	 * 葉の境界を置き換えて入れ直す（葉の番号は変わらない）。
	 * @param Leaf 対象の葉。
	 * @param Bounds 新しい余裕を持たせた境界。
	 */
	void Replace(Toolbox::int32 Leaf, const TQueryBounds<Dimension>& Bounds) noexcept
	{
		RemoveLeaf_Internal(Leaf);
		m_Nodes[static_cast<Toolbox::size_t>(Leaf)].Bounds = Bounds;
		InsertLeaf_Internal(Leaf);
	}
	/**
	 * ノードの境界を返す。
	 * @param Node ノード番号。
	 */
	FORCEINLINE const TQueryBounds<Dimension>& GetBounds(Toolbox::int32 Node) const noexcept
	{
		return m_Nodes[static_cast<Toolbox::size_t>(Node)].Bounds;
	}
	/**
	 * 根の番号を返す（空ならNullNode）。
	 */
	FORCEINLINE Toolbox::int32 GetRoot() const noexcept
	{
		return m_Root;
	}
	/**
	 * 木の高さを返す（空は0）。
	 */
	FORCEINLINE Toolbox::int32 GetHeight() const noexcept
	{
		return m_Root == NullNode ? 0 : m_Nodes[static_cast<Toolbox::size_t>(m_Root)].Height;
	}
	/**
	 * 使用中のノード数を返す。
	 */
	FORCEINLINE Toolbox::size_t GetNodeCount() const noexcept
	{
		return m_LeafCount == 0 ? 0 : m_LeafCount * 2 - 1;
	}
	/**
	 * 葉の数を返す。
	 */
	FORCEINLINE Toolbox::size_t GetLeafCount() const noexcept
	{
		return m_LeafCount;
	}
	/**
	 * 保持している領域のバイト数を返す。
	 */
	FORCEINLINE Toolbox::size_t GetMemoryBytes() const noexcept
	{
		return m_Nodes.Size() * sizeof(FNode);
	}
	/**
	 * 条件に合うノードを親の番号をたどって訪問し、条件に合う葉の要素を渡す。訪問順は木の構造で決まる。
	 * @param Test ノードの境界を受け取り、枝に入るかを返す関数。
	 * @param Visit 葉の要素を受け取る関数。
	 * @param Counters 訪問数を加算する集計。
	 */
	template <typename TTest, typename TVisit>
	void Query(TTest&& Test, TVisit&& Visit, FQueryVisitCounters& Counters) const
	{
		Toolbox::int32 Current = m_Root;
		if (Current == NullNode)
		{
			return;
		}
		while (true)
		{
			const FNode& Node = m_Nodes[static_cast<Toolbox::size_t>(Current)];
			++Counters.Nodes;
			if (Test(Node.Bounds))
			{
				if (Node.Child1 != NullNode)
				{
					Current = Node.Child1;
					continue;
				}
				++Counters.Leaves;
				Visit(Node.Item);
			}
			// 次に訪問する枝: 自分が一つ目の子なら兄弟へ、そうでなければ親へ戻る。
			while (true)
			{
				if (Current == m_Root)
				{
					return;
				}
				const Toolbox::int32 Parent = m_Nodes[static_cast<Toolbox::size_t>(Current)].Parent;
				const FNode& ParentNode = m_Nodes[static_cast<Toolbox::size_t>(Parent)];
				if (ParentNode.Child1 == Current)
				{
					Current = ParentNode.Child2;
					break;
				}
				Current = Parent;
			}
		}
	}
	/**
	 * 構造の整合（親子の対応、境界の包含、高さ、葉の数）を確認する（試験用）。
	 */
	bool Validate() const noexcept
	{
		if (m_Root == NullNode)
		{
			return m_LeafCount == 0;
		}
		if (m_Nodes[static_cast<Toolbox::size_t>(m_Root)].Parent != NullNode)
		{
			return false;
		}
		Toolbox::size_t Leaves = 0;
		for (Toolbox::size_t Index = 0; Index < m_Nodes.Size(); ++Index)
		{
			const FNode& Node = m_Nodes[Index];
			if (Node.Height < 0)
			{
				continue;
			}
			if (static_cast<Toolbox::int32>(Index) != m_Root)
			{
				if (Node.Parent == NullNode)
				{
					return false;
				}
				const FNode& Parent = m_Nodes[static_cast<Toolbox::size_t>(Node.Parent)];
				if (Parent.Child1 != static_cast<Toolbox::int32>(Index) &&
				    Parent.Child2 != static_cast<Toolbox::int32>(Index))
				{
					return false;
				}
				if (!Contains_Internal(Parent.Bounds, Node.Bounds))
				{
					return false;
				}
			}
			if (Node.Child1 == NullNode)
			{
				if (Node.Child2 != NullNode || Node.Height != 0)
				{
					return false;
				}
				++Leaves;
				continue;
			}
			if (Node.Child2 == NullNode)
			{
				return false;
			}
			const FNode& A = m_Nodes[static_cast<Toolbox::size_t>(Node.Child1)];
			const FNode& B = m_Nodes[static_cast<Toolbox::size_t>(Node.Child2)];
			if (Node.Height != 1 + Toolbox::Max(A.Height, B.Height))
			{
				return false;
			}
			if (Toolbox::Abs(A.Height - B.Height) > 1)
			{
				return false;
			}
		}
		return Leaves == m_LeafCount;
	}

private:
	// 空きリストからノードを取り出す。Reserveで確保済みであること。
	Toolbox::int32 Allocate_Internal() noexcept
	{
		const Toolbox::int32 Node = m_FreeList;
		m_FreeList = m_Nodes[static_cast<Toolbox::size_t>(Node)].Parent;
		FNode& Taken = m_Nodes[static_cast<Toolbox::size_t>(Node)];
		Taken.Parent = NullNode;
		Taken.Child1 = NullNode;
		Taken.Child2 = NullNode;
		Taken.Height = 0;
		return Node;
	}
	// ノードを空きリストへ戻す。
	void Free_Internal(Toolbox::int32 Node) noexcept
	{
		FNode& Freed = m_Nodes[static_cast<Toolbox::size_t>(Node)];
		Freed.Height = -1;
		Freed.Child1 = NullNode;
		Freed.Child2 = NullNode;
		Freed.Parent = m_FreeList;
		m_FreeList = Node;
	}
	// ノード参照の略記。
	FORCEINLINE FNode& At_Internal(Toolbox::int32 Node) noexcept
	{
		return m_Nodes[static_cast<Toolbox::size_t>(Node)];
	}
	// 子から境界と高さを求め直す。
	void Refit_Internal(Toolbox::int32 Node) noexcept
	{
		FNode& Current = At_Internal(Node);
		const FNode& A = At_Internal(Current.Child1);
		const FNode& B = At_Internal(Current.Child2);
		Current.Height = 1 + Toolbox::Max(A.Height, B.Height);
		Current.Bounds = Union_Internal(A.Bounds, B.Bounds);
	}
	// 葉を、費用の増加が最も小さい位置の兄弟として挿入し、根まで回転と境界の更新を行う。
	void InsertLeaf_Internal(Toolbox::int32 Leaf) noexcept
	{
		if (m_Root == NullNode)
		{
			m_Root = Leaf;
			At_Internal(Leaf).Parent = NullNode;
			return;
		}
		const TQueryBounds<Dimension> LeafBounds = At_Internal(Leaf).Bounds;
		Toolbox::int32 Index = m_Root;
		while (At_Internal(Index).Child1 != NullNode)
		{
			const FNode& Node = At_Internal(Index);
			const Toolbox::f64 Area = Cost_Internal(Node.Bounds);
			const Toolbox::f64 CombinedArea = Cost_Internal(Union_Internal(Node.Bounds, LeafBounds));
			// ここで新しい親を作る費用と、子へ降りるために祖先が広がる費用。
			const Toolbox::f64 Cost = 2 * CombinedArea;
			const Toolbox::f64 Inheritance = 2 * (CombinedArea - Area);
			const Toolbox::f64 Cost1 = ChildCost_Internal(Node.Child1, LeafBounds) + Inheritance;
			const Toolbox::f64 Cost2 = ChildCost_Internal(Node.Child2, LeafBounds) + Inheritance;
			if (Cost < Cost1 && Cost < Cost2)
			{
				break;
			}
			Index = Cost1 < Cost2 ? Node.Child1 : Node.Child2;
		}
		const Toolbox::int32 Sibling = Index;
		const Toolbox::int32 OldParent = At_Internal(Sibling).Parent;
		const Toolbox::int32 NewParent = Allocate_Internal();
		FNode& Parent = At_Internal(NewParent);
		Parent.Parent = OldParent;
		Parent.Bounds = Union_Internal(LeafBounds, At_Internal(Sibling).Bounds);
		Parent.Height = At_Internal(Sibling).Height + 1;
		Parent.Child1 = Sibling;
		Parent.Child2 = Leaf;
		if (OldParent != NullNode)
		{
			FNode& Old = At_Internal(OldParent);
			if (Old.Child1 == Sibling)
			{
				Old.Child1 = NewParent;
			}
			else
			{
				Old.Child2 = NewParent;
			}
		}
		else
		{
			m_Root = NewParent;
		}
		At_Internal(Sibling).Parent = NewParent;
		At_Internal(Leaf).Parent = NewParent;
		// 根まで回転し、境界と高さを求め直す。
		Toolbox::int32 Walk = At_Internal(Leaf).Parent;
		while (Walk != NullNode)
		{
			Walk = Balance_Internal(Walk);
			Refit_Internal(Walk);
			Walk = At_Internal(Walk).Parent;
		}
	}
	// 子へ降りた場合に増える費用。
	Toolbox::f64 ChildCost_Internal(Toolbox::int32 Child, const TQueryBounds<Dimension>& LeafBounds) noexcept
	{
		const FNode& Node = At_Internal(Child);
		const Toolbox::f64 Combined = Cost_Internal(Union_Internal(LeafBounds, Node.Bounds));
		if (Node.Child1 == NullNode)
		{
			return Combined;
		}
		return Combined - Cost_Internal(Node.Bounds);
	}
	// 葉を木から外し、兄弟を親の位置へ上げる。葉のノード自体は解放しない。
	void RemoveLeaf_Internal(Toolbox::int32 Leaf) noexcept
	{
		if (Leaf == m_Root)
		{
			m_Root = NullNode;
			return;
		}
		const Toolbox::int32 Parent = At_Internal(Leaf).Parent;
		const Toolbox::int32 GrandParent = At_Internal(Parent).Parent;
		const Toolbox::int32 Sibling =
		    At_Internal(Parent).Child1 == Leaf ? At_Internal(Parent).Child2 : At_Internal(Parent).Child1;
		if (GrandParent != NullNode)
		{
			FNode& Grand = At_Internal(GrandParent);
			if (Grand.Child1 == Parent)
			{
				Grand.Child1 = Sibling;
			}
			else
			{
				Grand.Child2 = Sibling;
			}
			At_Internal(Sibling).Parent = GrandParent;
			Free_Internal(Parent);
			Toolbox::int32 Walk = GrandParent;
			while (Walk != NullNode)
			{
				Walk = Balance_Internal(Walk);
				Refit_Internal(Walk);
				Walk = At_Internal(Walk).Parent;
			}
		}
		else
		{
			m_Root = Sibling;
			At_Internal(Sibling).Parent = NullNode;
			Free_Internal(Parent);
		}
		At_Internal(Leaf).Parent = NullNode;
	}
	// 子の高さの差が1を超える場合に回転し、この位置の新しいノードを返す。
	Toolbox::int32 Balance_Internal(Toolbox::int32 IndexA) noexcept
	{
		FNode& A = At_Internal(IndexA);
		if (A.Child1 == NullNode || A.Height < 2)
		{
			return IndexA;
		}
		const Toolbox::int32 IndexB = A.Child1;
		const Toolbox::int32 IndexC = A.Child2;
		FNode& B = At_Internal(IndexB);
		FNode& C = At_Internal(IndexC);
		const Toolbox::int32 Difference = C.Height - B.Height;
		if (Difference > 1)
		{
			// Cを上げる。
			const Toolbox::int32 IndexF = C.Child1;
			const Toolbox::int32 IndexG = C.Child2;
			FNode& F = At_Internal(IndexF);
			FNode& G = At_Internal(IndexG);
			C.Child1 = IndexA;
			C.Parent = A.Parent;
			A.Parent = IndexC;
			ReplaceChild_Internal(C.Parent, IndexA, IndexC);
			if (F.Height > G.Height)
			{
				C.Child2 = IndexF;
				A.Child2 = IndexG;
				G.Parent = IndexA;
				A.Bounds = Union_Internal(B.Bounds, G.Bounds);
				C.Bounds = Union_Internal(A.Bounds, F.Bounds);
				A.Height = 1 + Toolbox::Max(B.Height, G.Height);
				C.Height = 1 + Toolbox::Max(A.Height, F.Height);
			}
			else
			{
				C.Child2 = IndexG;
				A.Child2 = IndexF;
				F.Parent = IndexA;
				A.Bounds = Union_Internal(B.Bounds, F.Bounds);
				C.Bounds = Union_Internal(A.Bounds, G.Bounds);
				A.Height = 1 + Toolbox::Max(B.Height, F.Height);
				C.Height = 1 + Toolbox::Max(A.Height, G.Height);
			}
			return IndexC;
		}
		if (Difference < -1)
		{
			// Bを上げる。
			const Toolbox::int32 IndexD = B.Child1;
			const Toolbox::int32 IndexE = B.Child2;
			FNode& D = At_Internal(IndexD);
			FNode& E = At_Internal(IndexE);
			B.Child1 = IndexA;
			B.Parent = A.Parent;
			A.Parent = IndexB;
			ReplaceChild_Internal(B.Parent, IndexA, IndexB);
			if (D.Height > E.Height)
			{
				B.Child2 = IndexD;
				A.Child1 = IndexE;
				E.Parent = IndexA;
				A.Bounds = Union_Internal(C.Bounds, E.Bounds);
				B.Bounds = Union_Internal(A.Bounds, D.Bounds);
				A.Height = 1 + Toolbox::Max(C.Height, E.Height);
				B.Height = 1 + Toolbox::Max(A.Height, D.Height);
			}
			else
			{
				B.Child2 = IndexE;
				A.Child1 = IndexD;
				D.Parent = IndexA;
				A.Bounds = Union_Internal(C.Bounds, D.Bounds);
				B.Bounds = Union_Internal(A.Bounds, E.Bounds);
				A.Height = 1 + Toolbox::Max(C.Height, D.Height);
				B.Height = 1 + Toolbox::Max(A.Height, E.Height);
			}
			return IndexB;
		}
		return IndexA;
	}
	// 親の子の参照を差し替える（親がなければ根を差し替える）。
	void ReplaceChild_Internal(Toolbox::int32 Parent, Toolbox::int32 Old, Toolbox::int32 New) noexcept
	{
		if (Parent == NullNode)
		{
			m_Root = New;
			return;
		}
		FNode& Node = At_Internal(Parent);
		if (Node.Child1 == Old)
		{
			Node.Child1 = New;
		}
		else
		{
			Node.Child2 = New;
		}
	}
	// ノードの領域（使用中と空き）。
	Toolbox::TVector<FNode> m_Nodes;
	// 根のノード。
	Toolbox::int32 m_Root = NullNode;
	// 空きリストの先頭。
	Toolbox::int32 m_FreeList = NullNode;
	// 葉の数。
	Toolbox::size_t m_LeafCount = 0;
};
} // namespace Dxf::PhysicsPrivate
#endif
