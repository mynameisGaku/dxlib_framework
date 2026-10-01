// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SceneContentTicket.h"
#include "Dxf/ContentRequestState.h"
namespace Dxf
{
FSceneContentTicket::FSceneContentTicket(Toolbox::TSharedPtr<ContentPrivate::FContentRequestState> State)
    : m_State(Toolbox::Move(State))
{
}
ESceneContentRequestState FSceneContentTicket::GetState() const noexcept
{
	return m_State ? static_cast<ESceneContentRequestState>(m_State->Status.Load())
	               : ESceneContentRequestState::Canceled;
}
FSceneContentDiagnostic FSceneContentTicket::GetDiagnostic() const
{
	if (!m_State)
	{
		return {};
	}
	Toolbox::FScopedLock Lock(m_State->Mutex);
	auto Result = m_State->Diagnostic;
	if (Result.Reason.IsEmpty() && m_State->FailureReason[0] != 0)
	{
		Result.Reason = m_State->FailureReason;
	}
	return Result;
}
Toolbox::uint64 FSceneContentTicket::GetSequence() const noexcept
{
	return m_State ? m_State->Sequence : 0;
}
} // namespace Dxf
