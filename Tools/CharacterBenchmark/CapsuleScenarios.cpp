// SPDX-License-Identifier: NOASSERTION
#include "CapsuleScenarios.h"
#include "BenchmarkSupport.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#include "Toolbox/Platform.h"
#include <stdio.h>
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
#include "../../Source/Physics/Private/Dxf/WorldInteractionProbe.h"
#endif

namespace Dxf::Benchmark
{
namespace
{
// 系列の条件。
enum class ECapsuleCase
{
	// 静止したカプセルの障害物の間を、カプセルのキャラクターが歩く。
	CapsuleStatic,
	// Dynamicのカプセルの山。
	CapsuleDense,
	// 静止したSensorのカプセルの間を、Kinematicのカプセルが往復する（イベントあり）。
	CapsuleTrigger,
	// カプセルのキャラクターが、それぞれ前の箱を押す。
	Push,
	// 疎なDynamicの球（重力なし、接触なし）。索引なし。
	BroadPhaseOff,
	// 同じ配置で索引あり。
	BroadPhaseOn,
	// 密に積んだDynamicの箱。索引なし／あり。
	DenseSolidOff,
	DenseSolidOn,
	// DynamicのSolidと静止したSensorが混ざる（イベントあり）。
	MixedSolidSensor,
	// 動く床に乗ったカプセルのキャラクター。
	CapsuleMovingFloor
};

// CSVの条件名。
const char* CaseName(ECapsuleCase Case)
{
	switch (Case)
	{
	case ECapsuleCase::CapsuleStatic:
		return "capsule-static";
	case ECapsuleCase::CapsuleDense:
		return "capsule-dense";
	case ECapsuleCase::CapsuleTrigger:
		return "capsule-trigger";
	case ECapsuleCase::Push:
		return "push";
	case ECapsuleCase::BroadPhaseOff:
		return "solver-broadphase-off";
	case ECapsuleCase::BroadPhaseOn:
		return "solver-broadphase-on";
	case ECapsuleCase::DenseSolidOff:
		return "solver-dense-solid-off";
	case ECapsuleCase::DenseSolidOn:
		return "solver-dense-solid";
	case ECapsuleCase::MixedSolidSensor:
		return "mixed-solid-sensor";
	case ECapsuleCase::CapsuleMovingFloor:
		return "capsule-moving-floor";
	}
	throw Toolbox::FException("Unknown capsule benchmark case");
}
// 計測条件が壊れていた場合だけ止める。
void Require(bool bCondition, const char* Message)
{
	if (!bCondition)
	{
		throw Toolbox::FException(Message);
	}
}

// 2Dの型と形状。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBody = FBodyId2D;
	using FBodyDescription = FBodyDescription2D;
	using FColliderDescription = FColliderDescription2D;
	using FSettings = FCharacterMoveSettings2D;
	using FState = FCharacterState2D;
	using FInput = FCharacterMoveInput2D;
	using FVector = Toolbox::FVector2;
	static constexpr const char* Name = "2D";
	// 平面の点（3DはZ=Lane）。
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Lane = 0)
	{
		(void)Lane;
		return {X, Y};
	}
	static FColliderDescription Capsule(Toolbox::f32 Half, Toolbox::f32 Radius, bool bStanding)
	{
		FColliderDescription Description;
		Description.Shape = bStanding ? Toolbox::FCapsule2D{{0, -Half}, {0, Half}, Radius}
		                              : Toolbox::FCapsule2D{{-Half, 0}, {Half, 0}, Radius};
		return Description;
	}
	static FColliderDescription Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{{}, {HalfX, HalfY}, 0};
		return Description;
	}
	static FColliderDescription Ball(Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FCircle2D{{}, Radius};
		return Description;
	}
};
// 3Dの型と形状（奥行きのある床・箱）。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBody = FBodyId3D;
	using FBodyDescription = FBodyDescription3D;
	using FColliderDescription = FColliderDescription3D;
	using FSettings = FCharacterMoveSettings3D;
	using FState = FCharacterState3D;
	using FInput = FCharacterMoveInput3D;
	using FVector = Toolbox::FVector3;
	static constexpr const char* Name = "3D";
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Lane = 0)
	{
		return {X, Y, Lane};
	}
	static FColliderDescription Capsule(Toolbox::f32 Half, Toolbox::f32 Radius, bool bStanding)
	{
		FColliderDescription Description;
		Description.Shape = bStanding ? Toolbox::FCapsule{{0, -Half, 0}, {0, Half, 0}, Radius}
		                              : Toolbox::FCapsule{{-Half, 0, 0}, {Half, 0, 0}, Radius};
		return Description;
	}
	static FColliderDescription Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{{}, {HalfX, HalfY, HalfX < 5 ? HalfX : 200}};
		return Description;
	}
	static FColliderDescription Ball(Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FSphere{{}, Radius};
		return Description;
	}
};

