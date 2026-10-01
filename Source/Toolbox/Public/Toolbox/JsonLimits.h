// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_JSON_LIMITS_H
#define TOOLBOX_JSON_LIMITS_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 読み取り前と確保前に検査する有限のJSON上限。
 */
struct FJsonLimits
{
	/**
	 * 入力バイト数の上限。
	 */
	size_t MaxBytes = 4 * 1024 * 1024;
	/**
	 * 入れ子の上限。最上位を1と数える。
	 */
	uint32 MaxDepth = 64;
	/**
	 * 復号後の一文字列のバイト上限。
	 */
	size_t MaxStringBytes = 64 * 1024;
	/**
	 * 空配列なども含む値の総数上限。
	 */
	uint32 MaxValues = 1024 * 1024;
};
} // namespace Toolbox
#endif
