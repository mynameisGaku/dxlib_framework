// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_ANGULARJOINTTARGETCOMMAND_H
#define DXF_ANGULARJOINTTARGETCOMMAND_H
#include "Dxf/AngularJointDrive.h"
#include "Dxf/JointTargetState.h"
namespace Dxf
{
/**
 * Componentへ渡す有限Motor要求と、位置・速度に基づく到達判定。
 */
struct FAngularJointTargetCommand
{
	/**
	 * 次のPrePhysicsへ渡す値。停止待ちでは速度0の有限ブレーキ。
	 */
	FAngularJointDrive Drive;
	/**
	 * 目標への要求状態。World全体の状態ではない。
	 */
	EJointTargetState State = EJointTargetState::Driving;
};
} // namespace Dxf
#endif
