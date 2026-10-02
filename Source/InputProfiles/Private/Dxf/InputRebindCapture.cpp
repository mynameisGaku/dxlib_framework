// SPDX-License-Identifier: NOASSERTION
#include "Dxf/InputRebindCapture.h"
namespace Dxf
{
bool FInputRebindCapture::Begin(EInputBindingDevice Device, Toolbox::uint32 PadSlot, const FInputSnapshot& Current) noexcept
{
	m_Binding = {};
	m_Binding.Device = Device;
	m_Binding.PadSlot = PadSlot;
	m_State = EInputRebindState::Canceled;
	if ((Device != EInputBindingDevice::Keyboard && Device != EInputBindingDevice::Mouse && Device != EInputBindingDevice::Gamepad) || !Current.GetRaw().bFocused || (Device == EInputBindingDevice::Gamepad && (PadSlot >= 4 || !Current.GetRaw().Pads[PadSlot].bConnected)))
	{
		return false;
	}
	m_State = EInputRebindState::WaitingRelease;
	return true;
}
void FInputRebindCapture::Poll(const FInputSnapshot& Current) noexcept
{
	if (m_State != EInputRebindState::WaitingRelease && m_State != EInputRebindState::WaitingPress)
	{
		return;
	}
	if (!Current.GetRaw().bFocused || (m_Binding.Device == EInputBindingDevice::Gamepad && !Current.GetRaw().Pads[m_Binding.PadSlot].bConnected))
	{
		m_State = EInputRebindState::Canceled;
		return;
	}
	// 履歴からの押下だけを採用し、開始入力の保持と同時入力を別に扱う。
	Toolbox::uint32 DownCount = 0;
	Toolbox::uint32 PressCount = 0;
	// 音量項目の区切り。
	Toolbox::size_t Last = 0;
	// 対象装置の判定するボタン総数。
	const Toolbox::size_t Count = m_Binding.Device == EInputBindingDevice::Keyboard ? static_cast<Toolbox::size_t>(EKey::Count) : m_Binding.Device == EInputBindingDevice::Mouse ? static_cast<Toolbox::size_t>(EMouseButton::Count)
	                                                                                                                                                                             : 16;
	for (Toolbox::size_t Index = 0; Index < Count; ++Index)
	{
		// 対象ボタンの現在の保持。
		const bool Down = m_Binding.Device == EInputBindingDevice::Keyboard ? Current.IsDown(static_cast<EKey>(Index)) : m_Binding.Device == EInputBindingDevice::Mouse ? Current.IsMouseDown(static_cast<EMouseButton>(Index))
		                                                                                                                                                                : Current.IsPadDown(m_Binding.PadSlot, Index);
		// 履歴から得た新しい押下。
		const bool Pressed = m_Binding.Device == EInputBindingDevice::Keyboard ? Current.WasPressed(static_cast<EKey>(Index)) : m_Binding.Device == EInputBindingDevice::Mouse ? Current.WasMousePressed(static_cast<EMouseButton>(Index))
		                                                                                                                                                                       : Current.WasPadPressed(m_Binding.PadSlot, Index);
		DownCount += Down ? 1U : 0U;
		PressCount += Pressed ? 1U : 0U;
		if (Pressed)
		{
			Last = Index;
		}
	}
	if (m_State == EInputRebindState::WaitingRelease)
	{
		if (DownCount == 0)
		{
			m_State = EInputRebindState::WaitingPress;
		}
		return;
	}
	if (PressCount == 0)
	{
		return;
	}
	if (DownCount != 1 || PressCount != 1)
	{
		m_State = EInputRebindState::Canceled;
		return;
	}
	if (m_Binding.Device == EInputBindingDevice::Keyboard)
	{
		m_Binding.Key = static_cast<EKey>(Last);
	}
	else if (m_Binding.Device == EInputBindingDevice::Mouse)
	{
		m_Binding.MouseButton = static_cast<EMouseButton>(Last);
	}
	else
	{
		m_Binding.PadButton = static_cast<Toolbox::uint32>(Last);
	}
	m_State = EInputRebindState::Ready;
}
void FInputRebindCapture::Cancel() noexcept
{
	if (m_State == EInputRebindState::WaitingRelease || m_State == EInputRebindState::WaitingPress)
	{
		m_State = EInputRebindState::Canceled;
	}
}
EInputRebindState FInputRebindCapture::GetState() const noexcept
{
	return m_State;
}
bool FInputRebindCapture::TryGetBinding(FInputBindingDescription& Result) const noexcept
{
	if (m_State != EInputRebindState::Ready)
	{
		return false;
	}
	Result = m_Binding;
	return true;
}
} // namespace Dxf
