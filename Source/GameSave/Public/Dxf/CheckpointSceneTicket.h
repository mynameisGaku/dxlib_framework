// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_SCENE_TICKET_H
#define DXF_CHECKPOINT_SCENE_TICKET_H
#include "Dxf/CheckpointSceneState.h"
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
namespace SavePrivate
{
struct FCheckpointSceneStatus;
}
/**
 * Sceneポインタを持たず、元Sceneの完了と終了だけを所有側で読む値。
 */
class FCheckpointSceneTicket
{
public:
	/**
	 * 空の観察はEnded。新しいSceneを探し直さない。
	 */
	FCheckpointSceneTicket() = default;
	/**
	 * @param Status Sceneだけが更新するCPU値。実行時BodyやNative資源を含めない。
	 */
	explicit FCheckpointSceneTicket(Toolbox::TSharedPtr<SavePrivate::FCheckpointSceneStatus> Status);
	/**
	 * 所有側の通常Pollで読む。Workerからの利用はしない。
	 */
	ECheckpointSceneState GetState() const noexcept;

private:
	/**
	 * Scene終了後にも終端を保持する小さいCPU値。
	 */
	Toolbox::TSharedPtr<SavePrivate::FCheckpointSceneStatus> m_Status;
};
} // namespace Dxf
#endif
