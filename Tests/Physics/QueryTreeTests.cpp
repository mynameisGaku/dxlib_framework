// SPDX-License-Identifier: NOASSERTION
// 問い合わせ索引の部品（境界の計算、動的AABB木、結果の順序）の単体試験。World形状の境界が形状を取りこぼさないこと、
// 木が追加・削除・入れ直しの後も構造の整合を保ち、走査が境界の重なる葉を過不足なく一度ずつ返すこと、
// 深い木でも走査が止まらないこと、線分の判定が接する・始点が内部・長さ0の場合を落とさないことを確かめる。
#include "QueryIndexTestSupport.h"
#include "Dxf/QueryIndex.h"
#include "Dxf/QueryResultOrder.h"
#include "Dxf/WorldQueryShapes2D.h"
#include "Dxf/WorldQueryShapes3D.h"
using namespace Toolbox;
using namespace Dxf;
using namespace Dxf::PhysicsPrivate;
using namespace PhysicsTest::QueryIndexTest;
namespace
{
// 乱数の境界。
template <int32 D> TQueryBounds<D> RandomBounds_Internal(FRandom& Random, f32 Extent)
{
	TQueryBounds<D> Bounds;
	for (int32 Axis = 0; Axis < D; ++Axis)
	{
		const f64 Center = Random.Range(-Extent, Extent);
		const f64 Half = Random.Chance(10) ? 0.0 : Random.Range(0, 3);
		Bounds.Min[Axis] = Center - Half;
		Bounds.Max[Axis] = Center + Half;
	}
	return Bounds;
}
// 追加・削除・入れ直しの後、構造の整合と、走査が重なる葉を一度ずつ返すことを総当たりの影と比べる。
template <int32 D> void TreeOperations_Internal()
{
	for (uint64 Seed = 1; Seed <= 4; ++Seed)
	{
		FRandom Random(Seed * 104729);
		TQueryTree<D> Tree;
		Tree.Reserve(512);
		// 要素ごとの葉（-1は未登録）と境界の影。
		int32 Leaves[512];
		TQueryBounds<D> Shadow[512];
		for (int32 Index = 0; Index < 512; ++Index)
		{
			Leaves[Index] = -1;
		}
		for (int32 Step = 0; Step < 3000; ++Step)
		{
			const int32 Item = Random.Below(512);
			if (Leaves[Item] < 0)
			{
				Shadow[Item] = RandomBounds_Internal<D>(Random, 50);
				Leaves[Item] = Tree.Insert(Shadow[Item], static_cast<uint32>(Item));
			}
			else if (Random.Chance(50))
			{
				Shadow[Item] = RandomBounds_Internal<D>(Random, 50);
				Tree.Replace(Leaves[Item], Shadow[Item]);
			}
			else
			{
				Tree.Remove(Leaves[Item]);
				Leaves[Item] = -1;
			}
			if (Step % 50 != 0)
			{
				continue;
			}
			PHYSICS_REQUIRE(Tree.Validate());
			const TQueryBounds<D> Area = RandomBounds_Internal<D>(Random, 50);
			int32 Seen[512]{};
			FQueryVisitCounters Counters;
			Tree.Query(
			    [&](const TQueryBounds<D>& Bounds)
			    {
				    return Overlaps_Internal(Bounds, Area);
			    },
			    [&](uint32 Found)
			    {
				    ++Seen[Found];
			    },
			    Counters);
			for (int32 Index = 0; Index < 512; ++Index)
			{
				const bool bExpected = Leaves[Index] >= 0 && Overlaps_Internal(Shadow[Index], Area);
				PHYSICS_REQUIRE(Seen[Index] == (bExpected ? 1 : 0));
			}
		}
	}
}
// 一列に並べた多数の葉でも高さの差を回転で抑え、スタックのない走査がすべての葉を訪れる。
template <int32 D> void DeepTree_Internal()
{
	TQueryTree<D> Tree;
	Tree.Reserve(20000);
	for (int32 Index = 0; Index < 20000; ++Index)
	{
		TQueryBounds<D> Bounds;
		for (int32 Axis = 0; Axis < D; ++Axis)
		{
			Bounds.Min[Axis] = Axis == 0 ? Index : 0;
			Bounds.Max[Axis] = Axis == 0 ? Index + 0.5 : 0.5;
		}
		Tree.Insert(Bounds, static_cast<uint32>(Index));
	}
	PHYSICS_REQUIRE(Tree.Validate());
	// AVLの高さは葉の数nに対し1.44*log2(n)程度。20000では約21。
	PHYSICS_REQUIRE(Tree.GetHeight() <= 30);
	uint64 Visited = 0;
	FQueryVisitCounters Counters;
	Tree.Query(
	    [](const TQueryBounds<D>&)
	    {
		    return true;
	    },
	    [&](uint32)
	    {
		    ++Visited;
	    },
	    Counters);
	PHYSICS_REQUIRE(Visited == 20000 && Counters.Nodes == Tree.GetNodeCount());
}
// 線分の判定: 接する・端点で触れる・始点が内部・長さ0・軸に平行を落とさない。離れたものは偽。
void SegmentTest_Internal()
{
	TQueryBounds<3> Box;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Box.Min[Axis] = -1;
		Box.Max[Axis] = 1;
	}
	auto Make = [](f64 X0, f64 Y0, f64 Z0, f64 X1, f64 Y1, f64 Z1, f64 Radius)
	{
		TQuerySegment<3> Segment;
		const f64 Start[3] = {X0, Y0, Z0};
		const f64 End[3] = {X1, Y1, Z1};
		Segment.Radius = Radius;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Segment.Start[Axis] = Start[Axis];
			Segment.Delta[Axis] = End[Axis] - Start[Axis];
			Segment.Bounds.Min[Axis] = Min(Start[Axis], End[Axis]) - Radius;
			Segment.Bounds.Max[Axis] = Max(Start[Axis], End[Axis]) + Radius;
		}
		return Segment;
	};
	// 面に接して通る（Y=1の面上）。
	PHYSICS_REQUIRE(SegmentOverlaps_Internal(Box, Make(-3, 1, 0, 3, 1, 0, 0)));
	// 終点がちょうど面に触れる。
	PHYSICS_REQUIRE(SegmentOverlaps_Internal(Box, Make(-3, 0, 0, -1, 0, 0, 0)));
	// 始点が内部、長さ0。
	PHYSICS_REQUIRE(SegmentOverlaps_Internal(Box, Make(0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0)));
	// 斜めに頂点へ触れる。
	PHYSICS_REQUIRE(SegmentOverlaps_Internal(Box, Make(-2, -2, -2, -1, -1, -1, 0)));
	// 半径で届く（中心線は外、半径1で面に接する）。
	PHYSICS_REQUIRE(SegmentOverlaps_Internal(Box, Make(-3, 2, 0, 3, 2, 0, 1)));
	// 離れている。
	PHYSICS_REQUIRE(!SegmentOverlaps_Internal(Box, Make(-3, 2.5, 0, 3, 2.5, 0, 1)));
	PHYSICS_REQUIRE(!SegmentOverlaps_Internal(Box, Make(2, 2, 2, 5, 5, 5, 0)));
	// 乱数: 線分上の点が箱に入るなら判定は真（保守的）。
	FRandom Random(99991);
	for (int32 Trial = 0; Trial < 20000; ++Trial)
	{
		const f64 X0 = Random.Range(-4, 4);
		const f64 Y0 = Random.Range(-4, 4);
		const f64 Z0 = Random.Range(-4, 4);
		const f64 X1 = Random.Range(-4, 4);
		const f64 Y1 = Random.Range(-4, 4);
		const f64 Z1 = Random.Range(-4, 4);
		bool bInside = false;
		for (int32 Sample = 0; Sample <= 64 && !bInside; ++Sample)
		{
			const f64 T = Sample / 64.0;
			const f64 P[3] = {X0 + (X1 - X0) * T, Y0 + (Y1 - Y0) * T, Z0 + (Z1 - Z0) * T};
			bInside = Abs(P[0]) <= 1 && Abs(P[1]) <= 1 && Abs(P[2]) <= 1;
		}
		if (bInside)
		{
			PHYSICS_REQUIRE(SegmentOverlaps_Internal(Box, Make(X0, Y0, Z0, X1, Y1, Z1, 0)));
		}
	}
}
// World形状の境界が、形状の頂点・端を取りこぼさない（格納した軸のまま、半幅0の辺・面を含む）。
void ShapeBounds_Internal()
{
	FRandom Random(271828);
	for (int32 Trial = 0; Trial < 5000; ++Trial)
	{
		const auto Description = F3D::RandomShape(Random, 1000);
		if (Description.Shape.Index() != 1)
		{
			continue;
		}
		const FOBB& Box = Description.Shape.Get<1>();
		const auto Bounds = QueryShapeBounds_Internal(Box);
		PHYSICS_REQUIRE(Bounds.bIndexable);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			for (int32 Component = 0; Component < 3; ++Component)
			{
				f64 Value = Box.Center.Component(Component);
				for (int32 Axis = 0; Axis < 3; ++Axis)
				{
					const f64 Sign = (Corner >> Axis) & 1 ? 1.0 : -1.0;
					Value += Sign * Box.HalfExtents.Component(Axis) *
					         Box.Axes[static_cast<size_t>(Axis)].Component(Component);
				}
				PHYSICS_REQUIRE(Value >= Bounds.Tight.Min[Component] && Value <= Bounds.Tight.Max[Component]);
			}
		}
	}
	for (int32 Trial = 0; Trial < 5000; ++Trial)
	{
		const auto Description = F2D::RandomShape(Random, 1000);
		if (Description.Shape.Index() != 1)
		{
			continue;
		}
		const FOrientedBox2D& Box = Description.Shape.Get<1>();
		const auto Bounds = QueryShapeBounds_Internal(Box);
		const f64 C = Cos(f64(Box.Angle));
		const f64 S = Sin(f64(Box.Angle));
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const f64 X = (Corner & 1 ? 1 : -1) * f64(Box.HalfExtents.X);
			const f64 Y = (Corner & 2 ? 1 : -1) * f64(Box.HalfExtents.Y);
			const f64 WX = Box.Center.X + C * X - S * Y;
			const f64 WY = Box.Center.Y + S * X + C * Y;
			PHYSICS_REQUIRE(WX >= Bounds.Tight.Min[0] && WX <= Bounds.Tight.Max[0]);
			PHYSICS_REQUIRE(WY >= Bounds.Tight.Min[1] && WY <= Bounds.Tight.Max[1]);
		}
	}
	// 無効な形状（非有限・直交しない軸）は索引に入れない。
	FOBB Skewed{{0, 0, 0}, {1, 1, 1}};
	Skewed.Axes[0] = {2, 0, 0};
	PHYSICS_REQUIRE(!QueryShapeBounds_Internal(Skewed).bIndexable);
	// 無限大の座標（ビット列から作る）。
	const uint32 InfinityBits = 0x7f800000u;
	f32 Infinity = 0;
	memcpy(&Infinity, &InfinityBits, sizeof(Infinity));
	PHYSICS_REQUIRE(!QueryShapeBounds_Internal(FCircle2D{{Infinity, 0}, 1}).bIndexable);
}
// 結果の順序: 固定容量の集合へ乱数の順で入れても、スロット昇順の先頭32件と全件数になる。ヒープソートは昇順。
void ResultOrder_Internal()
{
	FRandom Random(31337);
	for (int32 Trial = 0; Trial < 200; ++Trial)
	{
		const int32 Count = 1 + Random.Below(80);
		size_t Slots[80];
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Slots[Index] = static_cast<size_t>(Index * 3 + 1);
		}
		// 乱数の順に並べ替える。
		for (int32 Index = Count - 1; Index > 0; --Index)
		{
			const int32 Other = Random.Below(Index + 1);
			const size_t Swapped = Slots[Index];
			Slots[Index] = Slots[Other];
			Slots[Other] = Swapped;
		}
		FWorldContactSet3D Set;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FWorldContact3D Item;
			Item.Collider.Index = Slots[Index];
			InsertContactBySlot_Internal(Set, Item);
		}
		PHYSICS_REQUIRE(Set.TotalFound == static_cast<uint32>(Count));
		PHYSICS_REQUIRE(Set.Count == static_cast<uint32>(Min(Count, 32)));
		for (uint32 Index = 0; Index < Set.Count; ++Index)
		{
			PHYSICS_REQUIRE(Set.Items[Index].Collider.Index == Index * 3 + 1);
		}
		HeapSort_Internal(Slots, static_cast<size_t>(Count),
		                  [](size_t A, size_t B)
		                  {
			                  return A < B;
		                  });
		for (int32 Index = 0; Index < Count; ++Index)
		{
			PHYSICS_REQUIRE(Slots[Index] == static_cast<size_t>(Index * 3 + 1));
		}
	}
	// 同じ割合はスロットの小さい方。
	PHYSICS_REQUIRE(IsCloserHit_Internal(false, 0, 0, 0.5, 9));
	PHYSICS_REQUIRE(IsCloserHit_Internal(true, 0.5, 9, 0.5, 3));
	PHYSICS_REQUIRE(!IsCloserHit_Internal(true, 0.5, 3, 0.5, 9));
	PHYSICS_REQUIRE(!IsCloserHit_Internal(true, 0.4, 9, 0.5, 3));
}
const PhysicsTest::FCase Cases_Internal[] = {
    {"2D query tree keeps structure and returns overlapping leaves once", &TreeOperations_Internal<2>},
    {"3D query tree keeps structure and returns overlapping leaves once", &TreeOperations_Internal<3>},
    {"2D query tree stays balanced and traverses without a stack", &DeepTree_Internal<2>},
    {"3D query tree stays balanced and traverses without a stack", &DeepTree_Internal<3>},
    {"query segment test keeps touching, inside and zero-length segments", &SegmentTest_Internal},
    {"query shape bounds cover the stored shape and reject invalid shapes", &ShapeBounds_Internal},
    {"query result order is independent of the visit order", &ResultOrder_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetQueryTreeCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
