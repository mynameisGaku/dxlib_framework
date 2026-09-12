// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/CollisionShapes.h"
namespace Toolbox
{
/**
 * 三角形と、構築後は変更しない空間分割。
 */
struct FMesh::FData
{
	/**
	 * 子領域をまたぐ三角形を保持する分割ノード。
	 */
	struct FNode
	{
		/**
		 * 形状またはノードを囲む境界箱。
		 */
		FAABB Box;
		/**
		 * 三角形の番号一覧。
		 */
		TVector<size_t> Triangles;
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
	 * メッシュが所有する頂点座標。
	 */
	TVector<FVector3> Vertices;
	/**
	 * 三角形を構成する頂点番号。
	 */
	TVector<uint32> Indices;
	/**
	 * 各三角形の境界箱キャッシュ。
	 */
	TVector<FAABB> TriangleBounds;
	/**
	 * 再帰分割で構築したノード配列。
	 */
	TVector<FNode> Nodes;
	/**
	 * XY平面での四分割を選ぶか。
	 */
	bool Planar = false;
	/**
	 * 完全に収まる子領域にだけ三角形を配置する。
	 */
	size_t Build(FAABB Box, const TVector<size_t>& Triangles, int32 Depth)
	{
		/**
		 * 現在の要素または作成先の番号。
		 */
		const size_t Index = Nodes.Size();
		Nodes.EmplaceBack();
		Nodes[Index].Box = Box;
		if (Triangles.Size() <= 8 || Depth >= 10)
		{
			Nodes[Index].Triangles = Triangles;
			return Index;
		}
		/**
		 * 処理対象の要素数。
		 */
		const size_t Count = Planar ? 4 : 8;
		/**
		 * 分割基準となる領域の中心。
		 */
		const FVector3 Center = Box.Center();
		TArray<TVector<size_t>, 8> Groups;
		/**
		 * 分割後の各子領域の境界箱。
		 */
		TArray<FAABB, 8> Boxes;
		for (size_t Child = 0; Child < Count; ++Child)
		{
			Boxes[Child] = {{Child & 1 ? Center.X : Box.Min.X, Child & 2 ? Center.Y : Box.Min.Y,
			                 !Planar && (Child & 4) ? Center.Z : Box.Min.Z},
			                {Child & 1 ? Box.Max.X : Center.X, Child & 2 ? Box.Max.Y : Center.Y,
			                 Planar || (Child & 4) ? Box.Max.Z : Center.Z}};
		}
		for (size_t Triangle : Triangles)
		{
			/**
			 * 一つの子領域に完全に収まったか。
			 */
			bool Assigned = false;
			for (size_t Child = 0; Child < Count; ++Child)
			{
				if (Boxes[Child].Contains(TriangleBounds[Triangle]))
				{
					Groups[Child].PushBack(Triangle);
					Assigned = true;
					break;
				}
			}
			if (!Assigned)
			{
				Nodes[Index].Triangles.PushBack(Triangle);
			}
		}
		for (size_t Child = 0; Child < Count; ++Child)
		{
			if (!Groups[Child].IsEmpty())
			{
				/**
				 * 次に追加する支持点または子ノード番号。
				 */
				const size_t Next = Build(Boxes[Child], Groups[Child], Depth + 1);
				Nodes[Index].Children[Child] = Next;
			}
		}
		return Index;
	}
	/**
	 * 検索箱に重なる三角形番号を列挙する。
	 */
	void Query(size_t Index, const FAABB& Box, TVector<size_t>& Result) const
	{
		/**
		 * 現在探索する分割ノード。
		 */
		const auto& Node = Nodes[Index];
		if (!Node.Box.Intersects(Box))
		{
			return;
		}
		for (size_t Triangle : Node.Triangles)
		{
			if (TriangleBounds[Triangle].Intersects(Box))
			{
				Result.PushBack(Triangle);
			}
		}
		for (size_t Child : Node.Children)
		{
			if (Child != TNumericLimits<size_t>::Max())
			{
				Query(Child, Box, Result);
			}
		}
	}
};
FMesh::FMesh(TVector<FVector3> Vertices, TVector<uint32> Indices)
{
	if (Vertices.IsEmpty() || Indices.IsEmpty() || Indices.Size() % 3 != 0)
	{
		throw FException("Mesh needs vertices and complete triangles");
	}
	for (FVector3 Vertex : Vertices)
	{
		if (!Vertex.IsValid())
		{
			throw FException("Nonfinite mesh vertex");
		}
	}
	for (uint32 Index : Indices)
	{
		if (Index >= Vertices.Size())
		{
			throw FException("Mesh index out of range");
		}
	}
	/**
	 * 構築中の共有メッシュデータ。
	 */
	auto Data = MakeShared<FData>();
	Data->Vertices = Move(Vertices);
	Data->Indices = Move(Indices);
	/**
	 * 三角形の番号一覧。
	 */
	TVector<size_t> Triangles;
	/**
	 * 形状またはノードを囲む境界箱。
	 */
	FAABB Box{};
	for (size_t I = 0; I < Data->Indices.Size(); I += 3)
	{
		/**
		 * 三角形の第一頂点。
		 */
		const FVector3 A = Data->Vertices[Data->Indices[I]];
		/**
		 * 三角形の第二頂点。
		 */
		const FVector3 B = Data->Vertices[Data->Indices[I + 1]];
		/**
		 * 三角形の第三頂点。
		 */
		const FVector3 C = Data->Vertices[Data->Indices[I + 2]];
		/**
		 * 現在の三角形を表す判定データ。
		 */
		const FAABB Triangle{Min(A, Min(B, C)), Max(A, Max(B, C))};
		Box = I == 0 ? Triangle : Box.Merged(Triangle);
		Data->TriangleBounds.PushBack(Triangle);
		Triangles.PushBack(I / 3);
	}
	/**
	 * 領域の三軸の幅。
	 */
	const FVector3 Extents = Box.Max - Box.Min;
	Data->Planar = Extents.Z <= (Extents.X + Extents.Y) * 0.001f;
	Data->Build(Box, Triangles, 0);
	m_pData = Move(Data);
}
bool FMesh::IsValid() const noexcept
{
	return static_cast<bool>(m_pData);
}
const TVector<FVector3>& FMesh::GetVertices() const
{
	if (!m_pData)
	{
		throw FBadAccess();
	}
	return m_pData->Vertices;
}
const TVector<uint32>& FMesh::GetIndices() const
{
	if (!m_pData)
	{
		throw FBadAccess();
	}
	return m_pData->Indices;
}
FAABB FMesh::GetBounds() const
{
	if (!m_pData)
	{
		throw FBadAccess();
	}
	return m_pData->Nodes[0].Box;
}
TVector<size_t> FMesh::QueryTriangles_Internal(const FAABB& Box) const
{
	if (!m_pData || !Box.IsValid())
	{
		throw FBadAccess();
	}
	/**
	 * 計算または検索の結果。
	 */
	TVector<size_t> Result;
	m_pData->Query(0, Box, Result);
	return Result;
}
} // namespace Toolbox
