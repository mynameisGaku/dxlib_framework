// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_DEBUG_SNAPSHOT_3D_H
#define DXF_PHYSICS_DEBUG_SNAPSHOT_3D_H
#include "Dxf/Result.h"
#include "Toolbox/CollisionShapes.h"
namespace Dxf
{
/**
 * 観察登録時の運動区分。実Worldの設定を変更しない。
 */
enum class EDebugBodyMotion
{
	/**
	 * 静止Body。
	 */
	Static,
	/**
	 * 指定運動のBody。
	 */
	Kinematic,
	/**
	 * 力と接触に応答するBody。
	 */
	Dynamic
};
/**
 * 値として採取したColliderとBody状態。Native資源やWorldへの参照を持たない。
 */
struct FPhysicsDebugItem3D
{
	/**
	 * 登録Bodyのスロット番号。
	 */
	Toolbox::size_t BodyIndex = 0;
	/**
	 * 登録Bodyの世代。
	 */
	Toolbox::uint64 BodyGeneration = 0;
	/**
	 * Colliderのスロット番号。
	 */
	Toolbox::size_t ColliderIndex = 0;
	/**
	 * Colliderの世代。
	 */
	Toolbox::uint64 ColliderGeneration = 0;
	/**
	 * ワールド座標へ変換した、登録対象の形状。
	 */
	Toolbox::TVariant<Toolbox::FSphere, Toolbox::FOBB> Shape;
	/**
	 * 実Worldから読んだ重心位置。
	 */
	Toolbox::FVector3 CenterOfMass;
	/**
	 * 実Worldから読んだ毎秒メートルの速度。
	 */
	Toolbox::FVector3 Velocity;
	/**
	 * 実Worldから読んだワールド角速度。
	 */
	Toolbox::FVector3 AngularVelocity;
	/**
	 * 登録時に渡された運動区分。
	 */
	EDebugBodyMotion Motion = EDebugBodyMotion::Dynamic;
	/**
	 * 採取時の休止状態。
	 */
	bool bSleeping = false;
};
/**
 * 固定更新直後に採取する表示専用Snapshot。接触・Impulseの再計算は行わない。
 */
struct FPhysicsDebugSnapshot3D
{
	/**
	 * 観察対象のWorld識別子。
	 */
	Toolbox::uint64 World = 0;
	/**
	 * 呼出し側が管理する、実行済み固定更新番号。
	 */
	Toolbox::uint64 Step = 0;
	/**
	 * 実行済み固定更新の経過秒数。
	 */
	Toolbox::f64 SimulationSeconds = 0;
	/**
	 * 明示登録した、生存するColliderの採取結果。
	 */
	Toolbox::TVector<FPhysicsDebugItem3D> Items;
	/**
	 * 削除済みまたは期限切れになった登録の数。
	 */
	Toolbox::size_t SkippedCount = 0;
};
/**
 * Colliderを作成した際の定義を保存する観察登録。World全走査ではない。
 * 同じIDのShapeを変更するAPIを将来追加する場合は、登録側の定義も更新すること。
 */
template <typename TColliderId> struct TPhysicsDebugWatch3D
{
	/**
	 * Worldから受け取った世代付きCollider識別子。
	 */
	TColliderId Collider;
	/**
	 * AttachColliderへ実際に渡した重心相対形状。
	 */
	Toolbox::TVariant<Toolbox::FSphere, Toolbox::FOBB> LocalShape;
	/**
	 * CreateBodyへ実際に渡した運動区分。
	 */
	EDebugBodyMotion Motion = EDebugBodyMotion::Dynamic;
};
/**
 * @param Snapshot 所有状態を変えずに検査する固定更新の観察値。
 */
bool IsValidPhysicsDebugSnapshot3D(const FPhysicsDebugSnapshot3D& Snapshot) noexcept;
/**
 * 明示登録したColliderを公開読取APIから採取する。Step完了後、所有スレッドで呼ぶ。
 * Worldと登録配列を並行変更しない。戻り値は値所有で、World破棄後も利用できる。
 * 接触点・Impulse・Islandは取得していない。接触判定を再実行して実測値と偽らない。
 * @param World IsColliderAlive/GetPosition/GetOrientation/GetVelocity/GetAngularVelocity/IsSleepingを持つWorld。
 * @param Watches 作成時に記録した最大256件の観察対象。
 * @param Step 実行済み固定更新番号。
	 * @param Seconds 実行済みシミュレーション秒数。
 */
template <typename TWorld, typename TColliderId>
TResult<FPhysicsDebugSnapshot3D> CapturePhysicsDebugSnapshot3D(const TWorld& World,
	const Toolbox::TVector<TPhysicsDebugWatch3D<TColliderId>>& Watches,
	Toolbox::uint64 Step, Toolbox::f64 Seconds)
{
	if (Watches.Size() > 256 || !Toolbox::IsFinite(Seconds) || Seconds < 0)
	{
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidArgument, "Invalid snapshot request");
	}
	FPhysicsDebugSnapshot3D Snapshot;
	Snapshot.Step = Step;
	Snapshot.SimulationSeconds = Seconds;
	Snapshot.World = Watches.IsEmpty() ? 0 : Watches[0].Collider.Body.World;
	for (Toolbox::size_t Index = 0; Index < Watches.Size(); ++Index)
	{
		const auto& Watch = Watches[Index];
		if (Watch.Collider.Body.World != Snapshot.World || Snapshot.World == 0)
		{
			return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidArgument, "Mixed or invalid worlds");
		}
		for (Toolbox::size_t Previous = 0; Previous < Index; ++Previous)
		{
			if (Watches[Previous].Collider == Watch.Collider)
			{
				return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidArgument, "Duplicate collider watch");
			}
		}
		if (!World.IsColliderAlive(Watch.Collider))
		{
			++Snapshot.SkippedCount;
			continue;
		}
		FPhysicsDebugItem3D Item;
		Item.BodyIndex = Watch.Collider.Body.Index;
		Item.BodyGeneration = Watch.Collider.Body.Generation;
		Item.ColliderIndex = Watch.Collider.Index;
		Item.ColliderGeneration = Watch.Collider.Generation;
		Item.CenterOfMass = World.GetPosition(Watch.Collider.Body);
		const auto Orientation = World.GetOrientation(Watch.Collider.Body);
		Item.Velocity = World.GetVelocity(Watch.Collider.Body);
		Item.AngularVelocity = World.GetAngularVelocity(Watch.Collider.Body);
		Item.bSleeping = World.IsSleeping(Watch.Collider.Body);
		Item.Motion = Watch.Motion;
		Watch.LocalShape.Visit([&](const auto& Local)
		{
			auto Transformed = Local;
			Transformed.Center = Item.CenterOfMass + Orientation.Rotate(Local.Center);
			if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Local)>, Toolbox::FOBB>)
			{
				for (Toolbox::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Transformed.Axes[Axis] = Orientation.Rotate(Local.Axes[Axis]);
				}
			}
			Item.Shape = Toolbox::Move(Transformed);
		});
		Snapshot.Items.PushBack(Toolbox::Move(Item));
	}
	if (!IsValidPhysicsDebugSnapshot3D(Snapshot))
	{
		return TResult<FPhysicsDebugSnapshot3D>::Failure(EErrorCode::InvalidState, "Invalid captured physics state");
	}
	return TResult<FPhysicsDebugSnapshot3D>::Success(Toolbox::Move(Snapshot));
}
}
#endif
