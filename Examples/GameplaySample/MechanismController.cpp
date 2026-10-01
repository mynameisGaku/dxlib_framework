// SPDX-License-Identifier: NOASSERTION
#include "MechanismController.h"
#include "Toolbox/Utility.h"
namespace Dxf::GameplaySample
{
void FMechanismController::Select(Toolbox::int32 Index)
{
	if (Index < 0 || Index > 4)
	{
		throw Toolbox::FException("Mechanism selection out of range");
	}
	m_Selection = Index;
}
void FMechanismController::SetMotion(Toolbox::f64 Target, Toolbox::f64 Speed, Toolbox::f64 Effort)
{
	if (!Toolbox::IsFinite(Target) || !Toolbox::IsFinite(Speed) || !Toolbox::IsFinite(Effort) || Target < -1 || Target > 1 || Speed < 0 || Effort < 0)
	{
		throw Toolbox::FException("Invalid mechanism motion request");
	}
	m_Values[m_Selection].Target = Target;
	m_Values[m_Selection].Speed = Speed;
	m_Values[m_Selection].Effort = Effort;
}
const FMechanismOperationValues& FMechanismController::GetValues(Toolbox::int32 Index) const
{
	if (Index < 0 || Index > 4)
	{
		throw Toolbox::FException("Mechanism selection out of range");
	}
	return m_Values[Index];
}
} // namespace Dxf::GameplaySample
