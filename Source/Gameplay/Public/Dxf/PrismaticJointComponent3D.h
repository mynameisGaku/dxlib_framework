// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_PRISMATICJOINTCOMPONENT3D_H
#define DXF_GAMEPLAY_PRISMATICJOINTCOMPONENT3D_H
#include "Dxf/PrismaticJointComponentDescription3D.h"
#include "Dxf/PrismaticJointObservation3D.h"
#include "Dxf/JointConnection.h"
#include "Dxf/PrePhysicsStep.h"
#include "Dxf/PostPhysicsStep.h"
namespace Dxf
{
/**
 * 種類別拘束一つの接続要求と寿命を管理する。Bodyは所有せず、描画しない。
 */
class DPrismaticJoint3DComponent : public DGameObjectComponent, private IPrePhysicsStep, private IPostPhysicsStep
{
public:
	/**
	 * @param Description 両端とFrame設定。非有限値・退化した方向は構築時に拒否する。
	 */
	explicit DPrismaticJoint3DComponent(FPrismaticJointComponentDescription3D Description);
	/**
	 * 次の固定更新境界で解除する。同じ境界までの最後の要求を採用する。
	 */
	void RequestDisconnect() noexcept;
	/**
	 * 次の固定更新境界で接続する。同じ有効な接続の再要求はIDを維持する。
	 * @param Description 新しい両端と設定。不正数値は要求時に拒否する。
	 */
	void RequestConnect(FPrismaticJointComponentDescription3D Description);
	/**
	 * 接続意図に属するDriveを更新する。解除後は再接続しない。
	 * @param Drive 有限の目標速度と最大駆動力。不正値は旧要求を保持して拒否する。
	 */
	void RequestDrive(FLinearJointDrive Drive);
	/**
	 * 接続意図に属するLimitを更新する。後のConnectは設定全体を置き換える。
	 * @param Limits 有効区間。不正値は旧要求を保持して拒否する。
	 */
	void RequestLimits(FLinearJointLimits Limits);
	/**
	 * 破棄要求やWorld側の失効も調べる。生成・起床・Stepは行わない。
	 */
	EJointConnection GetConnectionState() const noexcept;
	/**
	 * 現在生存する接続ID。未接続・失効時は空。
	 */
	Toolbox::TOptional<FJointId3D> GetJointId() const noexcept;
	/**
	 * 最後の成功PostPhysics値。次のStep開始・解除・失効後は空。
	 */
	Toolbox::TOptional<FPrismaticJointObservation3D> GetObservation() const noexcept;
	/**
	 * 現在の接続設定。保留中の要求とは区別する。
	 */
	const FPrismaticJointComponentDescription3D& GetDescription() const noexcept;
	/**
	 * 表示Bodyと同じ補間姿勢の両Anchorを得る。失効時はfalse。
	 * @param A A側の出力。
	 * @param B B側の出力。
	 */
	bool GetRenderAnchors(Toolbox::FVector3& A, Toolbox::FVector3& B) const;

protected:
	/**
	 * 同じ固定更新のPre/PostPhysicsへ一度予約する。
	 * @param Context 固定更新の実行環境。PhysicsScene以外は拒否する。
	 */
	void OnFixedTick(const FFixedTickContext& Context) override;
	/**
	 * 生存Jointを破棄し、予約と参照を終了する。
	 */
	void OnDeinitialize() noexcept override;

private:
	/**
	 * 両端の登録後に最後の要求を反映する。
	 * @param Context 同じ固定更新の環境。
	 */
	void OnPrePhysicsStep_Internal(const FFixedTickContext& Context) override;
	/**
	 * 成功したStepだけを観察値へ保存する。
	 * @param Context 同じ固定更新の環境。
	 */
	void OnPostPhysicsStep_Internal(const FFixedTickContext& Context) override;
	/**
	 * Destroy要求後のPostPhysicsを抑止する。
	 */
	bool IsPostPhysicsStepAlive_Internal() const noexcept override;
	/**
	 * Worldと両端の生存を読み取る。
	 */
	bool IsConnected_Internal() const noexcept;
	/**
	 * 生存する拘束だけを安全に破棄する。
	 */
	void Release_Internal() noexcept;
	/**
	 * 現在接続している設定。
	 */
	FPrismaticJointComponentDescription3D m_Description;
	/**
	 * 次の境界で採用する設定。
	 */
	FPrismaticJointComponentDescription3D m_Requested;
	/**
	 * 新しい接続要求が残っているか。
	 */
	bool m_bConnectRequested = true;
	/**
	 * 解除要求が残っているか。
	 */
	bool m_bDisconnectRequested = false;
	/**
	 * 現在の接続を保ったまま、検証済みの設定集合を次の境界で反映する。
	 */
	bool m_bSettingsRequested = false;
	/**
	 * 現在の接続先World。Sceneの所有で、終了まで有効。
	 */
	FPhysicsWorld3D* m_pWorld = nullptr;
	/**
	 * Worldが所有する拘束の識別子。
	 */
	FJointId3D m_Joint;
	/**
	 * 最後に反映した接続状態。
	 */
	EJointConnection m_Connection = EJointConnection::PendingBodies;
	/**
	 * 成功したStepだけの観察値。
	 */
	Toolbox::TOptional<FPrismaticJointObservation3D> m_Observation;
	/**
	 * 成功PostPhysicsの通し番号。
	 */
	Toolbox::uint64 m_SuccessfulStep = 0;
};
} // namespace Dxf
#endif