// 固定更新1回の区間の合計。
struct FCapsuleTotals
{
	Toolbox::uint64 WorldNs = 0;
	Toolbox::uint64 CharacterNs = 0;
	Toolbox::uint64 WorldAllocations = 0;
	Toolbox::uint64 CharacterAllocations = 0;
	Toolbox::uint64 BroadPhaseNs = 0;
	Toolbox::uint64 BroadPhaseAllocations = 0;
	Toolbox::uint64 Candidates = 0;
	Toolbox::uint64 Manifolds = 0;
	Toolbox::uint64 EventPairs = 0;
	Toolbox::uint64 Overflows = 0;
	Toolbox::uint64 Pushes = 0;
	Toolbox::uint64 Steps = 0;
};

// 1条件の場面。Worldと値のキャラクターを持つ。
template <typename T> class TCapsuleScenario
{
public:
	TCapsuleScenario(ECapsuleCase Case, Toolbox::int32 Colliders, Toolbox::int32 Actors)
	    : m_Case(Case), m_Actors(Actors)
	{
		// 索引なしの条件では、Solverとイベントの組の候補を総当たりの参照経路で作る（問い合わせの索引は切り替えない）。
		m_World.SetSolverBroadPhaseEnabled_Internal(Case != ECapsuleCase::BroadPhaseOff &&
		                                            Case != ECapsuleCase::DenseSolidOff);
		m_bEvents = Case == ECapsuleCase::CapsuleTrigger || Case == ECapsuleCase::MixedSolidSensor;
		if (m_bEvents)
		{
			FWorldEventSettings Events;
			Events.bEnabled = true;
			Events.MaxPairs = 1u << 16;
			m_World.SetEventSettings(Events);
		}
		typename T::FBodyDescription Static;
		Static.Type = EBodyType::Static;
		typename T::FBodyDescription Dynamic;
		typename T::FBodyDescription Kinematic;
		Kinematic.Type = EBodyType::Kinematic;
		const bool bSparse = Case == ECapsuleCase::BroadPhaseOff || Case == ECapsuleCase::BroadPhaseOn;
		if (bSparse)
		{
			m_World.SetGravity(T::At(0, 0));
		}
		else
		{
			// 上面y=0の広い床。
			Static.Position = T::At(0, -0.5f);
			m_World.AttachCollider(m_World.CreateBody(Static), T::Box(400, 0.5f));
		}
		const Toolbox::int32 Columns =
		    static_cast<Toolbox::int32>(Toolbox::Sqrt(static_cast<Toolbox::f64>(Colliders))) + 1;
		for (Toolbox::int32 Index = 0; Index < Colliders; ++Index)
		{
			const Toolbox::f32 Column = static_cast<Toolbox::f32>(Index % Columns);
			const Toolbox::f32 Row = static_cast<Toolbox::f32>(Index / Columns);
			switch (Case)
			{
			case ECapsuleCase::CapsuleStatic:
				// 歩く列（Y＝0付近）の上に、離れて並ぶ静止したカプセル。
				Static.Position = T::At(Column * 4 - 100, 3 + Row * 3, Row);
				m_World.AttachCollider(m_World.CreateBody(Static), T::Capsule(0.5f, 0.3f, false));
				break;
			case ECapsuleCase::CapsuleDense:
				Dynamic.Position = T::At(Column * 1.3f, 0.4f + Row * 0.9f, Column * 0.1f);
				m_World.AttachCollider(m_World.CreateBody(Dynamic), T::Capsule(0.3f, 0.3f, false));
				break;
			case ECapsuleCase::CapsuleTrigger:
			{
				auto Sensor = T::Capsule(0.5f, 0.5f, true);
				Sensor.Response = EColliderResponse::Sensor;
				if (Index % 2 == 0)
				{
					Static.Position = T::At(Column * 3, 1 + Row * 3, Row);
					m_World.AttachCollider(m_World.CreateBody(Static), Sensor);
				}
				else
				{
					// 隣のSensor（x=Column*3−3）の手前1.0から、周期ごとに0〜+1.0往復し、半径の和0.8の内外を出入りする。
					Kinematic.Position = T::At(Column * 3 - 4, 1 + Row * 3, Row);
					const auto Body = m_World.CreateBody(Kinematic);
					m_World.AttachCollider(Body, T::Capsule(0.3f, 0.3f, true));
					m_Moving.PushBack(Body);
				}
				break;
			}
			case ECapsuleCase::BroadPhaseOff:
			case ECapsuleCase::BroadPhaseOn:
				Dynamic.Position = T::At(Column * 3, Row * 3, Row);
				m_World.AttachCollider(m_World.CreateBody(Dynamic), T::Ball(0.5f));
				break;
			case ECapsuleCase::DenseSolidOff:
			case ECapsuleCase::DenseSolidOn:
				Dynamic.Position = T::At(Column * 1.05f, 0.5f + Row * 1.02f);
				m_World.AttachCollider(m_World.CreateBody(Dynamic), T::Box(0.5f, 0.5f));
				break;
			case ECapsuleCase::MixedSolidSensor:
				if (Index % 2 == 0)
				{
					Dynamic.Position = T::At(Column * 1.5f, 1 + Row * 1.2f, Row * 0.1f);
					m_World.AttachCollider(m_World.CreateBody(Dynamic), T::Ball(0.4f));
				}
				else
				{
					auto Sensor = T::Box(0.7f, 0.7f);
					Sensor.Response = EColliderResponse::Sensor;
					Static.Position = T::At(Column * 1.5f, 1 + Row * 1.2f, Row * 0.1f);
					m_World.AttachCollider(m_World.CreateBody(Static), Sensor);
				}
				break;
			case ECapsuleCase::Push:
			case ECapsuleCase::CapsuleMovingFloor:
				break;
			}
		}
		// キャラクター（値だけ）。カプセル、押し合いの条件では押す。
		m_Settings.Shape = ECharacterShape::Capsule;
		m_Settings.HalfHeight = 0.4;
		m_Settings.bPushDynamicBodies = Case == ECapsuleCase::Push;
		if (Case == ECapsuleCase::CapsuleMovingFloor)
		{
			Kinematic.Position = T::At(0, -0.25f);
			m_Floor = m_World.CreateBody(Kinematic);
			m_World.AttachCollider(*m_Floor, T::Box(300, 0.25f));
		}
		for (Toolbox::int32 Index = 0; Index < Actors; ++Index)
		{
			// 2Dは列をXで、3DはZで離す。
			const Toolbox::f32 Lane = static_cast<Toolbox::f32>(Index) * 6;
			typename T::FState State;
			const bool b3D = sizeof(typename T::FVector) == sizeof(Toolbox::FVector3);
			State.Center = b3D ? T::At(0, 0.92f, Lane) : T::At(Lane * 2, 0.92f);
			if (Case == ECapsuleCase::CapsuleMovingFloor)
			{
				State.Center = T::At(b3D ? 0 : Lane * 2, 0.92f, b3D ? Lane : 0);
			}
			m_States.PushBack(State);
			if (Case == ECapsuleCase::Push)
			{
				Dynamic.Position = T::At((b3D ? 0 : Lane * 2) + 1.5f, 0.5f, b3D ? Lane : 0);
				m_World.AttachCollider(m_World.CreateBody(Dynamic), T::Box(0.5f, 0.5f));
			}
		}
	}
	// 固定更新1回を計測する。
	void Step(Toolbox::int64 StepIndex, FCapsuleTotals& Totals)
	{
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
		PhysicsPrivate::FWorldInteractionProbe Probe(NowNanoseconds, TotalAllocations);
#endif
		for (const auto Body : m_Moving)
		{
			m_World.SetVelocity(Body, T::At(static_cast<Toolbox::f32>(1.5 * Toolbox::Sin(0.05 * StepIndex)), 0));
		}
		if (m_Floor)
		{
			m_World.SetVelocity(*m_Floor, T::At(2, 0));
		}
		// 押す・歩く入力は60回ごとに向きを変え、配置の範囲に留める。
		typename T::FInput Input;
		Input.Move = T::At((StepIndex / 60) % 2 == 0 ? 1.0f : -1.0f, 0);
		if (m_Case == ECapsuleCase::CapsuleMovingFloor)
		{
			Input.Move = T::At(0, 0);
		}
		const Toolbox::uint64 CharacterAllocations = TotalAllocations();
		const Toolbox::uint64 CharacterBegin = NowNanoseconds();
		for (Toolbox::size_t Index = 0; Index < m_States.Size(); ++Index)
		{
			const auto Result = StepCharacter(m_World, m_Settings, m_States[Index], Input, StepSeconds);
			m_States[Index] = Result.State;
			for (Toolbox::uint32 Push = 0; Push < Result.Pushes.Count; ++Push)
			{
				m_World.ApplyLinearImpulse(Result.Pushes.Items[Push].Body, Result.Pushes.Items[Push].Impulse);
			}
			Totals.Pushes += Result.Pushes.Count;
		}
		Totals.CharacterNs += NowNanoseconds() - CharacterBegin;
		Totals.CharacterAllocations += TotalAllocations() - CharacterAllocations;
		const Toolbox::uint64 WorldAllocations = TotalAllocations();
		const Toolbox::uint64 WorldBegin = NowNanoseconds();
		m_World.Step(StepSeconds);
		Totals.WorldNs += NowNanoseconds() - WorldBegin;
		Totals.WorldAllocations += TotalAllocations() - WorldAllocations;
		++Totals.Steps;
		// 以下は計測区間の外。
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
		const auto& Pairs =
		    Probe.m_Values[static_cast<Toolbox::size_t>(PhysicsPrivate::FWorldInteractionProbe::EPhase::SolverPairs)];
		Totals.BroadPhaseNs += Pairs.Nanoseconds;
		Totals.BroadPhaseAllocations += Pairs.Allocations;
#endif
		const auto Diagnostics = m_World.GetExecutionDiagnostics();
		Totals.Candidates += Diagnostics.CandidatePairCount;
		Totals.Manifolds += Diagnostics.ManifoldCount;
		if (m_bEvents)
		{
			const auto& Batch = m_World.GetEventBatch();
			Require(Batch.bPublished && !Batch.bOverflowed, "capsule benchmark event batch");
			Totals.EventPairs += Batch.PairCount;
			Totals.Overflows += Batch.bOverflowed ? 1 : 0;
		}
		for (const auto& State : m_States)
		{
			Require(State.Center.IsValid(), "capsule benchmark character is not finite");
		}
	}
	// 登録したColliderの数（床を含む）。
	Toolbox::uint64 Colliders() const
	{
		return m_World.GetQueryDiagnostics().AliveColliders;
	}
	// 問い合わせ索引の更新（Solverの経路と共有の索引）。
	FWorldQueryDiagnostics Index() const
	{
		return m_World.GetQueryDiagnostics();
	}

private:
	// 条件。
	ECapsuleCase m_Case;
	// キャラクターの数。
	Toolbox::int32 m_Actors;
	// イベントを生成するか。
	bool m_bEvents = false;
	// Worldとキャラクター。
	typename T::FWorld m_World;
	typename T::FSettings m_Settings;
	Toolbox::TVector<typename T::FState> m_States;
	// 往復するKinematicのBody。
	Toolbox::TVector<typename T::FBody> m_Moving;
	// 動く床。
	Toolbox::TOptional<typename T::FBody> m_Floor;
};

