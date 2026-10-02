// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVELIMITS_H
#define DXF_SAVELIMITS_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 保存の確保前に適用する有限上限。ゲーム固有の個体・状態上限は上位codecが加える。
 */
struct FSaveLimits
{
	/**
	 * 一文書の最大バイト数。
	 */
	Toolbox::size_t MaxBytes = 8 * 1024 * 1024;
	/**
	 * 最上位を1と数えるJSON深度。
	 */
	Toolbox::uint32 MaxDepth = 64;
	/**
	 * 復号後の一文字列の上限。
	 */
	Toolbox::size_t MaxStringBytes = 64 * 1024;
	/**
	 * 全JSON値の上限。
	 */
	Toolbox::uint32 MaxValues = 128 * 1024;
	/**
	 * 論理キーの最大バイト数。
	 */
	Toolbox::size_t MaxKeyBytes = 512;
};
} // namespace Dxf
#endif
