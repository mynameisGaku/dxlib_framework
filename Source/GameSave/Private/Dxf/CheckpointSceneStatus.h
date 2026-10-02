// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_SCENE_STATUS_PRIVATE_H
#define DXF_CHECKPOINT_SCENE_STATUS_PRIVATE_H
#include "Dxf/CheckpointSceneState.h"
namespace Dxf::SavePrivate
{
// 所有側だけが更新する。SceneやNative資源への参照を持たない。
struct FCheckpointSceneStatus
{
	// 最初の成功固定更新と終了を区別する。
	ECheckpointSceneState State = ECheckpointSceneState::Pending;
};
} // namespace Dxf::SavePrivate
#endif
