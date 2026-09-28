// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_JOINTID3D_H
#define DXF_PHYSICS_JOINTID3D_H
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 空間の拘束を識別する、世代付きの非所有ハンドル。
 * BodyとColliderと同じ考え方で、登録先ワールド・スロット・世代を持つ値型である。
 */
struct FJointId3D
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
	bool operator==(const FJointId3D&) const = default;
};
} // namespace Dxf
#endif
