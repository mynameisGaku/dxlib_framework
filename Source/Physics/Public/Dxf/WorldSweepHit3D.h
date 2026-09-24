// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_SWEEP_HIT_3D_H
#define DXF_WORLD_SWEEP_HIT_3D_H
#include "Dxf/ColliderId3D.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 球を直線移動させたときの、問い合わせ時点の最初の接触。後のWorld変更に対するIDの生存保証はない。
 * 法線・接触点・侵入量は持たない。
 */
struct FWorldSweepHit3D
{
	/**
	 * World/Body/Colliderの世代を含む非所有ID。
	 */
	FColliderId3D Collider;
	/**
	 * 移動区間全体を1とした最初の接触割合（0～1）。秒数・距離ではない。
	 */
	Toolbox::f64 Fraction = 0;
	/**
	 * その割合での問い合わせ球の中心。接触表面の点ではない。f64の凸結合からf32へ戻した有限値。
	 */
	Toolbox::FVector3 CenterAtHit;
	/**
	 * 開始状態で接触または重なりがあったか（Fraction=0）。厳密な貫通だけを示すものではない。
	 */
	bool bInitialContact = false;
};
} // namespace Dxf
#endif
