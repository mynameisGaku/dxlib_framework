#pragma once
#include <cstdint>
#include <string_view>

namespace Dxf
{
/** Validates Unicode scalar values without allocating or relying on the system locale. */
inline bool IsValidUtf8(std::string_view Text) noexcept
{
	std::size_t Index = 0;
	while (Index < Text.size())
	{
		const auto Lead = static_cast<unsigned char>(Text[Index++]);
		if (Lead < 0x80)
		{
			continue;
		}
		std::uint32_t Value = 0;
		std::uint32_t Minimum = 0;
		std::size_t Remaining = 0;
		if (Lead >= 0xC2 && Lead <= 0xDF)
		{
			Value = Lead & 0x1F;
			Minimum = 0x80;
			Remaining = 1;
		}
		else if (Lead >= 0xE0 && Lead <= 0xEF)
		{
			Value = Lead & 0x0F;
			Minimum = 0x800;
			Remaining = 2;
		}
		else if (Lead >= 0xF0 && Lead <= 0xF4)
		{
			Value = Lead & 0x07;
			Minimum = 0x10000;
			Remaining = 3;
		}
		else
		{
			return false;
		}
		if (Remaining > Text.size() - Index)
		{
			return false;
		}
		for (std::size_t Count = 0; Count < Remaining; ++Count)
		{
			const auto Byte = static_cast<unsigned char>(Text[Index++]);
			if ((Byte & 0xC0) != 0x80)
			{
				return false;
			}
			Value = (Value << 6) | (Byte & 0x3F);
		}
		if (Value < Minimum || Value > 0x10FFFF || (Value >= 0xD800 && Value <= 0xDFFF))
		{
			return false;
		}
	}
	return true;
}

namespace Detail
{
inline bool IsValidNativeString_Internal(std::string_view Text, bool bAllowEmpty = false) noexcept
{
	return (bAllowEmpty || !Text.empty()) && Text.find('\0') == std::string_view::npos && IsValidUtf8(Text);
}
}
}
