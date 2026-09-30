// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_JOINT_CONSUMER_RESULT_H
#define DXF_PACKAGE_JOINT_CONSUMER_RESULT_H
#include "Toolbox/Vector3.h"
/**
 * 配布先のJoint試験で、Sceneより長く保持する観察値。
 */
struct FJointConsumerResult
{
	/**
	 * 解除・再接続・Body世代入替をすべて確認したか。
	 */
	bool bComplete = false;
	/**
	 * Scene終了時にJoint Component自身の終了を確認したか。
	 */
	bool bShutdown = false;
	/**
	 * Nativeで表示した荷物の補間位置（2DはZ=0）。
	 */
	Toolbox::FVector3 RenderWeight;
};
#endif
