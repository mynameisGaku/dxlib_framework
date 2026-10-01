// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_CONTENTPARTDEFINITION2D_H
#define DXF_CONTENT_CONTENTPARTDEFINITION2D_H
#include "Dxf/RigidBody2D.h"
#include "Dxf/ContentVisualDefinition.h"
#include "Toolbox/String.h"
namespace Dxf
{
/**
 * 論理部品の不変の初期条件。生成済みIDを含めない。
 */
struct FContentPartDefinition2D
{
	/**
	 * インスタンス内で一意な論理ID。
	 */
	Toolbox::FString Id;
	/**
	 * 人向けの任意名。論理参照の解決には使わない。
	 */
	Toolbox::FString DisplayName;
	/**
	 * 物理Bodyを持つか。
	 */
	bool bHasBody = false;
	/**
	 * KinematicMoverを使うか。RigidBodyとは排他。
	 */
	bool bMover = false;
	/**
	 * 初期Body条件。表示のみでも初期Poseを使用する。
	 */
	FBodyDescription2D Body;
	/**
	 * 重心基準の形状。
	 */
	Toolbox::TVector<FColliderDescription2D> Colliders;
	/**
	 * 補間Poseを読む表示。
	 */
	FContentVisualDefinition Visual;
};
} // namespace Dxf
#endif
