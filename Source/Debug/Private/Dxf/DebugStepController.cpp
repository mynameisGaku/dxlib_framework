// SPDX-License-Identifier: NOASSERTION
#include "Dxf/DebugStepController.h"
namespace Dxf
{
void FDebugStepController::Reset() noexcept
{
	m_Scheduler.Reset();
	m_bPendingStep = false;
}
void FDebugStepController::SetPaused(bool Paused) noexcept
{
	if (m_bPaused != Paused)
	{
		m_bPaused = Paused;
		m_Scheduler.Reset();
		m_bPendingStep = false;
	}
}
bool FDebugStepController::RequestSingleStep() noexcept
{
	if (!m_bPaused || m_bPendingStep)
	{
		return false;
	}
	m_bPendingStep = true;
	return true;
}
TResult<void> FDebugStepController::SetTimeScale(Toolbox::f64 Scale)
{
	if (!Toolbox::IsFinite(Scale) || Scale < 0.01 || Scale > 4)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid debug time scale");
	}
	m_TimeScale = Scale;
	return {};
}
TResult<FDebugStepPlan> FDebugStepController::Plan(Toolbox::f64 RealSeconds)
{
	if (!Toolbox::IsFinite(RealSeconds) || RealSeconds < 0 || RealSeconds > 3600)
	{
		return TResult<FDebugStepPlan>::Failure(EErrorCode::InvalidArgument, "Invalid debug frame time");
	}
	FDebugStepPlan Plan;
	Plan.StepSeconds = m_Scheduler.GetStepSeconds();
	if (m_bPaused)
	{
		Plan.StepCount = m_bPendingStep ? 1 : 0;
		m_bPendingStep = false;
		return TResult<FDebugStepPlan>::Success(Plan);
	}
	// 固定幅の丸め・過負荷の時間破棄は既存Toolboxの契約へ集約する。
	const auto Fixed = m_Scheduler.Advance(RealSeconds * m_TimeScale);
	Plan.StepCount = Fixed.StepCount;
	Plan.StepSeconds = Fixed.StepSeconds;
	Plan.DroppedSeconds = Fixed.DroppedSeconds;
	return TResult<FDebugStepPlan>::Success(Plan);
}
}
