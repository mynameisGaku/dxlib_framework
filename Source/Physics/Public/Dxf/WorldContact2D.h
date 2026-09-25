// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_CONTACT_2D_H
#define DXF_WORLD_CONTACT_2D_H
#include "Dxf/ColliderId2D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector2.h"
namespace Dxf
{
/**
 * 問い合わせ円と一つのColliderの、問い合わせ時点の接触・分離。後のWorld変更に対するIDの生存保証はない。
 */
struct FWorldContact2D
{
	/**
	 * World/Body/Colliderの世代を含む非所有ID。
	 */
	FColliderId2D Collider;
	/**
	 * 表面間の符号付き距離（距離単位）。正は隙間、0は境界だけの接触、負は重なり（大きさは法線方向の解消距離）。
	 */
	Toolbox::f64 Separation = 0;
	/**
	 * 分離が最も速く増える単位方向（対象から円の中心へ向く向き）。同心や同じ距離の面が複数ある場合など、
	 * 一つに決められなければ空（接触・重なり自体は成立している）。
	 */
	Toolbox::TOptional<Toolbox::FVector2> Normal;
};
} // namespace Dxf
#endif
