// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAMPLE_MECHANISM_COURSE_3D_H
#define DXF_SAMPLE_MECHANISM_COURSE_3D_H
#include "MechanismController.h"
#include "Dxf/RevoluteJointComponent3D.h"
#include "Dxf/FixedJointComponent3D.h"
#include "Dxf/PrismaticJointComponent3D.h"
#include "Dxf/DistanceJointComponent3D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Toolbox/Array.h"
namespace Dxf::GameplaySample
{
/**
 * 手動扉・電動扉・スライド・動く支点の昇降機・固定荷物を既存Sceneへ置く。
 * 子の寿命はScene、Jointの接続は既存Componentが管理する。
 */
class FMechanismCourse3D
{
public:
	/**
	 * 生成先のSceneへ接続し、操作用の固定更新Objectを一つだけ置く。
	 */
	void Initialize(DPhysicsScene3D& Scene);
	/**
	 * 古い装置を破棄要求し、Scene内に新世代の装置を置く。
	 */
	void Regenerate();
	/**
	 * 固定更新から一度だけ呼ぶ。描画やUIから呼ばない。
	 */
	void FixedUpdate(Toolbox::f64 Seconds);
	/**
	 * 次の固定更新へ渡す操作意図。UIはこの値だけを変更する。
	 */
	FMechanismController& GetController() noexcept
	{
		return m_Controller;
	}
	/**
	 * 読み取り専用の操作意図。
	 */
	const FMechanismController& GetController() const noexcept
	{
		return m_Controller;
	}
	/**
	 * 補間描画と試験で共有する剛体の非所有参照。
	 */
	const Toolbox::TArray<TObjectHandle<DRigidBody3DComponent>, 12>& GetBodies() const noexcept
	{
		return m_Bodies;
	}
	/**
	 * 手動と電動の回転Joint。
	 */
	const Toolbox::TArray<TObjectHandle<DRevoluteJoint3DComponent>, 2>& GetRevolutes() const noexcept
	{
		return m_Revolutes;
	}
	/**
	 * スライドと昇降の直動Joint。
	 */
	const Toolbox::TArray<TObjectHandle<DPrismaticJoint3DComponent>, 2>& GetPrismatics() const noexcept
	{
		return m_Prismatics;
	}
	/**
	 * 独立荷物と昇降機の荷物の固定Joint。
	 */
	const Toolbox::TArray<TObjectHandle<DFixedJoint3DComponent>, 2>& GetFixed() const noexcept
	{
		return m_Fixed;
	}
	/**
	 * 固定更新の実行数。表示数とは無関係。
	 */
	Toolbox::uint64 GetUpdateCount() const noexcept
	{
		return m_Updates;
	}

private:
	/**
	 * 子を所有するScene。
	 */
	DPhysicsScene3D* m_pScene = nullptr;
	/**
	 * 操作意図だけを持つ非所有の制御。
	 */
	FMechanismController m_Controller;
	/**
	 * 再生成時に破棄要求するSceneの子。
	 */
	Toolbox::TArray<TObjectHandle<DGameObject>, 12> m_Objects;
	/**
	 * 剛体の非所有参照。6番はKinematicMoverのため空。
	 */
	Toolbox::TArray<TObjectHandle<DRigidBody3DComponent>, 12> m_Bodies;
	/**
	 * 回転接続の非所有参照。
	 */
	Toolbox::TArray<TObjectHandle<DRevoluteJoint3DComponent>, 2> m_Revolutes;
	/**
	 * 直動接続の非所有参照。
	 */
	Toolbox::TArray<TObjectHandle<DPrismaticJoint3DComponent>, 2> m_Prismatics;
	/**
	 * 固定接続の非所有参照。
	 */
	Toolbox::TArray<TObjectHandle<DFixedJoint3DComponent>, 2> m_Fixed;
	/**
	 * 昇降機の動く支点。
	 */
	TObjectHandle<DKinematicMover3DComponent> m_Carrier;
	/**
	 * 反映済み接続意図。再生成時は接続済みへ戻す。
	 */
	bool m_Connected[5] = {true, true, true, true, true};
	/**
	 * 固定更新の通し番号。
	 */
	Toolbox::uint64 m_Updates = 0;
};
} // namespace Dxf::GameplaySample
#endif
