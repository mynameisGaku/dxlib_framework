// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_BENCHMARK_SCENARIO_RUNNER_H
#define DXF_CHARACTER_BENCHMARK_SCENARIO_RUNNER_H
#include "BenchmarkSupport.h"
#include "QueryCosts.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#include "Toolbox/UniquePtr.h"
namespace Dxf::Benchmark
{
/**
 * 固定更新の区間ごとの時間・確保・移動の累計。
 */
struct FStepTotals
{
	/**
	 * StepCharacterだけの時間（ナノ秒）。
	 */
	Toolbox::uint64 CharacterNs = 0;
	/**
	 * 自分のBodyへ結果を反映する時間（SetBodyTransform、索引の更新を含む）。
	 */
	Toolbox::uint64 ReflectNs = 0;
	/**
	 * 前回の計測と同じ範囲: 全キャラクターの移動のループ全体（StepCharacter＋SetBodyTransform）。
	 */
	Toolbox::uint64 MoveNs = 0;
	/**
	 * キャラクター以外のBodyを動かす時間（動的更新の系列）。
	 */
	Toolbox::uint64 OtherNs = 0;
	/**
	 * World.Stepの時間（索引の同期を含む）。
	 */
	Toolbox::uint64 WorldNs = 0;
	/**
	 * 固定更新全体の時間。
	 */
	Toolbox::uint64 FixedNs = 0;
	/**
	 * StepCharacterの中の確保件数。
	 */
	Toolbox::uint64 CharacterAllocations = 0;
	/**
	 * 結果の反映（SetBodyTransform）の中の確保件数。
	 */
	Toolbox::uint64 ReflectAllocations = 0;
	/**
	 * キャラクター以外のBodyの移動の中の確保件数。
	 */
	Toolbox::uint64 OtherAllocations = 0;
	/**
	 * World.Stepの中の確保件数。
	 */
	Toolbox::uint64 WorldAllocations = 0;
	/**
	 * StepCharacterの回数。
	 */
	Toolbox::uint64 CharacterSteps = 0;
	/**
	 * 固定更新の回数。
	 */
	Toolbox::uint64 FixedSteps = 0;
	/**
	 * StepCharacterの問い合わせ数の合計と最大。
	 */
	Toolbox::uint64 Queries = 0;
	/**
	 * 1回のStepCharacterの問い合わせ数の最大。
	 */
	Toolbox::int32 MaxQueries = 0;
	/**
	 * 反復（水平と垂直）の合計。
	 */
	Toolbox::uint64 Iterations = 0;
	/**
	 * 1回の反復の最大。
	 */
	Toolbox::int32 MaxIterations = 0;
	/**
	 * 接触（水平と垂直）の合計。
	 */
	Toolbox::uint64 Contacts = 0;
	/**
	 * 問い合わせの上限で止まった回数。
	 */
	Toolbox::uint64 QueryLimitStops = 0;
	/**
	 * 段差を上った回数。
	 */
	Toolbox::uint64 StepUps = 0;
	/**
	 * 中心が移動した距離の合計。
	 */
	Toolbox::f64 Travel = 0;
};

/**
 * 計測する一つの場面（配置と入力列）。
 */
class IScenario
{
public:
	virtual ~IScenario() = default;
	/**
	 * 1回の固定更新を行い、区間ごとの値を加算する。
	 * @param StepIndex 通し番号（入力列を決める）。
	 * @param Totals 加算先。
	 */
	virtual void Step(Toolbox::int64 StepIndex, FStepTotals& Totals) = 0;
	/**
	 * キャラクターの数を返す。
	 */
	virtual Toolbox::int32 GetCharacterCount() const = 0;
	/**
	 * 問い合わせを総当たりの参照経路にするか。
	 * @param bReference 総当たりにするか。
	 */
	virtual void SetReference(bool bReference) = 0;
	/**
	 * 問い合わせの集計を始める（0から）。
	 * @param bEnabled 集計するか。
	 */
	virtual void BeginCounts(bool bEnabled) = 0;
	/**
	 * 問い合わせの集計を読む。
	 */
	virtual FQueryCounts ReadCounts() const = 0;
	/**
	 * 現在のキャラクターの位置で、問い合わせ種別ごとの費用を計る（対応しない場面は未計測を返す）。
	 * @param Calls 1回の計測の呼出し数。
	 */
	virtual FQueryCostTable MeasureQueryCosts(Toolbox::int32 Calls)
	{
		(void)Calls;
		return {};
	}
};

/**
 * 1条件の計測結果。時間はマイクロ秒。
 */
struct FRunResult
{
	/**
	 * 前回と同じ範囲（StepCharacter＋SetBodyTransform）の、キャラクター1体・固定更新1回あたり。
	 */
	FSpread Move;
	/**
	 * StepCharacterだけの、1体・1回あたり。
	 */
	FSpread Character;
	/**
	 * 結果の反映（SetBodyTransform）の、1体・1回あたり。
	 */
	FSpread Reflect;
	/**
	 * 全キャラクター合計の移動＋反映の、固定更新1回あたり。
	 */
	FSpread MoveTotal;
	/**
	 * キャラクター以外のBodyの移動の、固定更新1回あたり。
	 */
	FSpread Other;
	/**
	 * World.Stepの、固定更新1回あたり。
	 */
	FSpread World;
	/**
	 * 固定更新全体の、1回あたり。
	 */
	FSpread Fixed;
	/**
	 * 計測したすべての繰り返しの累計。
	 */
	FStepTotals Totals;
	/**
	 * 計測と別の1回分（計測と同じ固定更新数）の問い合わせの集計。
	 */
	FQueryCounts Counts;
	/**
	 * 場面の構築（Body・Colliderの登録と索引の作成）の時間。
	 */
	Toolbox::f64 BuildMicroseconds = 0;
	/**
	 * 場面の構築の確保件数。
	 */
	Toolbox::uint64 BuildAllocations = 0;
};

/**
 * 場面を作る関数の型。
 */
using FScenarioFactory = Toolbox::TUniquePtr<IScenario> (*)(Toolbox::int32 Obstacles, Toolbox::int32 Characters);

/**
 * 場面を作り、慣らし・繰り返しの計測・集計用の1回を行う。
 * @param Factory 場面を作る関数。
 * @param Obstacles 障害物の数。
 * @param Characters キャラクターの数。
 * @param Warmup 慣らしの固定更新の数。
 * @param Measured 1回の繰り返しの固定更新の数。
 * @param bReference 総当たりの参照経路で計るか。
 */
FRunResult RunScenario(FScenarioFactory Factory, Toolbox::int32 Obstacles, Toolbox::int32 Characters,
                       Toolbox::int32 Warmup, Toolbox::int32 Measured, bool bReference);

/**
 * 場面を作って慣らした後、キャラクターの位置で問い合わせ種別ごとの費用を計る。
 * @param Factory 場面を作る関数。
 * @param Obstacles 障害物の数。
 * @param Characters キャラクターの数。
 * @param Warmup 慣らしの固定更新の数。
 * @param Calls 1回の計測の呼出し数。
 */
FQueryCostTable MeasureScenarioQueryCosts(FScenarioFactory Factory, Toolbox::int32 Obstacles, Toolbox::int32 Characters,
                                          Toolbox::int32 Warmup, Toolbox::int32 Calls);

/**
 * 1体のキャラクターを1回進め、時間・確保・結果の集計を加える。
 * @param World 問い合わせるWorld。
 * @param Settings 設定。
 * @param State 状態（更新する）。
 * @param Input 入力。
 * @param Body 自分のBody。
 * @param Reflect 結果の中心をBodyへ反映する関数。
 * @param Totals 加算先。
 */
template <typename TWorld, typename TSettings, typename TState, typename TInput, typename TBody, typename TReflect>
void StepCharacterTimed(const TWorld& World, const TSettings& Settings, TState& State, const TInput& Input, TBody Body,
                        TReflect&& Reflect, FStepTotals& Totals)
{
	const Toolbox::uint64 Allocations = TotalAllocations();
	const Toolbox::uint64 Begin = NowNanoseconds();
	const auto Result = StepCharacter(World, Settings, State, Input, StepSeconds, Body);
	const Toolbox::uint64 Stepped = NowNanoseconds();
	const Toolbox::uint64 StepAllocations = TotalAllocations();
	Totals.Travel += Distance(State.Center, Result.State.Center);
	State = Result.State;
	Reflect(State.Center);
	const Toolbox::uint64 Reflected = NowNanoseconds();
	Totals.CharacterNs += Stepped - Begin;
	Totals.ReflectNs += Reflected - Stepped;
	Totals.CharacterAllocations += StepAllocations - Allocations;
	Totals.ReflectAllocations += TotalAllocations() - StepAllocations;
	++Totals.CharacterSteps;
	const Toolbox::int32 Iterations = Result.Horizontal.Iterations + Result.Vertical.Iterations;
	Totals.Queries += static_cast<Toolbox::uint64>(Result.Queries);
	Totals.MaxQueries = Toolbox::Max(Totals.MaxQueries, Result.Queries);
	Totals.Iterations += static_cast<Toolbox::uint64>(Iterations);
	Totals.MaxIterations = Toolbox::Max(Totals.MaxIterations, Iterations);
	Totals.Contacts += Result.Horizontal.ContactCount + Result.Vertical.ContactCount;
	Totals.QueryLimitStops += Result.Horizontal.Stop == ECharacterMoveStop::QueryLimit ||
	                                  Result.Vertical.Stop == ECharacterMoveStop::QueryLimit
	                              ? 1
	                              : 0;
	Totals.StepUps += Result.bSteppedUp ? 1 : 0;
}
/**
 * World.Stepを行い、時間と確保を加える。
 * @param World 進めるWorld。
 * @param Totals 加算先。
 */
template <typename TWorld> void StepWorldTimed(TWorld& World, FStepTotals& Totals)
{
	const Toolbox::uint64 Allocations = TotalAllocations();
	const Toolbox::uint64 Begin = NowNanoseconds();
	World.Step(StepSeconds);
	Totals.WorldNs += NowNanoseconds() - Begin;
	Totals.WorldAllocations += TotalAllocations() - Allocations;
}
} // namespace Dxf::Benchmark
#endif
