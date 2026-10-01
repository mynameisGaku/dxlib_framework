// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_LINEARJOINTTARGETCOMMAND_H
#define DXF_LINEARJOINTTARGETCOMMAND_H
#include "Dxf/LinearJointDrive.h"
#include "Dxf/JointTargetState.h"
namespace Dxf
{
/**
 * Componentへ渡す有限Motor要求と、位置・速度に基づく到達判定。
 */
struct FLinearJointTargetCommand
{
	/**
	 * 次のPrePhysicsへ渡す値。停止待ちでは速度0の有限ブレーキ。
	 */
	FLinearJointDrive Drive;
	/**
	 * 目標への要求状態。World全体の状態ではない。
	 */
	EJointTargetState State = EJointTargetState::Driving;
};
} // namespace Dxf
#endif
