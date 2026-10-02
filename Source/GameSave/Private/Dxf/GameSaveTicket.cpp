// SPDX-License-Identifier: NOASSERTION
#include "Dxf/SaveRequestState.h"
namespace Dxf::SavePrivate
{
void SaveReason(FRequestState& State, const char* Reason) noexcept
{
	// 終端を保ちながらコピーする位置。
	Toolbox::size_t Index = 0;
	while (Reason[Index] != 0 && Index + 1 < sizeof(State.Reason))
	{
		State.Reason[Index] = Reason[Index];
		++Index;
	}
	State.Reason[Index] = 0;
}
} // namespace Dxf::SavePrivate
namespace Dxf
{
FGameSaveTicket::FGameSaveTicket(Toolbox::TSharedPtr<SavePrivate::FRequestState> State) : m_State(Toolbox::Move(State))
{
}
EGameSaveRequestState FGameSaveTicket::GetState() const noexcept
{
	return m_State ? static_cast<EGameSaveRequestState>(m_State->Status.Load()) : EGameSaveRequestState::Canceled;
}
Toolbox::FString FGameSaveTicket::GetReason() const
{
	if (!m_State)
	{
		return {};
	}
	Toolbox::FScopedLock Lock(m_State->Mutex);
	return Toolbox::FString(m_State->Reason);
}
Toolbox::uint64 FGameSaveTicket::GetSequence() const noexcept
{
	return m_State ? m_State->Sequence : 0;
}
} // namespace Dxf
