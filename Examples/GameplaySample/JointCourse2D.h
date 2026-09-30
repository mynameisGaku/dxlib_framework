// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_JOINT_COURSE_2D_H
#define DXF_SAMPLE_JOINT_COURSE_2D_H
#include "Dxf/DistanceJointComponent2D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Toolbox/Array.h"
namespace Dxf::GameplaySample
{
/**
 * 既存接触コースの距離拘束の仕掛け。Sceneの子の寿命は公開Spawn/Destroyへ任せる。
 * 入力と接続要求を扱い、描画はJointCourseOverlayへ分ける。
 */
class FJointCourse2D
{
public:
	/**
	 * 吊り下げ・四連結・移動支点を配置する。
	 * @param Scene この仕掛けを所有する既存PhysicsScene。
	 */
	void Initialize(DPhysicsScene2D& Scene);
	/**
	 * Pause中は操作を拒否する。J力積・K解除・L再接続・N再生成・B支点停止切替。
	 * @param Input UIで処理済みのゲーム入力。
	 * @param bPaused Sceneの一時停止状態。
	 */
	void HandleInput(const FInputSnapshot& Input, bool bPaused);
	/**
	 * 古いObjectを破棄要求して、新世代の仕掛けを配置する。
	 */
	void Regenerate();
	/**
	 * 指定の端または中間Bodyだけを破棄する。暗黙の再接続はしない。
	 * @param Index Body配列の位置。範囲外は拒否する。
	 */
	void RemoveBody(Toolbox::size_t Index);
	/**
	 * 次の固定更新で重りを動かす力積を予約する。
	 */
	void Kick();
	/**
	 * 吊り下げ重りの接続解除を要求する。
	 */
	void Disconnect();
	/**
	 * 吊り下げ重りの明示的再接続を要求する。
	 */
	void Reconnect();
	/**
	 * 既存KinematicMoverの経路を有効化または停止する。
	 * @param bMoving 動く支点にするか。
	 */
	void SetCarrierMoving(bool bMoving);
	/**
	 * 各仕掛けの世代付き非所有参照。描画・試験で同じ個体を使う。
	 */
	const Toolbox::TArray<TObjectHandle<DRigidBody2DComponent>, 9>& GetBodies() const noexcept;
	/**
	 * 吊り下げ・四連結・移動支点の六つの拘束Component。
	 */
	const Toolbox::TArray<TObjectHandle<DDistanceJoint2DComponent>, 6>& GetJoints() const noexcept;
	/**
	 * 動く支点。未初期化・破棄後はnullptr。
	 */
	DKinematicMover2DComponent* GetCarrier() const noexcept;

private:
	/**
	 * 生成する既存Scene。子の終了まで有効。
	 */
	DPhysicsScene2D* m_pScene = nullptr;
	/**
	 * Bodyを所有するSceneの子。再生成はこのObjectへ破棄要求する。
	 */
	Toolbox::TArray<TObjectHandle<DGameObject>, 9> m_Objects;
	/**
	 * 剛体の非所有参照。7番はMoverのため空。
	 */
	Toolbox::TArray<TObjectHandle<DRigidBody2DComponent>, 9> m_Bodies;
	/**
	 * 接続の非所有参照。
	 */
	Toolbox::TArray<TObjectHandle<DDistanceJoint2DComponent>, 6> m_Joints;
	/**
	 * 7番Objectにある既存の動く支点。
	 */
	TObjectHandle<DKinematicMover2DComponent> m_Carrier;
	/**
	 * 支点の移動を有効にしたか。
	 */
	bool m_bCarrierMoving = true;
};
} // namespace Dxf::GameplaySample
#endif
