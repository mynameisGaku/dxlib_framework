// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_JOINT_STATE_H
#define DXF_CHECKPOINT_JOINT_STATE_H
#include "Dxf/JointKind.h"
#include "Dxf/AngularJointDrive.h"
#include "Dxf/AngularJointLimits.h"
#include "Dxf/LinearJointDrive.h"
#include "Dxf/LinearJointLimits.h"
#include "Dxf/AngularJointTargetSettings.h"
#include "Dxf/LinearJointTargetSettings.h"
#include "Toolbox/String.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * 両次元で同じ意味のJoint論理状態。実行時ID、Frame、Impulseを持たない。
 */
struct FCheckpointJointState
{
	/**
	 * Prefab個体キー。空ならSceneの公開接続を指す。
	 */
	Toolbox::FString Instance;
	/**
	 * 個体の型付きexport名、またはSceneの接続ID。
	 */
	Toolbox::FString Export;
	/**
	 * 公開先と一致しなければ再開前に拒否する種類。
	 */
	EJointKind Kind = EJointKind::Distance;
	/**
	 * 再生成後に接続を要求するか。解除も明示的な状態として保持する。
	 */
	bool bConnected = true;
	/**
	 * Distanceだけの設定長。未指定なら信頼するContentの初期値。
	 */
	Toolbox::TOptional<Toolbox::f64> DistanceLength;
	/**
	 * Revoluteだけの有限速度指示。
	 */
	FAngularJointDrive AngularDrive;
	/**
	 * Revoluteだけの主値区間。
	 */
	FAngularJointLimits AngularLimits;
	/**
	 * Revoluteの任意の目標操作。速度指示とは別のゲーム要求。
	 */
	Toolbox::TOptional<FAngularJointTargetSettings> AngularTarget;
	/**
	 * Prismaticだけの有限速度指示。
	 */
	FLinearJointDrive LinearDrive;
	/**
	 * Prismaticだけの移動区間。
	 */
	FLinearJointLimits LinearLimits;
	/**
	 * Prismaticの任意の目標操作。
	 */
	Toolbox::TOptional<FLinearJointTargetSettings> LinearTarget;
};
} // namespace Dxf
#endif
