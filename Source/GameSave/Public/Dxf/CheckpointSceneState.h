// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_SCENE_STATE_H
#define DXF_CHECKPOINT_SCENE_STATE_H
namespace Dxf
{
/**
 * Sceneの受付・有効化・成功物理更新・終了を分ける所有側の観察値。
 */
enum class ECheckpointSceneState
{
	/**
	 * Navigatorの初期化と有効化を待つ。
	 */
	Pending,
	/**
	 * 有効化済みだが必須Body・Jointの成功観察を待つ。
	 */
	PendingPhysics,
	/**
	 * 全必須構成と成功PostPhysicsを確認した。
	 */
	Ready,
	/**
	 * 候補の拒否またはScene終了。新世代へ追従しない。
	 */
	Ended
};
} // namespace Dxf
#endif
