// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_DEBUG_SNAPSHOT_3D_H
#define DXF_PHYSICS_DEBUG_SNAPSHOT_3D_H
#include "Dxf/Result.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/CollisionShapes.h"
namespace Dxf
{
/**
 * 1回の観察で表示用に変換するColliderの上限。超過は切り詰めず失敗する。
 */
inline constexpr Toolbox::size_t MaxPhysicsDebugColliders3D = 256;
/**
 * 1回の観察でWorldから複製するBodyの上限。Colliderを持たないBodyも数える。
 */
inline constexpr Toolbox::size_t MaxPhysicsDebugBodies3D = 4096;
/**
 * 一つのColliderを表示用のワールド座標へ変換した値。Native資源やWorldへの参照を持たない。
 */
struct FPhysicsDebugItem3D
{
	/**
	 * 採取元Colliderの世代付きID。取り付け先BodyのIDを含む。
	 */
	FColliderId3D Collider;
	/**
	 * Bodyの位置・姿勢で重心相対形状をワールド座標へ変換した形状。
	 */
	Toolbox::TVariant<Toolbox::FSphere, Toolbox::FOBB> Shape;
	/**
	 * 採取時のBody重心位置。メートル単位。
	 */
	Toolbox::FVector3 CenterOfMass;
	/**
	 * 採取時のBody重心速度。メートル毎秒。
	 */
	Toolbox::FVector3 Velocity;
	/**
	 * 採取時のBody角速度。ワールド軸回りのラジアン毎秒。
	 */
	Toolbox::FVector3 AngularVelocity;
	/**
	 * 採取時の実Worldの運動区分。
	 */
	EBodyType Type = EBodyType::Dynamic;
	/**
	 * 採取時の休止状態。
	 */
	bool bSleeping = false;
};
/**
 * Worldが採取したFPhysicsSnapshot3Dを表示用に変換した値。接触・Impulseの再計算は行わない。
 */
struct FPhysicsDebugSnapshot3D
{
	/**
	 * 採取元World。0は未採取を表す。
	 */
	Toolbox::uint64 World = 0;
	/**
	 * Worldが数えた正常完了Step数。描画回数や内部SubStepsではない。
	 */
	Toolbox::uint64 Step = 0;
	/**
	 * 最後に正常完了したStepの秒数。未更新なら0。
	 */
	Toolbox::f64 LastDeltaSeconds = 0;
	/**
	 * 最後に正常完了したStepの分割数。未更新なら0。
	 */
	Toolbox::uint32 LastSubSteps = 0;
	/**
	 * 呼出し側の時計で記録したシミュレーション経過秒数。Step数から逆算しない。
	 */
	Toolbox::f64 SimulationSeconds = 0;
	/**
	 * 採取時の生存Body数。Colliderを持たないBodyも含む。
	 */
	Toolbox::size_t BodyCount = 0;
	/**
	 * 採取時の生存Collider全件。元スロット番号の昇順で、切り詰めない。
	 */
	Toolbox::TVector<FPhysicsDebugItem3D> Items;
};
/**
 * 表示用Snapshotの値域・件数・形状を検査する。
 * @param Snapshot 所有状態を変えずに検査する観察値。
 */
bool IsValidPhysicsDebugSnapshot3D(const FPhysicsDebugSnapshot3D& Snapshot) noexcept;
/**
 * Worldが採取した値を表示用のワールド座標へ変換する。Worldへは触れない。
 * BodyとColliderは世代付きIDで対応を確認し、不一致・非有限値・上限超過は失敗にする。
 * @param Source FPhysicsWorld3D::CaptureSnapshotの戻り値。
 * @param SimulationSeconds 呼出し側の時計で記録した0以上の経過秒数。
 */
TResult<FPhysicsDebugSnapshot3D> BuildPhysicsDebugSnapshot3D(const FPhysicsSnapshot3D& Source,
                                                             Toolbox::f64 SimulationSeconds);
/**
 * 正常Step完了後のWorldを明示的に採取し、表示用に変換する。Stepと並行して呼ばない。
 * 採取の拒否・上限超過・確保失敗は例外ではなくTResultの失敗として返す。
 * @param World 採取するWorld。変更しない。
 * @param SimulationSeconds 呼出し側の時計で記録した0以上の経過秒数。
 */
TResult<FPhysicsDebugSnapshot3D> CapturePhysicsDebugSnapshot3D(const FPhysicsWorld3D& World,
                                                               Toolbox::f64 SimulationSeconds);
} // namespace Dxf
#endif
