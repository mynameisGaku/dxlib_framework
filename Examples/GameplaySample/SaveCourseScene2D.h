// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_SAVE_COURSE_SCENE_2D_H
#define DXF_SAMPLE_SAVE_COURSE_SCENE_2D_H
#include "SaveCourseController.h"
#include "Dxf/CheckpointScene2D.h"
namespace Dxf::GameplaySample
{
/**
 * データ由来のゲームScene。定義の描画と所有は既存基盤、進行判断は子Controllerへ委譲する。
 */
class ASaveCourseScene2D final : public ACheckpointScene2D
{
public:
	/**
	 * 次元別Componentだけを渡し、参照と寿命の共通処理を使う。
	 */
	using FController = TSaveCourseController<ACheckpointScene2D, DRigidBody2DComponent, DPrefabInstance2D, FPreparedPrefab2D, FCheckpointInstance2D, DPrismaticJoint2DComponent, DContactListener2DComponent>;
	/**
	 * @param Prepared 信頼する再開計画から準備したScene。
	 * @param Checkpoint ゲームレシピを通した進行。取得済み値をイベント前に保持する。
	 * @param Cargo 許可された動的荷物の準備値。
	 * @param Settings ゲーム進行とは独立した現在の設定。
	 */
	ASaveCourseScene2D(FPreparedScene2D Prepared, FCheckpointDocument2D Checkpoint, FPreparedPrefab2D Cargo, FUserSettings Settings = FSaveCourseInput::Defaults());
	/**
	 * Sceneの子をmemberの解放より先に終了する。
	 */
	~ASaveCourseScene2D() override;
	/**
	 * 未初期化では例外。現在のゲーム値と最後に採取した値を区別する。
	 */
	const FSaveCourseProgress& GetProgress() const;
	/**
	 * @param Settings 全体を検証した設定候補。不正入力では現在値を保持する。
	 * @param Current UIで消費する前の元Snapshot。新割当の押し続けを抑止する。
	 */
	void ApplySettings(FUserSettings Settings, const FInputSnapshot& Current);
	/**
	 * @param Door doorAは0、doorBは1。未解錠・未初期化はfalse。
	 * @param bOpen 有限Motorで向かう開閉目標。
	 */
	bool RequestDoor(Toolbox::uint32 Door, bool bOpen) noexcept;
	/**
	 * 次の固定更新で荷物を一個だけ生成する。未初期化では例外。
	 */
	void RequestCargo();

protected:
	/**
	 * @param Context 現在のAssetService。親の初期化トランザクションへ子を参加させる。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override;
	/**
	 * @param Context 元の入力。UI消費の後、物理計画の前にActionを一度更新する。
	 */
	const FInputSnapshot* RouteInput_Internal(const FTickContext& Context) override;
	/**
	 * @param Context 全PostPhysicsの成功境界。ゲーム進行を親の採取より先に反映する。
	 */
	void OnCheckpointStepCompleted(const FFixedTickContext& Context) override;

private:
	/**
	 * 所有側の準備済み荷物。独自のNative所有器を作らない。
	 */
	FPreparedPrefab2D m_Cargo;
	/**
	 * 今回のSceneへ適用した独立設定。
	 */
	FUserSettings m_Settings;
	/**
	 * 既存Collectionが所有するControllerへの世代付き非所有参照。
	 */
	TObjectHandle<FController> m_Controller;
};
} // namespace Dxf::GameplaySample
#endif
