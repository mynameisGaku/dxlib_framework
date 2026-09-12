// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/FixedStepScheduler.h"
namespace Toolbox
{
FFixedStepScheduler::FFixedStepScheduler(FFixedStepSettings Settings) : m_Settings(Settings)
{
	if (!IsFinite(Settings.StepSeconds) || Settings.StepSeconds <= 0 || Settings.MaxStepsPerFrame == 0 ||
	    Settings.MaxStepsPerFrame > 1024 || !IsFinite(Settings.MaximumFrameSeconds) ||
	    Settings.MaximumFrameSeconds <= 0 || !IsFinite(Settings.StepSeconds * Settings.MaxStepsPerFrame))
	{
		throw FException("Invalid fixed-step settings");
	}
}
FFixedStepPlan FFixedStepScheduler::Advance(f64 ElapsedSeconds)
{
	if (!IsFinite(ElapsedSeconds) || ElapsedSeconds < 0)
	{
		throw FException("Invalid elapsed seconds");
	}
	const f64 Accepted = Min(ElapsedSeconds, m_Settings.MaximumFrameSeconds);
	const f64 Available = m_Accumulator + Accepted;
	if (!IsFinite(Available))
	{
		throw FException("Fixed-step accumulator overflow");
	}
	FFixedStepPlan Plan;
	Plan.StepSeconds = m_Settings.StepSeconds;
	const f64 Budget = Plan.StepSeconds * m_Settings.MaxStepsPerFrame;
	if (Available >= Budget)
	{
		Plan.StepCount = m_Settings.MaxStepsPerFrame;
	}
	else
	{
		// 除算は予算未満のときだけ行い、極小ステップでも整数変換を範囲内に収める。
		const f64 Quotient = Available / Plan.StepSeconds;
		const f64 Roundoff = 8 * DBL_EPSILON * Max(1.0, Quotient);
		Plan.StepCount = Min(m_Settings.MaxStepsPerFrame, static_cast<uint32>(floor(Quotient + Roundoff)));
	}
	const f64 Remaining = Max(0.0, Available - Plan.StepCount * Plan.StepSeconds);
	f64 Remainder = Remaining >= Plan.StepSeconds ? fmod(Remaining, Plan.StepSeconds) : Remaining;
	// 二進数に変換した固定幅の境界に限り、機械精度内の残差を次の境界へそろえる。
	if (Plan.StepSeconds - Remainder <= 8 * DBL_EPSILON * Plan.StepSeconds)
	{
		Remainder = 0;
	}
	Plan.DroppedSeconds = (ElapsedSeconds - Accepted) + (Remaining - Remainder);
	Plan.InterpolationAlpha = Remainder / Plan.StepSeconds;
	if (!IsFinite(Plan.DroppedSeconds) || !IsFinite(Plan.InterpolationAlpha) || Plan.InterpolationAlpha < 0 ||
	    Plan.InterpolationAlpha >= 1)
	{
		throw FException("Fixed-step plan overflow");
	}
	// 計画全体の検査に成功してから状態を確定する。
	m_Accumulator = Remainder;
	return Plan;
}
} // namespace Toolbox
