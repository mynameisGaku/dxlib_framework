// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Crc32.h"
namespace Toolbox
{
uint32 FCrc32::Compute(FStringView Bytes) noexcept
{
	// 反転表現のCRC初期値。
	uint32 State = 0xffffffffU;
	for (size_t I = 0; I < Bytes.Size(); ++I)
	{
		State ^= static_cast<uint8>(Bytes[I]);
		// テーブル確保を伴わない8ビットの多項式更新。
		for (uint32 Bit = 0; Bit < 8; ++Bit)
		{
			State = (State >> 1) ^ ((State & 1) != 0 ? 0xedb88320U : 0U);
		}
	}
	return State ^ 0xffffffffU;
}
} // namespace Toolbox
