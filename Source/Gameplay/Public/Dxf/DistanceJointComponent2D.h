// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_DISTANCEJOINTCOMPONENT2D_H
#define DXF_GAMEPLAY_DISTANCEJOINTCOMPONENT2D_H
#include "Dxf/DistanceJointComponentDescription2D.h"
#include "Dxf/DistanceJointObservation2D.h"
#include "Dxf/DistanceJointConnection.h"
#include "Dxf/PrePhysicsStep.h"
#include "Dxf/PostPhysicsStep.h"
namespace Dxf
{
/**
 * 距離拘束一つの接続要求と寿命を管理する。Bodyは所有せず、描画しない。
 */
class DDistanceJoint2DComponent : public DGameObjectComponent, private IPrePhysicsStep, private IPostPhysicsStep
{
public:
	/**
	 * @param Description 両端と距離設定。非有限値・負の長さは構築時に拒否する。
	 */
	explicit DDistanceJoint2DComponent(FDistanceJointComponentDescription2D Description);
	/**
	 * 次の固定更新境界で解除する。同じ境界までの最後の要求を採用する。
	 */
	void RequestDisconnect() noexcept;
	/**
	 * 次の固定更新境界で接続する。同じ有効な接続の再要求はIDを維持する。
	 * @param Description 新しい両端と設定。不正数値は要求時に拒否する。
	 */
	void RequestConnect(FDistanceJointComponentDescription2D Description);
	/**
	 * 破棄要求やWorld側の失効も調べる。生成・起床・Stepは行わない。
	 */
	EDistanceJointConnection GetConnectionState() const noexcept;
	/**
	 * 現在生存する接続ID。未接続・失効時は空。
	 */
	Toolbox::TOptional<FJointId2D> GetJointId() const noexcept;
	/**
	 * 最後の成功PostPhysics値。次のStep開始・解除・失効後は空。
	 */
	Toolbox::TOptional<FDistanceJointObservation2D> GetObservation() const noexcept;
	/**
	 * 現在の接続設定。保留中の要求とは区別する。
	 */
	const FDistanceJointComponentDescription2D& GetDescription() const noexcept;
	/**
	 * 表示Bodyと同じ補間姿勢の両Anchorを得る。失効時はfalse。
	 * @param A A側の出力。
	 * @param B B側の出力。
	 */
	bool GetRenderAnchors(Toolbox::FVector2& A, Toolbox::FVector2& B) const;

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
	FDistanceJointComponentDescription2D m_Description;
	/**
	 * 次の境界で採用する設定。
	 */
	FDistanceJointComponentDescription2D m_Requested;
	/**
	 * 新しい接続要求が残っているか。
	 */
	bool m_bConnectRequested = true;
	/**
	 * 解除要求が残っているか。
	 */
	bool m_bDisconnectRequested = false;
	/**
	 * 現在の接続先World。Sceneの所有で、終了まで有効。
	 */
	FPhysicsWorld2D* m_pWorld = nullptr;
	/**
	 * Worldが所有する拘束の識別子。
	 */
	FJointId2D m_Joint;
	/**
	 * 最後に反映した接続状態。
	 */
	EDistanceJointConnection m_Connection = EDistanceJointConnection::PendingBodies;
	/**
	 * 成功したStepだけの観察値。
	 */
	Toolbox::TOptional<FDistanceJointObservation2D> m_Observation;
	/**
	 * 成功PostPhysicsの通し番号。
	 */
	Toolbox::uint64 m_SuccessfulStep = 0;
};
} // namespace Dxf
#endif
