// SPDX-License-Identifier: NOASSERTION
#include "Report.h"
#include <stdio.h>
namespace Dxf::Benchmark
{
namespace
{
// 1回あたりの平均（回数0は0）。
Toolbox::f64 Per_Internal(Toolbox::uint64 Value, Toolbox::uint64 Count)
{
	return Count == 0 ? 0.0 : static_cast<Toolbox::f64>(Value) / static_cast<Toolbox::f64>(Count);
}
// 中央値（最小〜最大）の表記。
void Spread_Internal(const FSpread& Spread)
{
	printf(" %.2f (%.2f-%.2f) |", Spread.Median, Spread.Min, Spread.Max);
}
} // namespace

void PrintConditions(Toolbox::int32 Warmup, Toolbox::int32 Measured)
{
	printf("build=%s, warmup=%d fixed steps, measured=%d fixed steps x %d repetitions, dt=1/60; "
	       "times are median (min-max) of the repetitions in microseconds\n",
	       HasQueryIndex() ? "query-index" : "baseline", Warmup, Measured, Repetitions);
	fflush(stdout);
}
void PrintRunHeader(const char* Title)
{
	printf("\n### %s\n\n", Title);
	printf("| case | var | chars | path | move+reflect per char-step (previous scope) | StepCharacter per char-step | "
	       "reflect per char-step | all characters per fixed step | other bodies per fixed step | World.Step per "
	       "fixed step | fixed step total | queries avg/max | iterations avg/max | narrow tests per char-step | "
	       "fallback queries | index refresh/reinsert per fixed step | build us | index KB | travel per char-step | "
	       "step-ups | query-limit stops | allocations char/reflect/other/world per fixed step |\n");
	printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
	fflush(stdout);
}
void PrintRunRow(const char* Label, Toolbox::int32 Obstacles, Toolbox::int32 Characters, bool bReference,
                 const FRunResult& Result)
{
	const FStepTotals& Totals = Result.Totals;
	const FQueryCounts& Counts = Result.Counts;
	printf("| %s | %d | %d | %s |", Label, Obstacles, Characters,
	       HasQueryIndex() ? (bReference ? "reference" : "index") : "baseline");
	Spread_Internal(Result.Move);
	Spread_Internal(Result.Character);
	Spread_Internal(Result.Reflect);
	Spread_Internal(Result.MoveTotal);
	Spread_Internal(Result.Other);
	Spread_Internal(Result.World);
	Spread_Internal(Result.Fixed);
	printf(" %.2f / %d | %.2f / %d |", Per_Internal(Totals.Queries, Totals.CharacterSteps), Totals.MaxQueries,
	       Per_Internal(Totals.Iterations, Totals.CharacterSteps), Totals.MaxIterations);
	if (Counts.bAvailable)
	{
		Toolbox::uint64 Narrow = 0;
		for (Toolbox::int32 Kind = 0; Kind < 4; ++Kind)
		{
			Narrow += Counts.NarrowTests[Kind];
		}
		// 集計の1回分（Measured回の固定更新）のキャラクター数で割る。
		const Toolbox::uint64 CharacterSteps = Totals.CharacterSteps / Repetitions;
		const Toolbox::uint64 FixedSteps = Totals.FixedSteps / Repetitions;
		printf(" %.1f | %llu | %.2f / %.2f | %.0f | %.1f |", Per_Internal(Narrow, CharacterSteps),
		       static_cast<unsigned long long>(Counts.Fallbacks), Per_Internal(Counts.Refreshes, FixedSteps),
		       Per_Internal(Counts.Reinserts, FixedSteps), Result.BuildMicroseconds,
		       static_cast<Toolbox::f64>(Counts.MemoryBytes) / 1024.0);
	}
	else
	{
		printf(" n/a | n/a | n/a | %.0f | n/a |", Result.BuildMicroseconds);
	}
	printf(" %.4f | %llu | %llu | %.2f/%.2f/%.2f/%.2f |\n",
	       Totals.Travel / static_cast<Toolbox::f64>(Totals.CharacterSteps),
	       static_cast<unsigned long long>(Totals.StepUps), static_cast<unsigned long long>(Totals.QueryLimitStops),
	       Per_Internal(Totals.CharacterAllocations, Totals.FixedSteps),
	       Per_Internal(Totals.ReflectAllocations, Totals.FixedSteps),
	       Per_Internal(Totals.OtherAllocations, Totals.FixedSteps),
	       Per_Internal(Totals.WorldAllocations, Totals.FixedSteps));
	fflush(stdout);
}
void PrintCostHeader()
{
	printf("\n### Query cost by kind (at the character positions after warmup)\n\n");
	printf("| dim | obstacles | chars | query | index ns/call | reference ns/call | speedup | index narrow/call | "
	       "reference narrow/call | same results |\n");
	printf("|---|---|---|---|---|---|---|---|---|---|\n");
	fflush(stdout);
}
void PrintCostRows(const char* Label, Toolbox::int32 Obstacles, Toolbox::int32 Characters, const FQueryCostTable& Table)
{
	for (Toolbox::int32 Kind = 0; Kind < QueryKinds; ++Kind)
	{
		if (!HasQueryIndex())
		{
			printf("| %s | %d | %d | %s | n/a | %.0f | n/a | n/a | n/a | n/a |\n", Label, Obstacles, Characters,
			       QueryKindName(Kind), Table.ReferenceNs[Kind]);
			continue;
		}
		printf("| %s | %d | %d | %s | %.0f | %.0f | %.1fx | %.1f | %.1f | %s |\n", Label, Obstacles, Characters,
		       QueryKindName(Kind), Table.IndexedNs[Kind], Table.ReferenceNs[Kind],
		       Table.IndexedNs[Kind] > 0 ? Table.ReferenceNs[Kind] / Table.IndexedNs[Kind] : 0.0,
		       Table.IndexedNarrow[Kind], Table.ReferenceNarrow[Kind], Table.bSameResults[Kind] ? "yes" : "NO");
	}
	fflush(stdout);
}
} // namespace Dxf::Benchmark
