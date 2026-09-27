// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_TRAITS_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_TRAITS_H
#include "InteractionLayout.h"
#include "InteractionMode.h"
#include "InteractionRules.h"
#include "SampleCharacters.h"
#include "Dxf/ContactListenerComponent2D.h"
#include "Dxf/ContactListenerComponent3D.h"
#include "Dxf/KinematicMoverComponent2D.h"
#include "Dxf/KinematicMoverComponent3D.h"
#include "Dxf/PhysicsScene2D.h"
#include "Dxf/PhysicsScene3D.h"
#include "Dxf/RigidBodyComponent2D.h"
#include "Dxf/RigidBodyComponent3D.h"
#include "Dxf/TriggerVolumeComponent2D.h"
#include "Dxf/TriggerVolumeComponent3D.h"
namespace Dxf::GameplaySample
{
/**
 * 接触・Triggerのサンプルの2Dの型と形状（Yが上）。
 */
struct FInteraction2D
{
	using FVector = Toolbox::FVector2;
	using FBodyId = FBodyId2D;
	using FRigid = DRigidBody2DComponent;
	using FCollider = DCollider2DComponent;
	using FBodyDescription = FBodyDescription2D;
	using FColliderDescription = FColliderDescription2D;
	using FListener = DContactListener2DComponent;
	using FNotice = FContactNotice2D;
	using FTrigger = DTriggerVolume2DComponent;
	using FTriggerDescription = FTriggerVolumeDescription2D;
	using FMover = DKinematicMover2DComponent;
	using FMoverDescription = FKinematicMoverDescription2D;
	using FPose = FKinematicPose2D;
	using FPlayer = DPlayer2D;
	/**
	 * 配置の(X, Y)の位置。
	 */
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y) noexcept
	{
		return {X, Y};
	}
	/**
	 * 箱の形状（中心は重心）。
	 */
	static FColliderDescription Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{{0, 0}, {HalfX, HalfY}, 0};
		return Description;
	}
	/**
	 * 球（円）の形状。
	 */
	static FColliderDescription Ball(Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FCircle2D{{0, 0}, Radius};
		return Description;
	}
	/**
	 * 回転する床の向き（2Dは歩ける範囲の傾き）。
	 * @param Seconds 経路の時刻。
	 */
	static FPose Turn(Toolbox::f64 Seconds)
	{
		FPose Pose;
		Pose.Position = At(InteractionLayout::TurnX, InteractionLayout::TurnY);
		Pose.Rotation = static_cast<Toolbox::f32>(InteractionLayout::TiltAt(Seconds));
		return Pose;
	}
	/**
	 * 床のBodyの登録。
	 */
	static FBodyDescription StaticBody(FVector Position)
	{
		FBodyDescription Description;
		Description.Type = EBodyType::Static;
		Description.Position = Position;
		return Description;
	}
	/**
	 * 地形の箱の形状（原点に置いたBodyからの相対）。
	 * @param Box 配置の箱。
	 */
	static FColliderDescription Ground(const FLevelBox& Box)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOrientedBox2D{Box.Center, Box.Half, Box.Angle};
		return Description;
	}
};
/**
 * 接触・Triggerのサンプルの3Dの型と形状（Yが上、Z=0の平面に2Dと同じ配置）。
 */
struct FInteraction3D
{
	using FVector = Toolbox::FVector3;
	using FBodyId = FBodyId3D;
	using FRigid = DRigidBody3DComponent;
	using FCollider = DCollider3DComponent;
	using FBodyDescription = FBodyDescription3D;
	using FColliderDescription = FColliderDescription3D;
	using FListener = DContactListener3DComponent;
	using FNotice = FContactNotice3D;
	using FTrigger = DTriggerVolume3DComponent;
	using FTriggerDescription = FTriggerVolumeDescription3D;
	using FMover = DKinematicMover3DComponent;
	using FMoverDescription = FKinematicMoverDescription3D;
	using FPose = FKinematicPose3D;
	using FPlayer = DPlayer3D;
	static FVector At(Toolbox::f32 X, Toolbox::f32 Y) noexcept
	{
		return {X, Y, 0};
	}
	static FColliderDescription Box(Toolbox::f32 HalfX, Toolbox::f32 HalfY)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FOBB{{0, 0, 0}, {HalfX, HalfY, InteractionLayout::DepthHalf}};
		return Description;
	}
	static FColliderDescription Ball(Toolbox::f32 Radius)
	{
		FColliderDescription Description;
		Description.Shape = Toolbox::FSphere{{0, 0, 0}, Radius};
		return Description;
	}
	static FPose Turn(Toolbox::f64 Seconds)
	{
		FPose Pose;
		Pose.Position = At(InteractionLayout::TurnX, InteractionLayout::TurnY);
		Pose.Rotation = Toolbox::FQuaternion::FromAxisAngle(
		    {0, 1, 0}, static_cast<Toolbox::f32>(InteractionLayout::YawAt(Seconds)));
		return Pose;
	}
	static FBodyDescription StaticBody(FVector Position)
	{
		FBodyDescription Description;
		Description.Type = EBodyType::Static;
		Description.Position = Position;
		return Description;
	}
	static FColliderDescription Ground(const FLevelBox& Box)
	{
		FColliderDescription Description;
		Description.Shape = LevelBoxShape3D(Box);
		return Description;
	}
};
/**
 * 接触・Triggerのサンプルのシーンが、置いたオブジェクトへ渡す窓口（規則・プレイヤーのBody・復帰）。
 * @tparam T 次元の型（FInteraction2D／FInteraction3D）。
 */
template <typename T> class IInteractionHost
{
public:
	/**
	 * ゲームの規則と状態。
	 */
	virtual FInteractionRules& GetRules() noexcept = 0;
	/**
	 * プレイヤーのBody（未登録なら空）。
	 */
	virtual Toolbox::TOptional<typename T::FBodyId> GetPlayerBody() const noexcept = 0;
	/**
	 * プレイヤーを最後のチェックポイントへ戻す（支持・速度・補間の履歴を初期化する）。
	 */
	virtual void RespawnPlayer() = 0;
	/**
	 * キャラクターの遊び方（形状・押し合い）。
	 */
	virtual const FInteractionMode& GetMode() const noexcept = 0;
	/**
	 * プレイヤー（初期化前・破棄後はnullptr）。
	 */
	virtual typename T::FPlayer* GetPlayerObject() const noexcept = 0;

protected:
	/**
	 * シーンが所有する。
	 */
	~IInteractionHost() = default;
};
} // namespace Dxf::GameplaySample
#endif
