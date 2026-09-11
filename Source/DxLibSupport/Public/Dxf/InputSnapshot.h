#pragma once
#include "Dxf/InputTypes.h"

namespace Dxf
{
class FInputSnapshot
{
public:
	bool IsDown(EKey Key) const noexcept
	{
		return Read_Internal(m_Current.Keys, static_cast<std::size_t>(Key));
	}
	bool WasPressed(EKey Key) const noexcept
	{
		return IsDown(Key) && !Read_Internal(m_Previous.Keys, static_cast<std::size_t>(Key));
	}
	bool WasReleased(EKey Key) const noexcept
	{
		return !IsDown(Key) && Read_Internal(m_Previous.Keys, static_cast<std::size_t>(Key));
	}
	bool IsMouseDown(EMouseButton Button) const noexcept
	{
		return Read_Internal(m_Current.MouseButtons, static_cast<std::size_t>(Button));
	}
	bool WasMousePressed(EMouseButton Button) const noexcept
	{
		return IsMouseDown(Button) && !Read_Internal(m_Previous.MouseButtons, static_cast<std::size_t>(Button));
	}
	bool WasMouseReleased(EMouseButton Button) const noexcept
	{
		return !IsMouseDown(Button) && Read_Internal(m_Previous.MouseButtons, static_cast<std::size_t>(Button));
	}
	bool IsPadDown(std::size_t Pad, std::size_t Button) const noexcept
	{
		return PadRead_Internal(m_Current, Pad, Button);
	}
	bool WasPadPressed(std::size_t Pad, std::size_t Button) const noexcept
	{
		return IsPadDown(Pad, Button) && !PadRead_Internal(m_Previous, Pad, Button);
	}
	bool WasPadReleased(std::size_t Pad, std::size_t Button) const noexcept
	{
		return !IsPadDown(Pad, Button) && PadRead_Internal(m_Previous, Pad, Button);
	}
	const FRawInput& GetRaw() const noexcept
	{
		return m_Current;
	}
private:
	friend class FInputStateTracker;
	template <std::size_t N> static bool Read_Internal(const std::array<bool, N>& Values, std::size_t Index) noexcept
	{
		return Index < N && Values[Index];
	}
	static bool PadRead_Internal(const FRawInput& State, std::size_t Pad, std::size_t Button) noexcept
	{
		return Pad < State.Pads.size() && State.Pads[Pad].bConnected && Read_Internal(State.Pads[Pad].Buttons, Button);
	}
	FRawInput m_Current;
	FRawInput m_Previous;
};
}
