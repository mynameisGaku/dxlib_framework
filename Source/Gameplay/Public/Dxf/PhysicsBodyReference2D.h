// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_PHYSICSBODYREFERENCE2D_H
#define DXF_GAMEPLAY_PHYSICSBODYREFERENCE2D_H
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/KinematicMoverComponent2D.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * 世代付きComponent参照または明示Body ID。所有せず、同slotの別個体へ追従しない。
 */
class FPhysicsBodyReference2D
{
public:
	/**
	 * 未指定参照を作る。接続時には明確に拒否される。
	 */
	FPhysicsBodyReference2D() = default;
	/**
	 * @param Handle 同次元の剛体Component。登録前は固定更新境界で待つ。
	 */
	static FPhysicsBodyReference2D FromRigidBody(TObjectHandle<DRigidBody2DComponent> Handle) noexcept;
	/**
	 * @param Handle 同次元の移動支点Component。別の剛体を重ねて作らない。
	 */
	static FPhysicsBodyReference2D FromKinematicMover(TObjectHandle<DKinematicMover2DComponent> Handle) noexcept;
	/**
	 * @param Id 現在Worldで生存する完全な世代付きBody ID。自動更新しない。
	 */
	static FPhysicsBodyReference2D FromBodyId(FBodyId2D Id) noexcept;
	/**
	 * 同じ種類と完全な識別子かを調べる。
	 * @param Other 比較相手。
	 */
	bool operator==(const FPhysicsBodyReference2D& Other) const noexcept;
	/**
	 * 登録前は空、失効・別Worldは失敗を返す。Worldを変更しない。
	 * @param World 接続を要求された物理World。
	 */
	TResult<Toolbox::TOptional<FBodyId2D>> Resolve_Internal(const FPhysicsWorld2D& World) const;
	/**
	 * 描画用の補間姿勢からAnchorを求める。明示IDは現在姿勢を使う。
	 * @param World 接続先World。
	 * @param Local Body重心基準のAnchor。
	 */
	Toolbox::FVector2 RenderAnchor_Internal(const FPhysicsWorld2D& World, Toolbox::FVector2 Local) const;
	/**
	 * 登録済みで同じWorldに生存するか。確保せず、生成しない。
	 * @param World 読み取るWorld。
	 */
	bool IsAvailable_Internal(const FPhysicsWorld2D& World) const noexcept;

private:
	/**
	 * 参照の種類。初期値は未指定。
	 */
	enum class EKind
	{
		/**
		 * 未指定。
		 */
		Empty,
		/**
		 * 通常剛体Componentの世代付き参照。
		 */
		Rigid,
		/**
		 * 動く支点Componentの世代付き参照。
		 */
		Mover,
		/**
		 * World番号を持つ明示Body ID。
		 */
		Explicit
	};
	/**
	 * 保持している参照の種類。
	 */
	EKind m_Kind = EKind::Empty;
	/**
	 * 非所有の剛体Component参照。
	 */
	TObjectHandle<DRigidBody2DComponent> m_Rigid;
	/**
	 * 非所有の移動支点Component参照。
	 */
	TObjectHandle<DKinematicMover2DComponent> m_Mover;
	/**
	 * 自動更新しない明示Body ID。
	 */
	FBodyId2D m_Body;
};
} // namespace Dxf
#endif
