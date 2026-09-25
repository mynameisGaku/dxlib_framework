// SPDX-License-Identifier: NOASSERTION
#include "BenchmarkSupport.h"
#include "Toolbox/Platform.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
namespace Dxf::Benchmark
{
namespace
{
#if !defined(DXF_BENCHMARK_BASELINE)
// 診断から種別ごとの値を写す。
void Copy_Internal(const FWorldQueryTypeCounters& Source, Toolbox::int32 Kind, FQueryCounts& Counts)
{
	Counts.Queries[Kind] = Source.Queries;
	Counts.NarrowTests[Kind] = Source.NarrowTests;
	Counts.Candidates[Kind] = Source.Candidates;
	Counts.Nodes[Kind] = Source.NodesVisited;
	Counts.Fallbacks += Source.FallbackQueries;
}
// 診断を集計の形へ変える。
FQueryCounts Convert_Internal(const FWorldQueryDiagnostics& Diagnostics)
{
	FQueryCounts Counts;
	Counts.bAvailable = true;
	Copy_Internal(Diagnostics.Raycast, 0, Counts);
	Copy_Internal(Diagnostics.Sweep, 1, Counts);
	Copy_Internal(Diagnostics.Contacts, 2, Counts);
	Copy_Internal(Diagnostics.Overlap, 3, Counts);
	Counts.Refreshes = Diagnostics.IndexRefreshes;
	Counts.Reinserts = Diagnostics.IndexReinserts;
	Counts.MemoryBytes = Diagnostics.IndexMemoryBytes;
	Counts.Height = Diagnostics.IndexHeight;
	Counts.Colliders = Diagnostics.AliveColliders;
	return Counts;
}
#endif
} // namespace

FSpread Summarize(const Toolbox::f64 (&Values)[Repetitions])
{
	Toolbox::f64 Sorted[Repetitions];
	for (Toolbox::int32 Index = 0; Index < Repetitions; ++Index)
	{
		Sorted[Index] = Values[Index];
	}
	// 5要素の挿入ソート。
	for (Toolbox::int32 Outer = 1; Outer < Repetitions; ++Outer)
	{
		for (Toolbox::int32 Inner = Outer; Inner > 0 && Sorted[Inner] < Sorted[Inner - 1]; --Inner)
		{
			const Toolbox::f64 Swapped = Sorted[Inner];
			Sorted[Inner] = Sorted[Inner - 1];
			Sorted[Inner - 1] = Swapped;
		}
	}
	return {Sorted[Repetitions / 2], Sorted[0], Sorted[Repetitions - 1]};
}
Toolbox::f64 Distance(Toolbox::FVector2 A, Toolbox::FVector2 B)
{
	const Toolbox::f64 X = Toolbox::f64(A.X) - B.X;
	const Toolbox::f64 Y = Toolbox::f64(A.Y) - B.Y;
	return Toolbox::Sqrt(X * X + Y * Y);
}
Toolbox::f64 Distance(Toolbox::FVector3 A, Toolbox::FVector3 B)
{
	const Toolbox::f64 X = Toolbox::f64(A.X) - B.X;
	const Toolbox::f64 Y = Toolbox::f64(A.Y) - B.Y;
	const Toolbox::f64 Z = Toolbox::f64(A.Z) - B.Z;
	return Toolbox::Sqrt(X * X + Y * Y + Z * Z);
}
Toolbox::uint64 NowNanoseconds()
{
	return Toolbox::MonotonicNanoseconds();
}
Toolbox::uint64 TotalAllocations()
{
	return Toolbox::Testing::GetTotalTestAllocations();
}
#if defined(DXF_BENCHMARK_BASELINE)
bool HasQueryIndex() noexcept
{
	return false;
}
void BeginQueryCounts(FPhysicsWorld2D&, bool)
{
}
void BeginQueryCounts(FPhysicsWorld3D&, bool)
{
}
FQueryCounts ReadQueryCounts(const FPhysicsWorld2D&)
{
	return {};
}
FQueryCounts ReadQueryCounts(const FPhysicsWorld3D&)
{
	return {};
}
void SetReferencePath(FPhysicsWorld2D&, bool)
{
}
void SetReferencePath(FPhysicsWorld3D&, bool)
{
}
#else
bool HasQueryIndex() noexcept
{
	return true;
}
void BeginQueryCounts(FPhysicsWorld2D& World, bool bEnabled)
{
	World.ResetQueryDiagnostics();
	World.SetQueryDiagnosticsEnabled(bEnabled);
}
void BeginQueryCounts(FPhysicsWorld3D& World, bool bEnabled)
{
	World.ResetQueryDiagnostics();
	World.SetQueryDiagnosticsEnabled(bEnabled);
}
FQueryCounts ReadQueryCounts(const FPhysicsWorld2D& World)
{
	return Convert_Internal(World.GetQueryDiagnostics());
}
FQueryCounts ReadQueryCounts(const FPhysicsWorld3D& World)
{
	return Convert_Internal(World.GetQueryDiagnostics());
}
void SetReferencePath(FPhysicsWorld2D& World, bool bReference)
{
	World.SetQueryIndexEnabled_Internal(!bReference);
}
void SetReferencePath(FPhysicsWorld3D& World, bool bReference)
{
	World.SetQueryIndexEnabled_Internal(!bReference);
}
#endif
} // namespace Dxf::Benchmark
