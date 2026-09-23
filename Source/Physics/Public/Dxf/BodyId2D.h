// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_BODYID2D_H
#define DXF_BODYID2D_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 平面剛体を識別する、世代付きの非所有ハンドル。
 */
struct FBodyId2D
{
	/**
	 * 登録先ワールドの識別子。
	 */
	Toolbox::uint64 World = 0;
	/**
	 * 登録スロットの番号。
	 */
	Toolbox::size_t Index = 0;
	/**
	 * 同じスロットを再使用した際の世代。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 同じ登録を指すか調べる。
	 */
	bool operator==(const FBodyId2D&) const = default;
};
} // namespace Dxf
#endif
