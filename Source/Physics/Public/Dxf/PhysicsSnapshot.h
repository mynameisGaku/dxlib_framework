// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_SNAPSHOT_H
#define DXF_PHYSICS_SNAPSHOT_H
#include "Dxf/BodyType.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 明示的な観察要求の保持上限。超過時は切り詰めず例外で失敗する。
 */
struct FPhysicsSnapshotLimits
{
	/**
	 * 保持する生存Bodyの最大数。0は空Worldのみ許可する。
	 */
	Toolbox::size_t MaxBodies = 65536;
	/**
	 * 保持する生存Colliderの最大数。0はColliderのないWorldのみ許可する。
	 */
	Toolbox::size_t MaxColliders = 65536;
};
/**
 * 採取時の剛体状態。IDは識別情報であり、Worldへの所有・参照ではない。
 */
template <typename TId, typename TVector, typename TRotation, typename TAngular>
struct TPhysicsBodySnapshot
{
	/**
	 * 採取した剛体のWorld・スロット・世代。
	 */
	TId Id;
	/**
	 * 実Worldの運動区分。
	 */
	EBodyType Type = EBodyType::Dynamic;
	/**
	 * ワールド重心位置。メートル単位。
	 */
	TVector Position;
	/**
	 * 2Dではラジアン、3Dでは右手則の単位四元数。
	 */
	TRotation Rotation{};
	/**
	 * ワールド重心速度。メートル毎秒。
	 */
	TVector Velocity;
	/**
	 * ラジアン毎秒。3Dではワールド軸回り。
	 */
	TAngular AngularVelocity{};
	/**
	 * 採取時に休止しているか。
	 */
	bool bSleeping = false;
	/**
	 * 移動区間の連続衝突検査を利用するか。
	 */
	bool bUseContinuous = false;
};
/**
 * 採取時の形状と材質。形状は重心相対で値所有する。
 */
template <typename TId, typename TShape> struct TPhysicsColliderSnapshot
{
	/**
	 * Body識別子を含むColliderのスロットと世代。再登録は世代で区別する。
	 */
	TId Id;
	/**
	 * 採取時のローカル形状。Worldの配列やユーザーポインタを借用しない。
	 */
	TShape LocalShape;
	/**
	 * 摩擦係数。
	 */
	Toolbox::f32 Friction = 0;
	/**
	 * 反発係数。
	 */
	Toolbox::f32 Restitution = 0;
};
/**
 * 明示採取したWorldの観察値。接触再計算・自動採取・巻戻しは行わない。
 * 複製は配列も複製する。World破棄後も保持できるが、共有後の書込は利用者が同期する。
 */
template <typename TBodyId, typename TColliderId, typename TVector, typename TRotation,
          typename TAngular, typename TShape>
struct TPhysicsSnapshot
{
	/**
	 * 剛体の観察値型。
	 */
	using FBody = TPhysicsBodySnapshot<TBodyId, TVector, TRotation, TAngular>;
	/**
	 * 形状の観察値型。
	 */
	using FCollider = TPhysicsColliderSnapshot<TColliderId, TShape>;
	/**
	 * 元Worldの識別子。2Dと3Dの識別空間は別。
	 */
	Toolbox::uint64 World = 0;
	/**
	 * 正常完了したStep呼出しの通算数。内部SubStepsや描画フレーム数ではない。
	 */
	Toolbox::uint64 StepIndex = 0;
	/**
	 * 最後に正常完了したStepに渡した秒数。未更新なら0。
	 */
	Toolbox::f64 LastDeltaSeconds = 0;
	/**
	 * 最後に正常完了したStepの分割数。未更新なら0。
	 */
	Toolbox::uint32 LastSubSteps = 0;
	/**
	 * 生存Body全件。元スロット番号の昇順。Colliderを持たないBodyも含む。
	 */
	Toolbox::TVector<FBody> Bodies;
	/**
	 * 生存Collider全件。元スロット番号の昇順。
	 */
	Toolbox::TVector<FCollider> Colliders;
};
} // namespace Dxf
#endif
