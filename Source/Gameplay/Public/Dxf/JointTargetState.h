// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_JOINTTARGETSTATE_H
#define DXF_JOINTTARGETSTATE_H
namespace Dxf
{
/**
 * 目標への要求状態。障害物で停止しただけではReachedにならない。
 */
enum class EJointTargetState
{
	/**
	 * 目標へ向かう有限Motorを要求中。
	 */
	Driving,
	/**
	 * 目標付近で停止待ち、または最大速度0で待機中。
	 */
	Stopping,
	/**
	 * 位置・速度が両方許容内。
	 */
	Reached
};
} // namespace Dxf
#endif
