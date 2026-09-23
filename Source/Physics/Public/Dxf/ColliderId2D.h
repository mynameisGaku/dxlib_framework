// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_COLLIDERID2D_H
#define DXF_COLLIDERID2D_H
#include "Dxf/BodyId2D.h"
namespace Dxf
{
/**
 * 平面コライダーを識別する、世代付きの非所有ハンドル。
 */
struct FColliderId2D
{
	/**
	 * 取り付け先の剛体。
	 */
	FBodyId2D Body;
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
	bool operator==(const FColliderId2D&) const = default;
};
} // namespace Dxf
#endif
