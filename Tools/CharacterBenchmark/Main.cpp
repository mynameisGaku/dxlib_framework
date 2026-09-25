// SPDX-License-Identifier: NOASSERTION
// キャラクター移動（StepCharacter）の負荷を、障害物の数とキャラクターの数を変えて測る。CTestには含めない。
// 手順: 各条件で同じ配置・同じ入力列を作り、30回の固定更新で慣らした後、120回の固定更新を5回繰り返して計る。
// 集計: 各繰り返しの「キャラクター1体・固定更新1回あたりの平均時間」（移動計算だけ。World.Stepは別に計る）を求め、
// 5回の中央値・最小・最大を出す。
// 問い合わせ・反復・接触の数は全繰り返しの平均と最大。確保は、測定と別の1回の固定更新を確保禁止で実行して確認する。
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/CharacterMovement2D.h"
#include "Dxf/CharacterMovement3D.h"
#include "Toolbox/Platform.h"
#include "Toolbox/Utility.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
#include <stdio.h>
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 慣らしの固定更新の回数。
constexpr int32 WarmupSteps = 30;
// 1回の繰り返しで計る固定更新の回数。
constexpr int32 MeasuredSteps = 120;
// 繰り返しの回数。
constexpr int32 Repetitions = 5;
// 固定更新の間隔（秒）。
constexpr f64 StepSeconds = 1.0 / 60.0;

// 1回の固定更新の集計。
struct FCounters
{
	// 問い合わせの合計。
	int64 Queries = 0;
	// 問い合わせの最大（1体・1回）。
	int32 MaxQueries = 0;
	// 反復の合計（水平と垂直）。
	int64 Iterations = 0;
	// 反復の最大（1体・1回）。
	int32 MaxIterations = 0;
	// 接触の合計（水平と垂直）。
	int64 Contacts = 0;
	// 問い合わせの上限で止まった回数。
	int64 QueryLimits = 0;
	// 段差を上った回数。
	int64 StepUps = 0;
	// 集計した移動の回数。
	int64 Moves = 0;
	// 移動計算の時間（ナノ秒）。
	uint64 MoveNanoseconds = 0;
	// World.Stepの時間（ナノ秒）。
	uint64 WorldNanoseconds = 0;
};

// 1回分の結果を集計へ加える。
template <typename TResult> void Count_Internal(FCounters& Counters, const TResult& Result)
{
	const int32 Iterations = Result.Horizontal.Iterations + Result.Vertical.Iterations;
	Counters.Queries += Result.Queries;
	Counters.MaxQueries = Max(Counters.MaxQueries, Result.Queries);
	Counters.Iterations += Iterations;
	Counters.MaxIterations = Max(Counters.MaxIterations, Iterations);
	Counters.Contacts += Result.Horizontal.ContactCount + Result.Vertical.ContactCount;
	Counters.QueryLimits += Result.Horizontal.Stop == ECharacterMoveStop::QueryLimit ||
	                                Result.Vertical.Stop == ECharacterMoveStop::QueryLimit
	                            ? 1
	                            : 0;
	Counters.StepUps += Result.bSteppedUp ? 1 : 0;
	++Counters.Moves;
}

// 決まった入力列（向きは個体ごとにずらし、時間とともに回す。90回に1回跳ぶ）。
f32 Angle_Internal(int32 Character, int64 Step)
{
	return static_cast<f32>(Character) * 2.39996f + static_cast<f32>(Step) * 0.01f;
}

