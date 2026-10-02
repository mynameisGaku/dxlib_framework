// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CRC32_H
#define TOOLBOX_CRC32_H
#include "Toolbox/String.h"
namespace Toolbox
{
/**
 * 保存バイトの破損を検出するCRC-32/ISO-HDLC。署名・改ざん防止ではない。
 */
class FCrc32
{
public:
	/**
	 * @param Bytes 実際に書き込む全バイト。空入力の結果は0。
	 */
	static uint32 Compute(FStringView Bytes) noexcept;
};
} // namespace Toolbox
#endif
