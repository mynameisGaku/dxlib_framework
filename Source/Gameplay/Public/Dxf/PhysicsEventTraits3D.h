#pragma once
#include "Dxf/CharacterMovementComponent3D.h"
#include "Dxf/ContactNotice.h"
#include "Dxf/RigidBodyComponent3D.h"
namespace Dxf
{
/**
 * 接触イベントのComponentが使う、3DのWorldの型と操作。
 */
struct FPhysicsEventTraits3D
{
	/**
	 * 3DのWorld。
	 */
	using FWorld = FPhysicsWorld3D;
	/**
	 * Body・ColliderのID。
	 */
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	/**
	 * 位置の型。
	 */
	using FVector = Toolbox::FVector3;
	/**
	 * Colliderの形状（球または任意姿勢の箱）。
	 */
	using FShape = decltype(FColliderDescription3D::Shape);
	/**
	 * 配送先から見た状態遷移。
	 */
	using FNotice = FContactNotice3D;
	/**
	 * 固定更新の実行環境の3DのWorld（3Dの物理シーン以外はnullptr）。
	 * @param Context 固定更新の実行環境。
	 */
	static FWorld* GetWorld(const FFixedTickContext& Context) noexcept
	{
		return Context.Physics3D;
	}
	/**
	 * 所有者の剛体・キャラクター移動のBody（あれば）を集める。
	 * @param Owner 所有者。
	 * @param Out 追加先。
	 */
	static void CollectOwnerBodies(const DGameObject& Owner, Toolbox::TVector<FBodyId>& Out)
	{
		DRigidBody3DComponent* Body = Owner.FindComponent<DRigidBody3DComponent>().Get();
		if (Body != nullptr && Body->HasBody())
		{
			Out.PushBack(Body->GetBodyId());
		}
		DCharacterMovement3DComponent* Character = Owner.FindComponent<DCharacterMovement3DComponent>().Get();
		if (Character != nullptr && Character->GetBodyId())
		{
			Out.PushBack(*Character->GetBodyId());
		}
	}
	/**
	 * Bodyを指定の位置へ置く（姿勢は単位）。
	 * @param World 対象のWorld。
	 * @param Body 対象のBody。
	 * @param Position 新しい位置。
	 */
	static void Place(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, Toolbox::FQuaternion{});
	}
	/**
	 * Bodyの登録内容。
	 * @param Type 運動区分。
	 * @param Position 位置。
	 */
	static FBodyDescription3D BodyDescription(EBodyType Type, FVector Position)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = Position;
		return Description;
	}
	/**
	 * Colliderの登録内容。
	 * @param Shape 形状。
	 */
	static FColliderDescription3D ColliderDescription(const FShape& Shape)
	{
		FColliderDescription3D Description;
		Description.Shape = Shape;
		return Description;
	}
	/**
	 * 例外の文言に使う次元の名前。
	 */
	static constexpr const char* Name = "3D";
};
} // namespace Dxf