// 2Dの条件。キャラクターごとに別の床（高さ10ずつ）を置き、障害物は床へ順に配る。
struct FScenario2D
{
	FPhysicsWorld2D World;
	FCharacterMoveSettings2D Settings;
	FCharacterState2D States[64];
	FBodyId2D Bodies[64];
	int32 Characters = 0;
	FScenario2D(int32 Obstacles, int32 InCharacters) : Characters(InCharacters)
	{
		const int32 PerLane = (Obstacles + Characters - 1) / Characters;
		const f32 Half = static_cast<f32>(PerLane) * 1.5f + 3.0f;
		const FBodyId2D Level = World.CreateBody({});
		FColliderDescription2D Box;
		for (int32 Lane = 0; Lane < Characters; ++Lane)
		{
			const f32 Floor = static_cast<f32>(Lane) * 10.0f;
			Box.Shape = FOrientedBox2D{{0, Floor - 1}, {Half, 1}, 0};
			World.AttachCollider(Level, Box);
		}
		for (int32 Index = 0; Index < Obstacles; ++Index)
		{
			const int32 Lane = Index % Characters;
			const int32 Slot = Index / Characters;
			const f32 Floor = static_cast<f32>(Lane) * 10.0f;
			const f32 X = -Half + 3.0f + static_cast<f32>(Slot) * 3.0f + 1.5f;
			// 低い段差・高い壁・30度の坂を順に置く。
			switch (Slot % 3)
			{
			case 0:
				Box.Shape = FOrientedBox2D{{X, Floor + 0.1f}, {0.5f, 0.1f}, 0};
				break;
			case 1:
				Box.Shape = FOrientedBox2D{{X, Floor + 0.6f}, {0.5f, 0.6f}, 0};
				break;
			default:
				Box.Shape = FOrientedBox2D{{X, Floor}, {1.0f, 0.3f}, 0.5236f};
				break;
			}
			World.AttachCollider(Level, Box);
		}
		for (int32 Index = 0; Index < Characters; ++Index)
		{
			States[Index].Center = {-Half + 1.5f, static_cast<f32>(Index) * 10.0f + 0.52f};
			FBodyDescription2D Body;
			Body.Type = EBodyType::Kinematic;
			Body.Position = States[Index].Center;
			Bodies[Index] = World.CreateBody(Body);
			FColliderDescription2D Collider;
			Collider.Shape = FCircle2D{{}, Settings.Radius};
			World.AttachCollider(Bodies[Index], Collider);
		}
	}
	// 全員を1回進める。
	void Step(int64 Step, FCounters* pCounters)
	{
		const uint64 Begin = MonotonicNanoseconds();
		for (int32 Index = 0; Index < Characters; ++Index)
		{
			FCharacterMoveInput2D Input;
			Input.Move = {Cos(Angle_Internal(Index, Step)) >= 0 ? 1.0f : -1.0f, 0};
			Input.bJump = (Step + Index) % 90 == 0;
			const auto Result = StepCharacter(World, Settings, States[Index], Input, StepSeconds, Bodies[Index]);
			States[Index] = Result.State;
			World.SetBodyTransform(Bodies[Index], States[Index].Center, 0);
			if (pCounters != nullptr)
			{
				Count_Internal(*pCounters, Result);
			}
		}
		const uint64 Moved = MonotonicNanoseconds();
		World.Step(StepSeconds);
		if (pCounters != nullptr)
		{
			pCounters->MoveNanoseconds += Moved - Begin;
			pCounters->WorldNanoseconds += MonotonicNanoseconds() - Moved;
		}
	}
};

// 3Dの条件。一つの床の上に障害物を格子状に置き、キャラクターは格子の隅から歩く（互いのBodyも障害物）。
struct FScenario3D
{
	FPhysicsWorld3D World;
	FCharacterMoveSettings3D Settings;
	FCharacterState3D States[64];
	FBodyId3D Bodies[64];
	int32 Characters = 0;
	FScenario3D(int32 Obstacles, int32 InCharacters) : Characters(InCharacters)
	{
		int32 Side = 1;
		while (Side * Side < Obstacles || Side * Side < Characters)
		{
			++Side;
		}
		const f32 Half = static_cast<f32>(Side) * 1.5f;
		const FBodyId3D Level = World.CreateBody({});
		FColliderDescription3D Box;
		Box.Shape = FOBB{{0, -1, 0}, {Half + 2, 1, Half + 2}};
		World.AttachCollider(Level, Box);
		for (int32 Index = 0; Index < Obstacles; ++Index)
		{
			const f32 X = -Half + static_cast<f32>(Index % Side) * 3.0f + 1.5f;
			const f32 Z = -Half + static_cast<f32>(Index / Side) * 3.0f + 1.5f;
			switch (Index % 3)
			{
			case 0:
				Box.Shape = FOBB{{X, 0.1f, Z}, {0.5f, 0.1f, 0.5f}};
				break;
			case 1:
				Box.Shape = FOBB{{X, 0.6f, Z}, {0.5f, 0.6f, 0.5f}};
				break;
			default:
			{
				// 30度の坂（Z軸回りに回したOBB）。
				FOBB Ramp{{X, 0, Z}, {1.0f, 0.3f, 0.5f}};
				const f32 C = static_cast<f32>(Cos(0.5236));
				const f32 S = static_cast<f32>(Sin(0.5236));
				Ramp.Axes[0] = {C, S, 0};
				Ramp.Axes[1] = {-S, C, 0};
				Box.Shape = Ramp;
			}
			break;
			}
			World.AttachCollider(Level, Box);
		}
		for (int32 Index = 0; Index < Characters; ++Index)
		{
			States[Index].Center = {-Half + static_cast<f32>(Index % Side) * 3.0f, 0.52f,
			                        -Half + static_cast<f32>(Index / Side) * 3.0f};
			FBodyDescription3D Body;
			Body.Type = EBodyType::Kinematic;
			Body.Position = States[Index].Center;
			Bodies[Index] = World.CreateBody(Body);
			FColliderDescription3D Collider;
			Collider.Shape = FSphere{{}, Settings.Radius};
			World.AttachCollider(Bodies[Index], Collider);
		}
	}
	// 全員を1回進める。
	void Step(int64 Step, FCounters* pCounters)
	{
		const uint64 Begin = MonotonicNanoseconds();
		for (int32 Index = 0; Index < Characters; ++Index)
		{
			const f32 Angle = Angle_Internal(Index, Step);
			FCharacterMoveInput3D Input;
			Input.Move = {static_cast<f32>(Cos(Angle)), 0, static_cast<f32>(Sin(Angle))};
			Input.bJump = (Step + Index) % 90 == 0;
			const auto Result = StepCharacter(World, Settings, States[Index], Input, StepSeconds, Bodies[Index]);
			States[Index] = Result.State;
			World.SetBodyTransform(Bodies[Index], States[Index].Center, FQuaternion{});
			if (pCounters != nullptr)
			{
				Count_Internal(*pCounters, Result);
			}
		}
		const uint64 Moved = MonotonicNanoseconds();
		World.Step(StepSeconds);
		if (pCounters != nullptr)
		{
			pCounters->MoveNanoseconds += Moved - Begin;
			pCounters->WorldNanoseconds += MonotonicNanoseconds() - Moved;
		}
	}
};

