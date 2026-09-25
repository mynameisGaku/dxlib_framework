// SPDX-License-Identifier: NOASSERTION
#include "SampleHud.h"
namespace Dxf::GameplaySample
{
void RequireSample(TResult<void> Result)
{
	if (!Result)
	{
		throw Toolbox::FException(Result.Error().Message);
	}
}
const char* MoveStopName(ECharacterMoveStop Stop) noexcept
{
	switch (Stop)
	{
	case ECharacterMoveStop::NoMovement:
		return "none";
	case ECharacterMoveStop::Completed:
		return "completed";
	case ECharacterMoveStop::Slid:
		return "slid";
	case ECharacterMoveStop::Blocked:
		return "blocked";
	case ECharacterMoveStop::MissingNormal:
		return "no-normal";
	case ECharacterMoveStop::AmbiguousContact:
		return "ambiguous";
	case ECharacterMoveStop::PrecisionLimit:
		return "precision";
	case ECharacterMoveStop::IterationLimit:
		return "iterations";
	case ECharacterMoveStop::ContactLimit:
		return "contacts";
	case ECharacterMoveStop::QueryLimit:
		return "queries";
	}
	return "?";
}
const char* GroundName(ECharacterGroundState State) noexcept
{
	switch (State)
	{
	case ECharacterGroundState::Airborne:
		return "airborne";
	case ECharacterGroundState::Walkable:
		return "walkable";
	case ECharacterGroundState::Steep:
		return "steep";
	}
	return "?";
}
const char* RecoveryName(ECharacterRecoveryStatus Status) noexcept
{
	switch (Status)
	{
	case ECharacterRecoveryStatus::NoOverlap:
		return "none";
	case ECharacterRecoveryStatus::Resolved:
		return "resolved";
	case ECharacterRecoveryStatus::Ambiguous:
		return "ambiguous";
	case ECharacterRecoveryStatus::TooDeep:
		return "too-deep";
	case ECharacterRecoveryStatus::Blocked:
		return "blocked";
	case ECharacterRecoveryStatus::IterationLimit:
		return "iterations";
	case ECharacterRecoveryStatus::ContactLimit:
		return "contacts";
	case ECharacterRecoveryStatus::QueryLimit:
		return "queries";
	}
	return "?";
}
} // namespace Dxf::GameplaySample
