// SPDX-License-Identifier: NOASSERTION
#include "ScenarioRunner.h"
namespace Dxf::Benchmark
{
namespace
{
// 累計へ1回の繰り返しの値を加える。
void Accumulate_Internal(FStepTotals& Totals, const FStepTotals& Add)
{
	Totals.CharacterNs += Add.CharacterNs;
	Totals.ReflectNs += Add.ReflectNs;
	Totals.MoveNs += Add.MoveNs;
	Totals.OtherNs += Add.OtherNs;
	Totals.WorldNs += Add.WorldNs;
	Totals.FixedNs += Add.FixedNs;
	Totals.CharacterAllocations += Add.CharacterAllocations;
	Totals.ReflectAllocations += Add.ReflectAllocations;
	Totals.OtherAllocations += Add.OtherAllocations;
	Totals.WorldAllocations += Add.WorldAllocations;
	Totals.CharacterSteps += Add.CharacterSteps;
	Totals.FixedSteps += Add.FixedSteps;
	Totals.Queries += Add.Queries;
	Totals.MaxQueries = Toolbox::Max(Totals.MaxQueries, Add.MaxQueries);
	Totals.Iterations += Add.Iterations;
	Totals.MaxIterations = Toolbox::Max(Totals.MaxIterations, Add.MaxIterations);
	Totals.Contacts += Add.Contacts;
	Totals.QueryLimitStops += Add.QueryLimitStops;
	Totals.StepUps += Add.StepUps;
	Totals.Travel += Add.Travel;
}
} // namespace

FRunResult RunScenario(FScenarioFactory Factory, Toolbox::int32 Obstacles, Toolbox::int32 Characters,
                       Toolbox::int32 Warmup, Toolbox::int32 Measured, bool bReference)
{
	FRunResult Result;
	// 構築（登録と索引の作成）の時間と確保。
	const Toolbox::uint64 BuildAllocations = TotalAllocations();
	const Toolbox::uint64 BuildBegin = NowNanoseconds();
	Toolbox::TUniquePtr<IScenario> Scenario = Factory(Obstacles, Characters);
	Result.BuildMicroseconds = static_cast<Toolbox::f64>(NowNanoseconds() - BuildBegin) / 1000.0;
	Result.BuildAllocations = TotalAllocations() - BuildAllocations;
	Scenario->SetReference(bReference);
	const Toolbox::f64 CharacterSteps = static_cast<Toolbox::f64>(Measured) * Scenario->GetCharacterCount();
	const Toolbox::f64 FixedSteps = static_cast<Toolbox::f64>(Measured);
	Toolbox::int64 StepIndex = 0;
	FStepTotals Warm;
	for (Toolbox::int32 Index = 0; Index < Warmup; ++Index)
	{
		Scenario->Step(StepIndex++, Warm);
	}
	Toolbox::f64 Move[Repetitions];
	Toolbox::f64 Character[Repetitions];
	Toolbox::f64 Reflect[Repetitions];
	Toolbox::f64 MoveTotal[Repetitions];
	Toolbox::f64 Other[Repetitions];
	Toolbox::f64 World[Repetitions];
	Toolbox::f64 Fixed[Repetitions];
	for (Toolbox::int32 Repetition = 0; Repetition < Repetitions; ++Repetition)
	{
		FStepTotals Totals;
		for (Toolbox::int32 Index = 0; Index < Measured; ++Index)
		{
			const Toolbox::uint64 Begin = NowNanoseconds();
			Scenario->Step(StepIndex++, Totals);
			Totals.FixedNs += NowNanoseconds() - Begin;
			++Totals.FixedSteps;
		}
		Move[Repetition] = static_cast<Toolbox::f64>(Totals.MoveNs) / 1000.0 / CharacterSteps;
		Character[Repetition] = static_cast<Toolbox::f64>(Totals.CharacterNs) / 1000.0 / CharacterSteps;
		Reflect[Repetition] = static_cast<Toolbox::f64>(Totals.ReflectNs) / 1000.0 / CharacterSteps;
		MoveTotal[Repetition] = static_cast<Toolbox::f64>(Totals.MoveNs) / 1000.0 / FixedSteps;
		Other[Repetition] = static_cast<Toolbox::f64>(Totals.OtherNs) / 1000.0 / FixedSteps;
		World[Repetition] = static_cast<Toolbox::f64>(Totals.WorldNs) / 1000.0 / FixedSteps;
		Fixed[Repetition] = static_cast<Toolbox::f64>(Totals.FixedNs) / 1000.0 / FixedSteps;
		Accumulate_Internal(Result.Totals, Totals);
	}
	Result.Move = Summarize(Move);
	Result.Character = Summarize(Character);
	Result.Reflect = Summarize(Reflect);
	Result.MoveTotal = Summarize(MoveTotal);
	Result.Other = Summarize(Other);
	Result.World = Summarize(World);
	Result.Fixed = Summarize(Fixed);
	// 計測と別に、同じ固定更新数で問い合わせを集計する（集計の加算が時間に入らないようにする）。
	Scenario->BeginCounts(true);
	FStepTotals Counted;
	for (Toolbox::int32 Index = 0; Index < Measured; ++Index)
	{
		Scenario->Step(StepIndex++, Counted);
	}
	Result.Counts = Scenario->ReadCounts();
	Scenario->BeginCounts(false);
	return Result;
}
FQueryCostTable MeasureScenarioQueryCosts(FScenarioFactory Factory, Toolbox::int32 Obstacles, Toolbox::int32 Characters,
                                          Toolbox::int32 Warmup, Toolbox::int32 Calls)
{
	Toolbox::TUniquePtr<IScenario> Scenario = Factory(Obstacles, Characters);
	FStepTotals Warm;
	for (Toolbox::int32 Index = 0; Index < Warmup; ++Index)
	{
		Scenario->Step(Index, Warm);
	}
	return Scenario->MeasureQueryCosts(Calls);
}
} // namespace Dxf::Benchmark