// 小さい順に並べ替える（5要素）。
void Sort_Internal(f64 (&Values)[Repetitions])
{
	for (int32 Outer = 1; Outer < Repetitions; ++Outer)
	{
		for (int32 Inner = Outer; Inner > 0 && Values[Inner] < Values[Inner - 1]; --Inner)
		{
			const f64 Swap = Values[Inner];
			Values[Inner] = Values[Inner - 1];
			Values[Inner - 1] = Swap;
		}
	}
}

// 一つの条件を計って1行を出す。確保が起きたらfalse。
template <typename TScenario> bool Measure_Internal(const char* Dimension, int32 Obstacles, int32 Characters)
{
	TScenario Scenario(Obstacles, Characters);
	int64 Step = 0;
	for (int32 Index = 0; Index < WarmupSteps; ++Index)
	{
		Scenario.Step(Step++, nullptr);
	}
	// 確保の確認: キャラクターの移動だけを確保禁止で1回行う（World.Stepは対象外）。
	bool bAllocated = false;
	for (int32 Index = 0; Index < Scenario.Characters; ++Index)
	{
		Testing::SetAllocationFailureCountdown(0);
		const auto Result = StepCharacter(Scenario.World, Scenario.Settings, Scenario.States[Index], {}, StepSeconds,
		                                  Scenario.Bodies[Index]);
		bAllocated = bAllocated || Testing::WasAllocationFailureInjected() || Result.Queries <= 0;
		Testing::SetAllocationFailureCountdown(-1);
	}
	FCounters Counters;
	f64 PerCharacterMicroseconds[Repetitions];
	for (int32 Repetition = 0; Repetition < Repetitions; ++Repetition)
	{
		const uint64 Before = Counters.MoveNanoseconds;
		for (int32 Index = 0; Index < MeasuredSteps; ++Index)
		{
			Scenario.Step(Step++, &Counters);
		}
		PerCharacterMicroseconds[Repetition] = static_cast<f64>(Counters.MoveNanoseconds - Before) / 1000.0 /
		                                       (static_cast<f64>(MeasuredSteps) * Characters);
	}
	Sort_Internal(PerCharacterMicroseconds);
	const f64 Moves = static_cast<f64>(Counters.Moves);
	const f64 WorldMicroseconds =
	    static_cast<f64>(Counters.WorldNanoseconds) / 1000.0 / (static_cast<f64>(MeasuredSteps) * Repetitions);
	printf("| %s | %d | %d | %.2f | %.2f | %.2f | %.1f | %.2f / %d | %.2f / %d | %.2f | %lld | %lld | %s |\n",
	       Dimension, Obstacles, Characters, PerCharacterMicroseconds[Repetitions / 2], PerCharacterMicroseconds[0],
	       PerCharacterMicroseconds[Repetitions - 1], WorldMicroseconds, static_cast<f64>(Counters.Queries) / Moves,
	       Counters.MaxQueries, static_cast<f64>(Counters.Iterations) / Moves, Counters.MaxIterations,
	       static_cast<f64>(Counters.Contacts) / Moves, static_cast<long long>(Counters.QueryLimits),
	       static_cast<long long>(Counters.StepUps), bAllocated ? "ALLOCATED" : "0");
	fflush(stdout);
	return !bAllocated;
}
} // namespace

int main()
{
	printf("warmup=%d steps, measured=%d steps x %d repetitions, dt=1/60, time = per character-step "
	       "(movement only), median/min/max of repetitions; world us = mean World.Step per fixed step\n",
	       WarmupSteps, MeasuredSteps, Repetitions);
	printf("| dim | obstacles | characters | median us | min us | max us | world us | queries avg / max | iterations "
	       "avg / max "
	       "| contacts avg | query-limit stops | step-ups | allocations |\n");
	printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
	bool bOk = true;
	const int32 ObstacleCounts[] = {16, 128, 512};
	const int32 CharacterCounts[] = {1, 8, 32};
	for (const int32 Obstacles : ObstacleCounts)
	{
		for (const int32 Characters : CharacterCounts)
		{
			bOk = Measure_Internal<FScenario2D>("2D", Obstacles, Characters) && bOk;
		}
	}
	for (const int32 Obstacles : ObstacleCounts)
	{
		for (const int32 Characters : CharacterCounts)
		{
			bOk = Measure_Internal<FScenario3D>("3D", Obstacles, Characters) && bOk;
		}
	}
	return bOk ? 0 : 1;
}
