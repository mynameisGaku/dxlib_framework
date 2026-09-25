// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_RECOVERY_3D_H
#define DXF_CHARACTER_RECOVERY_3D_H
#include "Dxf/CharacterRecoveryStatus.h"
#include "Dxf/ColliderId3D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 初期重なりの解消の結果。Worldは変更しない。
 */
struct FCharacterRecovery3D
{
	/**
	 * 解消後の中心。解消できなかった場合は元の中心（途中の補正は採用しない）。
	 */
	Toolbox::FVector3 Center;
	/**
	 * 解消の結果。
	 */
	ECharacterRecoveryStatus Status = ECharacterRecoveryStatus::NoOverlap;
	/**
	 * 行った補正の回数。
	 */
	Toolbox::int32 Iterations = 0;
	/**
	 * 採用した補正の合計距離（解消できなかった場合は0）。
	 */
	Toolbox::f64 Distance = 0;
	/**
	 * 最後に扱った重なりの相手、または解消を妨げたCollider。
	 */
	Toolbox::TOptional<FColliderId3D> Collider;
	/**
	 * 行ったWorld問い合わせの回数。
	 */
	Toolbox::int32 Queries = 0;
};
} // namespace Dxf
#endif