// 固定更新1回あたりの値。
Toolbox::f64 PerStep(Toolbox::uint64 Value, Toolbox::uint64 Steps)
{
	return static_cast<Toolbox::f64>(Value) / static_cast<Toolbox::f64>(Steps);
}
// 小数をCSVへ。
Toolbox::FString Decimal(Toolbox::f64 Value)
{
	char Text[64];
	snprintf(Text, sizeof(Text), "%.6f", Value);
	return Text;
}

// 1条件を初回・慣らし・5回の繰り返しで計り、CSVの1行にする。
template <typename T>
FCapsuleTotals RunCase(ECapsuleCase Case, Toolbox::int32 Colliders, Toolbox::int32 Actors, Toolbox::int32 Warmup,
                       Toolbox::int32 Measured)
{
	const Toolbox::uint64 BuildAllocations = TotalAllocations();
	const Toolbox::uint64 BuildBegin = NowNanoseconds();
	TCapsuleScenario<T> Scenario(Case, Colliders, Actors);
	const Toolbox::uint64 BuildNs = NowNanoseconds() - BuildBegin;
	const Toolbox::uint64 BuildAllocated = TotalAllocations() - BuildAllocations;
	FCapsuleTotals Cold;
	Scenario.Step(0, Cold);
	Toolbox::int64 StepIndex = 1;
	FCapsuleTotals Warm;
	for (Toolbox::int32 Index = 0; Index < Warmup; ++Index)
	{
		Scenario.Step(StepIndex++, Warm);
	}
	const FWorldQueryDiagnostics IndexBefore = Scenario.Index();
	Toolbox::f64 WorldUs[Repetitions]{};
	Toolbox::f64 CharacterUs[Repetitions]{};
	Toolbox::f64 BroadPhaseUs[Repetitions]{};
	FCapsuleTotals All;
	for (Toolbox::int32 Repetition = 0; Repetition < Repetitions; ++Repetition)
	{
		FCapsuleTotals Totals;
		for (Toolbox::int32 Index = 0; Index < Measured; ++Index)
		{
			Scenario.Step(StepIndex++, Totals);
		}
		WorldUs[Repetition] = PerStep(Totals.WorldNs, Totals.Steps) / 1000;
		CharacterUs[Repetition] = PerStep(Totals.CharacterNs, Totals.Steps) / 1000;
		BroadPhaseUs[Repetition] = PerStep(Totals.BroadPhaseNs, Totals.Steps) / 1000;
		All.WorldNs += Totals.WorldNs;
		All.CharacterNs += Totals.CharacterNs;
		All.WorldAllocations += Totals.WorldAllocations;
		All.CharacterAllocations += Totals.CharacterAllocations;
		All.BroadPhaseNs += Totals.BroadPhaseNs;
		All.BroadPhaseAllocations += Totals.BroadPhaseAllocations;
		All.Candidates += Totals.Candidates;
		All.Manifolds += Totals.Manifolds;
		All.EventPairs += Totals.EventPairs;
		All.Overflows += Totals.Overflows;
		All.Pushes += Totals.Pushes;
		All.Steps += Totals.Steps;
	}
	const FWorldQueryDiagnostics IndexAfter = Scenario.Index();
	const FSpread World = Summarize(WorldUs);
	const FSpread Character = Summarize(CharacterUs);
	const bool bIndexed = Case != ECapsuleCase::BroadPhaseOff && Case != ECapsuleCase::DenseSolidOff;
	Toolbox::Out << T::Name << ',' << CaseName(Case) << ',' << Scenario.Colliders() << ',' << Actors << ','
	             << (bIndexed ? "on" : "off") << ',' << Decimal(static_cast<Toolbox::f64>(BuildNs) / 1000) << ','
	             << BuildAllocated << ',' << Decimal(static_cast<Toolbox::f64>(Cold.WorldNs) / 1000) << ','
	             << Cold.WorldAllocations << ',';
	Toolbox::Out << Decimal(World.Median) << ',' << Decimal(World.Min) << ',' << Decimal(World.Max) << ','
	             << Decimal(PerStep(All.WorldAllocations, All.Steps)) << ',' << Decimal(Character.Median) << ','
	             << Decimal(PerStep(All.CharacterAllocations, All.Steps)) << ',';
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	if (bIndexed)
	{
		Toolbox::Out << Decimal(Summarize(BroadPhaseUs).Median) << ','
		             << Decimal(PerStep(All.BroadPhaseAllocations, All.Steps)) << ',';
	}
	else
	{
		Toolbox::Out << "NA,NA,";
	}
#else
	(void)BroadPhaseUs;
	Toolbox::Out << "NA,NA,";
#endif
	Toolbox::Out << Decimal(PerStep(All.Candidates, All.Steps)) << ',' << Decimal(PerStep(All.Manifolds, All.Steps))
	             << ',' << Decimal(PerStep(All.EventPairs, All.Steps)) << ',' << All.Overflows << ','
	             << Decimal(PerStep(All.Pushes, All.Steps)) << ','
	             << Decimal(PerStep(IndexAfter.IndexRefreshes - IndexBefore.IndexRefreshes, All.Steps)) << ','
	             << Decimal(PerStep(IndexAfter.IndexReinserts - IndexBefore.IndexReinserts, All.Steps)) << '\n';
	Require(All.Overflows == 0, "capsule benchmark capacity overflow");
	return All;
}

