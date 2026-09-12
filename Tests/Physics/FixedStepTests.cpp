// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Toolbox/FixedStepScheduler.h"
using namespace Toolbox;
namespace
{
bool Near_Internal(f64 A, f64 B, f64 Epsilon = 1e-12)
{
	return Abs(A - B) <= Epsilon;
}
void FractionalFrames_Internal()
{
	FFixedStepScheduler Clock({0.125, 8, 1});
	PHYSICS_REQUIRE(Clock.Advance(0.0625).StepCount == 0);
	const FFixedStepPlan Plan = Clock.Advance(0.0625);
	PHYSICS_REQUIRE(Plan.StepCount == 1 && Plan.StepSeconds == 0.125);
	PHYSICS_REQUIRE(Plan.InterpolationAlpha == 0 && Plan.DroppedSeconds == 0);
}
void FrameRateIndependent_Internal()
{
	const int32 Rates[] = {30, 60, 120, 144, 240};
	for (int32 Rate : Rates)
	{
		FFixedStepScheduler Clock;
		uint32 Steps = 0;
		for (int32 Frame = 0; Frame < Rate; ++Frame)
		{
			Steps += Clock.Advance(1.0 / Rate).StepCount;
		}
		PHYSICS_REQUIRE(Steps == 60);
		PHYSICS_REQUIRE(Near_Internal(Clock.GetRemainderSeconds(), 0));
	}
}
void SpikeBudget_Internal()
{
	FFixedStepScheduler Clock;
	const FFixedStepPlan Plan = Clock.Advance(1);
	PHYSICS_REQUIRE(Plan.StepCount == 8);
	PHYSICS_REQUIRE(Near_Internal(Plan.DroppedSeconds, 1 - 8.0 / 60));
	PHYSICS_REQUIRE(Near_Internal(Plan.InterpolationAlpha, 0));
}
void ClampKeepsRemainder_Internal()
{
	FFixedStepScheduler Clock({0.1, 8, 0.25});
	const FFixedStepPlan Plan = Clock.Advance(1);
	PHYSICS_REQUIRE(Plan.StepCount == 2);
	PHYSICS_REQUIRE(Near_Internal(Plan.InterpolationAlpha, 0.5));
	PHYSICS_REQUIRE(Near_Internal(Plan.DroppedSeconds, 0.75));
}
void CatchUpDropIsExplicit_Internal()
{
	FFixedStepScheduler Clock({0.125, 2, 1});
	const FFixedStepPlan Plan = Clock.Advance(0.9375);
	PHYSICS_REQUIRE(Plan.StepCount == 2);
	PHYSICS_REQUIRE(Plan.DroppedSeconds == 0.625);
	PHYSICS_REQUIRE(Plan.InterpolationAlpha == 0.5);
	PHYSICS_REQUIRE(Clock.Advance(0.0625).StepCount == 1);
}
void InvalidElapsedPreservesState_Internal()
{
	FFixedStepScheduler Clock({0.125, 2, 1});
	(void)Clock.Advance(0.0625);
	const f64 Invalid[] = {-1, TNumericLimits<f64>::QuietNaN()};
	for (f64 Time : Invalid)
	{
		bool bThrown = false;
		try
		{
			(void)Clock.Advance(Time);
		}
		catch (const FException&)
		{
			bThrown = true;
		}
		PHYSICS_REQUIRE(bThrown && Clock.GetRemainderSeconds() == 0.0625);
	}
}
void InvalidSettings_Internal()
{
	const FFixedStepSettings Settings[] = {{0, 8, 1},
	                                       {-1, 8, 1},
	                                       {0.1, 0, 1},
	                                       {0.1, 1025, 1},
	                                       {0.1, 8, 0},
	                                       {1e308, 8, 1},
	                                       {TNumericLimits<f64>::QuietNaN(), 8, 1}};
	for (const FFixedStepSettings& Value : Settings)
	{
		bool bThrown = false;
		try
		{
			FFixedStepScheduler Clock(Value);
		}
		catch (const FException&)
		{
			bThrown = true;
		}
		PHYSICS_REQUIRE(bThrown);
	}
}
void Reset_Internal()
{
	FFixedStepScheduler Clock({0.125, 8, 1});
	(void)Clock.Advance(0.0625);
	Clock.Reset();
	PHYSICS_REQUIRE(Clock.GetRemainderSeconds() == 0);
	PHYSICS_REQUIRE(Clock.GetStepSeconds() == 0.125);
	PHYSICS_REQUIRE(Clock.Advance(0.0625).StepCount == 0);
}
void LargeInputAndSmallSteps_Internal()
{
	FFixedStepScheduler Clock;
	const FFixedStepPlan Large = Clock.Advance(TNumericLimits<f64>::Max());
	PHYSICS_REQUIRE(Large.StepCount == 8 && IsFinite(Large.DroppedSeconds));
	FFixedStepScheduler Tiny({1e-300, 4, 1});
	const FFixedStepPlan Plan = Tiny.Advance(0.25);
	PHYSICS_REQUIRE(Plan.StepCount == 4);
	PHYSICS_REQUIRE(Plan.InterpolationAlpha >= 0 && Plan.InterpolationAlpha < 1);
}
void AccumulatorOverflowPreservesState_Internal()
{
	FFixedStepScheduler Clock({1e308, 1, 1.7e308});
	(void)Clock.Advance(0.9e308);
	bool bThrown = false;
	try
	{
		(void)Clock.Advance(1.7e308);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown && Clock.GetRemainderSeconds() == 0.9e308);
}
void LongRunTimeAccounting_Internal()
{
	FFixedStepScheduler Clock;
	f64 Input = 0;
	f64 Simulated = 0;
	f64 Dropped = 0;
	for (int32 Frame = 0; Frame < 10000; ++Frame)
	{
		const f64 Delta = Frame % 97 == 0 ? 0.5 : (Frame % 7 + 1) / 1000.0;
		const FFixedStepPlan Plan = Clock.Advance(Delta);
		Input += Delta;
		Simulated += Plan.StepCount * Plan.StepSeconds;
		Dropped += Plan.DroppedSeconds;
		PHYSICS_REQUIRE(Plan.StepCount <= 8);
		PHYSICS_REQUIRE(Plan.InterpolationAlpha >= 0 && Plan.InterpolationAlpha < 1);
	}
	PHYSICS_REQUIRE(Near_Internal(Input, Simulated + Dropped + Clock.GetRemainderSeconds(), 1e-8));
}
const PhysicsTest::FCase Cases[] = {
    {"fractional frames accumulate fixed steps", FractionalFrames_Internal},
    {"30 60 120 144 240 Hz produce the same 60 steps", FrameRateIndependent_Internal},
    {"large frame obeys a bounded catch-up budget", SpikeBudget_Internal},
    {"frame clamp retains interpolation remainder", ClampKeepsRemainder_Internal},
    {"discarded backlog time is explicit", CatchUpDropIsExplicit_Internal},
    {"invalid elapsed time does not mutate the scheduler", InvalidElapsedPreservesState_Internal},
    {"invalid scheduler settings are rejected", InvalidSettings_Internal},
    {"reset keeps configuration and clears time", Reset_Internal},
    {"huge elapsed and tiny steps avoid integer overflow", LargeInputAndSmallSteps_Internal},
    {"accumulator overflow leaves state unchanged", AccumulatorOverflowPreservesState_Internal},
    {"10000-frame time accounting", LongRunTimeAccounting_Internal},
};
} // namespace
namespace PhysicsTest
{
const FCase* GetFixedStepCases(size_t& Count) noexcept
{
	Count = sizeof(Cases) / sizeof(Cases[0]);
	return Cases;
}
} // namespace PhysicsTest
