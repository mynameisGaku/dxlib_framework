// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHECKPOINT_SCENE_2D_H
#define DXF_CHECKPOINT_SCENE_2D_H
#include "Dxf/CheckpointCapture2D.h"
#include "Dxf/CheckpointSceneTicket.h"
namespace Dxf
{
namespace SavePrivate
{
class FCheckpointSceneRuntime2D;
}
/**
 * 再開計画から作るContentScene。論理採取と目標操作を持ち、実体所有は既存CollectionとWorld。
 */
class ACheckpointScene2D : public DContentScene2D
{
public:
	/**
	 * @param Prepared 許可カタログから作った再開計画の準備値。
	 * @param Checkpoint 純粋なゲーム変換と再開検証を通した進行。イベント消費前に保持する。
	 * @param Limits 採取と進行JSONにも適用する有限上限。
	 */
	ACheckpointScene2D(FPreparedScene2D Prepared, FCheckpointDocument2D Checkpoint, FCheckpointLimits Limits = {});
	/**
	 * 子と予約を既存終了経路で終了し、CPU観察へEndedを残す。
	 */
	~ACheckpointScene2D() override;
	/**
	 * Sceneポインタを含まない元Sceneの観察値。
	 */
	FCheckpointSceneTicket GetResumeTicket() const noexcept;
	/**
	 * 明示採取・生成・破棄の窓口。終了時にこのSceneが退役する。
	 */
	FCheckpointCapture2D& GetCheckpointCapture() noexcept;
	/**
	 * ゲームcodecが解釈する現在の進行Object。初期化中に音声や得点を発生させない。
	 */
	const Toolbox::FString& GetGamePayload() const noexcept;
	/**
	 * @param Payload ゲームcodecで検証した進行Object。不正JSONでは旧進行を保持する。
	 * 次の成功境界で採取し、Pause中の最後の正常保存は変更しない。
	 */
	void SetGamePayload(Toolbox::FString Payload);

protected:
	/**
	 * 有効化を観察値へ反映する。ゲーム音声は派生OnCheckpointActivatedで明示する。
	 * @param Context 既存Scene有効化の情報。
	 */
	void OnEnter(const FSceneActivationContext& Context) noexcept final;
	/**
	 * @param Context 有効化後のゲーム専用処理。初期化中の副作用と区別する。
	 */
	virtual void OnCheckpointActivated(const FSceneActivationContext& Context) noexcept;
	/**
	 * @param Context 全ゲームPostPhysics反映後。派生側の進行更新を採取より先に行う。
	 */
	virtual void OnCheckpointStepCompleted(const FFixedTickContext& Context);
	/**
	 * @param Context 成功物理更新。追加Stepは呼ばず、必須Readyを確認する。
	 */
	void OnPhysicsStepCompleted(const FFixedTickContext& Context) final;
	/**
	 * 旧Sceneの非所有参照を終了する。確保しない。
	 */
	void OnExit() noexcept final;
	/**
	 * 子の終了前に採取と目標操作を退役する。
	 */
	void OnDeinitialize() noexcept override;

private:
	/**
	 * Sceneだけが所有するCPU値と世代付き参照。
	 */
	Toolbox::TUniquePtr<SavePrivate::FCheckpointSceneRuntime2D> m_pRuntime;
};
} // namespace Dxf
#endif
