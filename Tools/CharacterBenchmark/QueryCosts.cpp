// SPDX-License-Identifier: NOASSERTION
#include "QueryCosts.h"
namespace Dxf::Benchmark
{
namespace
{
// 2Dの型と、問い合わせの形の作り方。
struct FTraits2D
{
	using FVector = Toolbox::FVector2;
	using FBody = FBodyId2D;
	using FShape = Toolbox::FCircle2D;
	static FVector Offset(FVector Center, Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {Center.X + X, Center.Y + Y};
	}
	static FShape Ball(FVector Center, Toolbox::f32 Radius)
	{
		return {Center, Radius};
	}
};
// 3Dの型と、問い合わせの形の作り方。
struct FTraits3D
{
	using FVector = Toolbox::FVector3;
	using FBody = FBodyId3D;
	using FShape = Toolbox::FSphere;
	static FVector Offset(FVector Center, Toolbox::f32 X, Toolbox::f32 Y)
	{
		return {Center.X + X, Center.Y + Y, Center.Z};
	}
	static FShape Ball(FVector Center, Toolbox::f32 Radius)
	{
		return {Center, Radius};
	}
};
// 任意の法線の一致。
template <typename TVector>
bool SameNormal_Internal(const Toolbox::TOptional<TVector>& A, const Toolbox::TOptional<TVector>& B)
{
	if (A.HasValue() != B.HasValue())
	{
		return false;
	}
	return !A.HasValue() || *A == *B;
}
// 移動キャラクターと同じ大きさ（半径0.5、接触余裕0.02）の問い合わせを1回行い、結果の要約を返す。
// bCompareがtrueなら、同じ入力の二つの経路の結果を比べて一致をOutSameへ書く。
template <typename T, typename TWorld>
Toolbox::uint64 RunOnce_Internal(TWorld& World, Toolbox::int32 Kind, typename T::FVector Center, typename T::FBody Body,
                                 bool bCompare, bool& OutSame)
{
	const Toolbox::TOptional<typename T::FBody> Self = Body;
	switch (Kind)
	{
	case 0:
	{
		const auto Shape = T::Ball(Center, 0.5f);
		const auto Result = World.QueryContacts(Shape, 0.04, Self);
		if (bCompare)
		{
			SetReferencePath(World, true);
			const auto Other = World.QueryContacts(Shape, 0.04, Self);
			SetReferencePath(World, false);
			bool bSame = Result.Count == Other.Count && Result.TotalFound == Other.TotalFound;
			for (Toolbox::uint32 Index = 0; bSame && Index < Result.Count; ++Index)
			{
				bSame = Result.Items[Index].Collider == Other.Items[Index].Collider &&
				        Result.Items[Index].Separation == Other.Items[Index].Separation &&
				        SameNormal_Internal(Result.Items[Index].Normal, Other.Items[Index].Normal);
			}
			OutSame = OutSame && bSame;
		}
		return Result.TotalFound;
	}
	case 1:
	case 2:
	{
		const auto Shape = T::Ball(Center, 0.52f);
		const auto End = T::Offset(Center, 0.1f, -0.02f);
		const auto Result = Kind == 1 ? World.SweepClosest(Shape, End, Self)
		                              : World.SweepClosestIgnoringInitialContacts(Shape, End, Self);
		if (bCompare)
		{
			SetReferencePath(World, true);
			const auto Other = Kind == 1 ? World.SweepClosest(Shape, End, Self)
			                             : World.SweepClosestIgnoringInitialContacts(Shape, End, Self);
			SetReferencePath(World, false);
			bool bSame = Result.HasValue() == Other.HasValue();
			if (bSame && Result.HasValue())
			{
				bSame = Result->Collider == Other->Collider && Result->Fraction == Other->Fraction &&
				        Result->CenterAtHit == Other->CenterAtHit &&
				        Result->bInitialContact == Other->bInitialContact &&
				        SameNormal_Internal(Result->Normal, Other->Normal);
			}
			OutSame = OutSame && bSame;
		}
		return Result.HasValue() ? 1 : 0;
	}
	case 3:
	{
		const auto End = T::Offset(Center, 0.3f, -2.0f);
		const auto Result = World.RaycastClosest(Center, End, Self);
		if (bCompare)
		{
			SetReferencePath(World, true);
			const auto Other = World.RaycastClosest(Center, End, Self);
			SetReferencePath(World, false);
			bool bSame = Result.HasValue() == Other.HasValue();
			if (bSame && Result.HasValue())
			{
				bSame = Result->Collider == Other->Collider && Result->Fraction == Other->Fraction &&
				        Result->Position == Other->Position;
			}
			OutSame = OutSame && bSame;
		}
		return Result.HasValue() ? 1 : 0;
	}
	default:
	{
		const auto Shape = T::Ball(Center, 1.0f);
		const auto Result = World.OverlapAll(Shape, Self);
		if (bCompare)
		{
			SetReferencePath(World, true);
			const auto Other = World.OverlapAll(Shape, Self);
			SetReferencePath(World, false);
			bool bSame = Result.Size() == Other.Size();
			for (Toolbox::size_t Index = 0; bSame && Index < Result.Size(); ++Index)
			{
				bSame = Result[Index] == Other[Index];
			}
			OutSame = OutSame && bSame;
		}
		return Result.Size();
	}
	}
}
// 一つの経路で、種別ごとに5回計って中央値の1回あたりの時間を返す。
template <typename T, typename TWorld>
Toolbox::f64 Time_Internal(TWorld& World, Toolbox::int32 Kind, const typename T::FVector* Centers,
                           const typename T::FBody* Bodies, Toolbox::int32 Count, Toolbox::int32 Calls,
                           Toolbox::uint64& Sink)
{
	Toolbox::f64 Samples[Repetitions];
	bool bUnused = true;
	for (Toolbox::int32 Repetition = 0; Repetition < Repetitions; ++Repetition)
	{
		const Toolbox::uint64 Begin = NowNanoseconds();
		for (Toolbox::int32 Call = 0; Call < Calls; ++Call)
		{
			const Toolbox::int32 Index = Call % Count;
			Sink += RunOnce_Internal<T>(World, Kind, Centers[Index], Bodies[Index], false, bUnused);
		}
		Samples[Repetition] = static_cast<Toolbox::f64>(NowNanoseconds() - Begin) / Calls;
	}
	return Summarize(Samples).Median;
}
// 詳細判定の数（1回あたり）。集計を有効にした別の呼出しで数える。
template <typename T, typename TWorld>
Toolbox::f64 Narrow_Internal(TWorld& World, Toolbox::int32 Kind, const typename T::FVector* Centers,
                             const typename T::FBody* Bodies, Toolbox::int32 Count)
{
	if (!HasQueryIndex())
	{
		return 0;
	}
	BeginQueryCounts(World, true);
	bool bUnused = true;
	for (Toolbox::int32 Index = 0; Index < Count; ++Index)
	{
		(void)RunOnce_Internal<T>(World, Kind, Centers[Index], Bodies[Index], false, bUnused);
	}
	const FQueryCounts Counts = ReadQueryCounts(World);
	BeginQueryCounts(World, false);
	// 種別の番号から診断の区分へ（接触・スイープ2種・線分・重なり）。
	const Toolbox::int32 Class = Kind == 0 ? 2 : (Kind == 3 ? 0 : (Kind == 4 ? 3 : 1));
	return static_cast<Toolbox::f64>(Counts.NarrowTests[Class]) / Count;
}
// 種別ごとの費用の表を作る。
template <typename T, typename TWorld>
FQueryCostTable Measure_Internal(TWorld& World, const typename T::FVector* Centers, const typename T::FBody* Bodies,
                                 Toolbox::int32 Count, Toolbox::int32 Calls)
{
	FQueryCostTable Table;
	Table.bMeasured = true;
	Toolbox::uint64 Sink = 0;
	for (Toolbox::int32 Kind = 0; Kind < QueryKinds; ++Kind)
	{
		SetReferencePath(World, true);
		Table.ReferenceNs[Kind] = Time_Internal<T>(World, Kind, Centers, Bodies, Count, Calls, Sink);
		Table.ReferenceNarrow[Kind] = Narrow_Internal<T>(World, Kind, Centers, Bodies, Count);
		SetReferencePath(World, false);
		Table.IndexedNs[Kind] = Time_Internal<T>(World, Kind, Centers, Bodies, Count, Calls, Sink);
		Table.IndexedNarrow[Kind] = Narrow_Internal<T>(World, Kind, Centers, Bodies, Count);
		// 全中心で二つの経路の結果を比べる。
		bool bSame = true;
		for (Toolbox::int32 Index = 0; Index < Count; ++Index)
		{
			(void)RunOnce_Internal<T>(World, Kind, Centers[Index], Bodies[Index], HasQueryIndex(), bSame);
		}
		Table.bSameResults[Kind] = bSame;
	}
	// 最適化で呼出しを消されないよう、結果の要約を使う。
	if (Sink == 0xffffffffffffffffULL)
	{
		Table.bMeasured = false;
	}
	return Table;
}
} // namespace

const char* QueryKindName(Toolbox::int32 Kind) noexcept
{
	switch (Kind)
	{
	case 0:
		return "QueryContacts";
	case 1:
		return "SweepClosest";
	case 2:
		return "SweepIgnoringInitial";
	case 3:
		return "RaycastClosest";
	default:
		return "OverlapAll";
	}
}
FQueryCostTable MeasureQueryCosts(FPhysicsWorld2D& World, const Toolbox::FVector2* Centers, const FBodyId2D* Bodies,
                                  Toolbox::int32 Count, Toolbox::int32 Calls)
{
	return Measure_Internal<FTraits2D>(World, Centers, Bodies, Count, Calls);
}
FQueryCostTable MeasureQueryCosts(FPhysicsWorld3D& World, const Toolbox::FVector3* Centers, const FBodyId3D* Bodies,
                                  Toolbox::int32 Count, Toolbox::int32 Calls)
{
	return Measure_Internal<FTraits3D>(World, Centers, Bodies, Count, Calls);
}
} // namespace Dxf::Benchmark
