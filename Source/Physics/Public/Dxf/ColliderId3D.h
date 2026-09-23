// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_COLLIDERID3D_H
#define DXF_COLLIDERID3D_H
#include "Dxf/BodyId3D.h"
namespace Dxf
{
/**
 * 立体コライダーを識別する、世代付きの非所有ハンドル。
 */
struct FColliderId3D
{
	/**
	 * 取り付け先の剛体。
	 */
	FBodyId3D Body;
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
	bool operator==(const FColliderId3D&) const = default;
};
} // namespace Dxf
#endif
