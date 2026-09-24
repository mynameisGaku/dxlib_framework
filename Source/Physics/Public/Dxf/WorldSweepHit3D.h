// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_SWEEP_HIT_3D_H
#define DXF_WORLD_SWEEP_HIT_3D_H
#include "Dxf/ColliderId3D.h"
#include "Toolbox/Optional.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 球を直線移動させたときの、問い合わせ時点の最初の接触。後のWorld変更に対するIDの生存保証はない。
 * 取得できた場合は接触法線を持つ。接触点・侵入量は持たない。
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
	/**
	 * 接触対象から問い合わせ球の中心へ向く、World座標の無次元の単位方向（有限、f32への丸めの範囲で長さ1）。
	 * 移動量・距離・押し戻し量ではない。初期接触、問い合わせ半径0、有効な方向を丸め誤差から区別できない場合は空。
	 * 空でも衝突は成立している（非交差は外側のOptionalが空）。
	 */
	Toolbox::TOptional<Toolbox::FVector3> Normal;
};
} // namespace Dxf
#endif
