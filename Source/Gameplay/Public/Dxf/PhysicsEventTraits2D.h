#pragma once
#include "Dxf/CharacterMovementComponent2D.h"
#include "Dxf/ContactNotice.h"
#include "Dxf/RigidBodyComponent2D.h"
namespace Dxf
{
/**
 * 接触イベントのComponentが使う、2DのWorldの型と操作。
 */
struct FPhysicsEventTraits2D
{
	/**
	 * 2DのWorld。
	 */
	using FWorld = FPhysicsWorld2D;
	/**
	 * Body・ColliderのID。
	 */
	using FBodyId = FBodyId2D;
	using FColliderId = FColliderId2D;
	/**
	 * 位置の型。
	 */
	using FVector = Toolbox::FVector2;
	/**
	 * Colliderの形状（円または回転矩形）。
	 */
	using FShape = decltype(FColliderDescription2D::Shape);
	/**
	 * 配送先から見た状態遷移。
	 */
	using FNotice = FContactNotice2D;
	/**
	 * 固定更新の実行環境の2DのWorld（2Dの物理シーン以外はnullptr）。
	 * @param Context 固定更新の実行環境。
	 */
	static FWorld* GetWorld(const FFixedTickContext& Context) noexcept
	{
		return Context.Physics2D;
	}
	/**
	 * 所有者の剛体・キャラクター移動のBody（あれば）を集める。
	 * @param Owner 所有者。
	 * @param Out 追加先。
	 */
	static void CollectOwnerBodies(const DGameObject& Owner, Toolbox::TVector<FBodyId>& Out)
	{
		DRigidBody2DComponent* Body = Owner.FindComponent<DRigidBody2DComponent>().Get();
		if (Body != nullptr && Body->HasBody())
		{
			Out.PushBack(Body->GetBodyId());
		}
		DCharacterMovement2DComponent* Character = Owner.FindComponent<DCharacterMovement2DComponent>().Get();
		if (Character != nullptr && Character->GetBodyId())
		{
			Out.PushBack(*Character->GetBodyId());
		}
	}
	/**
	 * Bodyを指定の位置へ置く（向きは0）。
	 * @param World 対象のWorld。
	 * @param Body 対象のBody。
	 * @param Position 新しい位置。
	 */
	static void Place(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 0);
	}
	/**
	 * Bodyの登録内容。
	 * @param Type 運動区分。
	 * @param Position 位置。
	 */
	static FBodyDescription2D BodyDescription(EBodyType Type, FVector Position)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = Position;
		return Description;
	}
	/**
	 * Colliderの登録内容。
	 * @param Shape 形状。
	 */
	static FColliderDescription2D ColliderDescription(const FShape& Shape)
	{
		FColliderDescription2D Description;
		Description.Shape = Shape;
		return Description;
	}
	/**
	 * 例外の文言に使う次元の名前。
	 */
	static constexpr const char* Name = "2D";
};
} // namespace Dxf
