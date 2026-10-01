// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_CONTENTJOINTDEFINITION3D_H
#define DXF_CONTENT_CONTENTJOINTDEFINITION3D_H
#include "Dxf/RigidBody3D.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 型検査と参照解決を終えたJoint。採用する設定はKindで決まる。
 */
struct FContentJointDefinition3D
{
	/**
	 * インスタンス内の論理ID。
	 */
	Toolbox::FString Id;
	/**
	 * 拘束の種類。
	 */
	EJointKind Kind = EJointKind::Distance;
	/**
	 * 解決済みA側の部品index。
	 */
	Toolbox::uint32 BodyA = 0;
	/**
	 * 解決済みB側の部品index。
	 */
	Toolbox::uint32 BodyB = 0;
	/**
	 * 初期接続を要求するか。
	 */
	bool bConnect = true;
	/**
	 * 距離の設定。
	 */
	FDistanceJointDescription3D Distance;
	/**
	 * 回転の設定。
	 */
	FRevoluteJointDescription3D Revolute;
	/**
	 * 固定の設定。
	 */
	FFixedJointDescription3D Fixed;
	/**
	 * 直動の設定。
	 */
	FPrismaticJointDescription3D Prismatic;
};
} // namespace Dxf
#endif