// 次元ごとに全条件を計る。疎な1024個の組の候補の削減を確かめる。
template <typename T> void RunDimension(Toolbox::int32 Warmup, Toolbox::int32 Measured)
{
	const Toolbox::int32 Sizes[] = {64, 512, 1024};
	for (const Toolbox::int32 Size : Sizes)
	{
		RunCase<T>(ECapsuleCase::CapsuleStatic, Size, 8, Warmup, Measured);
		RunCase<T>(ECapsuleCase::CapsuleDense, Size, 0, Warmup, Measured);
		RunCase<T>(ECapsuleCase::CapsuleTrigger, Size, 0, Warmup, Measured);
		const FCapsuleTotals Off = RunCase<T>(ECapsuleCase::BroadPhaseOff, Size, 0, Warmup, Measured);
		const FCapsuleTotals On = RunCase<T>(ECapsuleCase::BroadPhaseOn, Size, 0, Warmup, Measured);
		if (Size == 1024)
		{
			// 疎な配置で、調べる組がほぼ全組（約52万）にならないこと。
			Require(PerStep(On.Candidates, On.Steps) * 100 < PerStep(Off.Candidates, Off.Steps),
			        "sparse broad phase did not reduce narrow-phase pairs");
		}
		RunCase<T>(ECapsuleCase::DenseSolidOff, Size, 0, Warmup, Measured);
		RunCase<T>(ECapsuleCase::DenseSolidOn, Size, 0, Warmup, Measured);
		RunCase<T>(ECapsuleCase::MixedSolidSensor, Size, 0, Warmup, Measured);
	}
	const Toolbox::int32 PushActors[] = {1, 16, 64};
	for (const Toolbox::int32 Actors : PushActors)
	{
		RunCase<T>(ECapsuleCase::Push, 0, Actors, Warmup, Measured);
	}
	RunCase<T>(ECapsuleCase::CapsuleMovingFloor, 0, 8, Warmup, Measured);
	RunCase<T>(ECapsuleCase::CapsuleMovingFloor, 0, 64, Warmup, Measured);
}
} // namespace

