// SPDX-License-Identifier: NOASSERTION
#include "Dxf/InputBindingDescription.h"
namespace Dxf
{
bool FInputBindingDescription::operator==(const FInputBindingDescription& Other) const noexcept
{
	// 使用する装置の入力だけを比較し、未使用のfieldを割当識別へ混ぜない。
	if (Device != Other.Device)
	{
		return false;
	}
	switch (Device)
	{
	case EInputBindingDevice::Keyboard:
		return Key == Other.Key;
	case EInputBindingDevice::Mouse:
		return MouseButton == Other.MouseButton;
	case EInputBindingDevice::Gamepad:
		return PadSlot == Other.PadSlot && PadButton == Other.PadButton;
	}
	return false;
}
} // namespace Dxf
