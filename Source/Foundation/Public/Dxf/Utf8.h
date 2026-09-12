#pragma once
#include "Toolbox/Utility.h"
#include "Toolbox/String.h"

namespace Dxf
{
/**
 * メモリ確保やシステムロケールに依存せずUnicodeスカラー値を検証する。
 * @param Text 検証するUTF-8文字列。
 */
inline bool IsValidUtf8(Toolbox::FStringView Text) noexcept
{
	/**
	 * 要素の位置。
	 */
	Toolbox::size_t Index = 0;
	while (Index < Text.Size())
	{
		/**
		 * UTF-8シーケンスの先頭バイト。
		 */
		const auto Lead = static_cast<unsigned char>(Text[Index++]);
		if (Lead < 0x80)
		{
			continue;
		}
		/**
		 * 復号中のUnicodeコードポイント。
		 */
		Toolbox::uint32 Value = 0;
		/**
		 * 符号化長に対する最小コードポイント。
		 */
		Toolbox::uint32 Minimum = 0;
		/**
		 * 読み取りが残っているバイト数。
		 */
		Toolbox::size_t Remaining = 0;
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
		if (Remaining > Text.Size() - Index)
		{
			return false;
		}
		/**
		 * 要素の位置を進めて順に処理する。
		 */
		for (Toolbox::size_t Count = 0; Count < Remaining; ++Count)
		{
			/**
			 * 現在検証するバイト。
			 */
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
/**
 * ネイティブAPIに渡せるUTF-8文字列かを検証する。
 * @param Text 検証するUTF-8文字列。
 * @param bAllowEmpty 空文字列を有効として扱うか。
 */
inline bool IsValidNativeString_Internal(Toolbox::FStringView Text, bool bAllowEmpty = false) noexcept
{
	return (bAllowEmpty || !Text.IsEmpty()) && Text.Find('\0') == Toolbox::FStringView::NotFound && IsValidUtf8(Text);
}
} // namespace Detail
} // namespace Dxf