void RunCapsuleSeries(Toolbox::int32 Warmup, Toolbox::int32 Measured)
{
#if defined(NDEBUG)
	Toolbox::Out << "# compiled_configuration=Release\n";
#else
	Toolbox::Out << "# compiled_configuration=Debug (use Release for performance acceptance)\n";
#endif
	Toolbox::Out << "# capsule: cold=1, warmup=" << Warmup << ", measured=" << Measured << " x " << Repetitions
	             << " repetitions; times are microseconds per fixed step (median/min/max of repetition means)\n";
	Toolbox::Out << "# colliders include the floor; characters are value-only (no registered body); push applies "
	                "StepCharacter impulses before World.Step\n";
	Toolbox::Out << "# candidate_pairs = pairs that passed the response/filter checks and reached the narrow phase; "
	                "index refreshes/reinserts are shared by queries and the solver pair source\n";
#if defined(DXF_INTERACTION_BENCHMARK_PROBES)
	Toolbox::Out
	    << "# private_phase_probes=on; broadphase = index refresh + pair collection + sort (index path only)\n";
#else
	Toolbox::Out << "# private_phase_probes=off; broadphase columns are NA\n";
#endif
	Toolbox::Out
	    << "dimension,case,colliders,actors,index,build_us,build_allocations,cold_world_us,cold_world_allocations,"
	       "world_us_median,world_us_min,world_us_max,world_allocations_per_step,character_us_median,"
	       "character_allocations_per_step,broadphase_us_median,broadphase_allocations_per_step,"
	       "candidate_pairs_per_step,manifolds_per_step,event_pairs_per_step,capacity_overflows,"
	       "pushes_per_step,index_refreshes_per_step,index_reinserts_per_step\n";
	RunDimension<F2D>(Warmup, Measured);
	RunDimension<F3D>(Warmup, Measured);
}
} // namespace Dxf::Benchmark
