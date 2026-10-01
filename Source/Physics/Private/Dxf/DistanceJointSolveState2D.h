// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_DISTANCE_JOINT_SOLVE_STATE_2D_H
#define DXF_DISTANCE_JOINT_SOLVE_STATE_2D_H
#include "Toolbox/Vector2.h"
#include "MechanismConstraintMath.h"
namespace Dxf::PhysicsPrivate
{
/**
 * 一回のStepで求解する距離拘束の再利用値。全分割で共有し、成功時だけ登録値へ戻す。
 */
struct FDistanceJointSolveState2D
{
	/**
	 * Step開始時に登録中だったか。
	 */
	bool bActive = false;
	/**
	 * 作業開始時の種類と新種類の履歴。世代とともに確定先を照合する。
	 */
	EJointKind Kind = EJointKind::Distance;
	FMechanismCache Mechanism;
	/**
	 * 作業開始時の世代。別の登録への書き戻しを防ぐ。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 今回の分割までに求めた速度補正の合計。
	 */
	Toolbox::f64 AccumulatedImpulse = 0;
	/**
	 * 距離が縮退した際に使う最後の確定方向。
	 */
	Toolbox::FVector2 LastValidAxis{1, 0};
	/**
	 * 保存方向が確定済みか。
	 */
	bool bHasLastValidAxis = false;
};
} // namespace Dxf::PhysicsPrivate
#endif
