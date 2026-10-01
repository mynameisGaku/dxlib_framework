// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SceneContentError.h"
namespace Dxf
{
FSceneContentError::FSceneContentError(FSceneContentDiagnostic Diagnostic)
    : Toolbox::FException(Diagnostic.Path + ":" + Toolbox::ToString(Diagnostic.Line) + ":" + Toolbox::ToString(Diagnostic.Column) + " " + Diagnostic.Location + " " + Diagnostic.Reason),
      m_Diagnostic(Toolbox::Move(Diagnostic))
{
}
const FSceneContentDiagnostic& FSceneContentError::GetDiagnostic() const noexcept
{
	return m_Diagnostic;
}
} // namespace Dxf
