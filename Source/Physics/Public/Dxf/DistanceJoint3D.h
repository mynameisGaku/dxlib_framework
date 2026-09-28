// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_DISTANCEJOINT3D_H
#define DXF_PHYSICS_DISTANCEJOINT3D_H
#include "Dxf/BodyId3D.h"
#include "Toolbox/Vector3.h"
namespace Dxf
{
/**
 * 2つの空間BodyのLocal Anchor間の距離を維持する拘束の生成条件。
 * BodyはDescriptionの外で渡す（WorldのCreateDistanceJoint）。
 */
struct FDistanceJointDescription3D
{
	/**
	 * 維持するAnchor間距離（メートル）。有限な0以上。
	 */
	Toolbox::f64 Length = 0;
	/**
	 * BodyA側のLocal Anchor（Bodyの重心基準）。
	 */
	Toolbox::FVector3 LocalAnchorA;
	/**
	 * BodyB側のLocal Anchor（Bodyの重心基準）。
	 */
	Toolbox::FVector3 LocalAnchorB;
};
/**
 * 距離拘束の現在の状態の読み取り。WorldのGetDistanceJointで得る。
 */
struct FDistanceJointState3D
{
	/**
	 * 現在のAnchor間距離。
	 */
	Toolbox::f64 CurrentLength = 0;
	/**
	 * Lengthとの差。正なら伸び、負なら縮んでいる。
	 */
	Toolbox::f64 Error = 0;
};
} // namespace Dxf
#endif
