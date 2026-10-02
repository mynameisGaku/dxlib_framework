// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_RESUME_PLAN_3D_H
#define DXF_CHECKPOINT_RESUME_PLAN_3D_H
#include "Dxf/SceneDefinition3D.h"
#include "Dxf/CheckpointJointState.h"
namespace Dxf
{
/**
 * 旧Sceneを変更せず完成する再開用CPU値。準備完了とゲーム再開完了は別。
 */
struct FCheckpointResumePlan3D
{
	/**
	 * 既存PrepareSceneへ渡す新しい初期構成。資源取込は所有側で行う。
	 */
	FSceneDefinition3D Scene;
	/**
	 * Ready後に公開Jointへ渡す任意の目標操作。ここではまだ操作しない。
	 */
	Toolbox::TVector<FCheckpointJointState> Targets;
	/**
	 * イベント消費前にゲーム側が採用する検証済み進行Object。
	 */
	Toolbox::FString GamePayload;
};
} // namespace Dxf
#endif
