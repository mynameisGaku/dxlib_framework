// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_RIGID_BODY_COMPONENT_3D_H
#define DXF_GAMEPLAY_RIGID_BODY_COMPONENT_3D_H
#include "Dxf/GameObjectComponent.h"
#include "Dxf/GameObject.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 物理シーンの剛体へゲーム側の姿勢と外力を中継する。
 * Dynamicは物理状態が正、Kinematicはゲーム側の速度指示が正になる。
 */
class DRigidBody3DComponent : public DGameObjectComponent
{
public:
	/**
	 * 初期条件を受け取り、物理登録前の状態を構築する。
	 * @param Description 初期条件。
	 */
	explicit DRigidBody3DComponent(FBodyDescription3D Description = {}) : m_Description(Description)
	{
		m_GamePosition = Description.Position;
		m_GameOrientation = Description.Orientation;
		m_GameVelocity = Description.Velocity;
		m_GameAngularVelocity = Description.AngularVelocity;
		m_PrevPosition = m_GamePosition;
		m_PrevOrientation = m_GameOrientation;
		m_CurrentPosition = m_GamePosition;
		m_CurrentOrientation = m_GameOrientation;
		m_Type = Description.Type;
	}
	/**
	 * 物理登録が済んでいるかを調べる。
	 */
	FORCEINLINE bool HasBody() const noexcept
	{
		return m_bHasBody;
	}
	/**
	 * 物理登録のIDを返す。未登録は例外で通知する。
	 */
	FORCEINLINE FBodyId3D GetBodyId() const
	{
		if (!m_bHasBody)
		{
			throw Toolbox::FException("3D rigid body is not created");
		}
		return m_Body;
	}
	/**
	 * 最後に参照した物理ワールドを返す。未参照はnullptr。
	 */
	FORCEINLINE FPhysicsWorld3D* GetWorld() noexcept
	{
		return m_pWorld;
	}
	/**
	 * ゲーム側の重心位置を返す。
	 */
	FORCEINLINE Toolbox::FVector3 GetGamePosition() const noexcept
	{
		return m_GamePosition;
	}
	/**
	 * ゲーム側の姿勢を返す。
	 */
	FORCEINLINE Toolbox::FQuaternion GetGameOrientation() const noexcept
	{
		return m_GameOrientation;
	}
	/**
	 * ゲーム側の重心位置と姿勢を指定する。Kinematicの移動指示に使う。
	 * @param Position メートル単位の重心位置。
	 * @param Orientation 右手則の単位姿勢。
	 */
	void SetGameTransform(Toolbox::FVector3 Position, Toolbox::FQuaternion Orientation)
	{
		if (!Position.IsValid())
		{
			throw Toolbox::FException("Invalid 3D game transform");
		}
		m_GamePosition = Position;
		m_GameOrientation = Orientation;
	}
	/**
	 * ゲーム側の速度を指定する。Kinematicの運動指示に使う。
	 * @param Velocity 毎秒メートル単位の速度。
	 * @param AngularVelocity ワールド軸回りの毎秒ラジアン単位の角速度。
	 */
	void SetGameVelocity(Toolbox::FVector3 Velocity, Toolbox::FVector3 AngularVelocity)
	{
		if (!Velocity.IsValid() || !AngularVelocity.IsValid())
		{
			throw Toolbox::FException("Invalid 3D game velocity");
		}
		m_GameVelocity = Velocity;
		m_GameAngularVelocity = AngularVelocity;
	}
	/**
	 * 物理姿勢を直接移し、補間履歴と接触記録を破棄する。
	 * @param Position メートル単位の重心位置。
	 * @param Orientation 右手則の単位姿勢。
	 */
	void Teleport(Toolbox::FVector3 Position, Toolbox::FQuaternion Orientation)
	{
		if (!Position.IsValid())
		{
			throw Toolbox::FException("Invalid 3D teleport transform");
		}
		m_GamePosition = Position;
		m_GameOrientation = Orientation;
		m_PrevPosition = Position;
		m_PrevOrientation = Orientation;
		m_CurrentPosition = Position;
		m_CurrentOrientation = Orientation;
		m_Alpha = 0;
		if (m_bHasBody && m_pWorld != nullptr)
		{
			m_pWorld->SetBodyTransform(m_Body, Position, Orientation);
			m_pWorld->ClearContactCache();
		}
	}
	/**
	 * 次の固定更新で使う力を加算する。Dynamic以外は例外で通知する。
	 * @param Force ニュートン単位の力。
	 */
	void AddForce(Toolbox::FVector3 Force)
	{
		if (!Force.IsValid())
		{
			throw Toolbox::FException("Invalid 3D pending force");
		}
		RequireDynamic_Internal();
		m_PendingForce += Force;
	}
	/**
	 * 次の固定更新で使うトルクを加算する。Dynamic以外は例外で通知する。
	 * @param Torque ワールド軸回りのニュートンメートル単位のトルク。
	 */
	void AddTorque(Toolbox::FVector3 Torque)
	{
		if (!Torque.IsValid())
		{
			throw Toolbox::FException("Invalid 3D pending torque");
		}
		RequireDynamic_Internal();
		m_PendingTorque += Torque;
	}
	/**
	 * 速度へ即時反映する力積を予約する。Dynamic以外は例外で通知する。
	 * @param Impulse ニュートン秒単位の力積。
	 */
	void AddImpulse(Toolbox::FVector3 Impulse)
	{
		if (!Impulse.IsValid())
		{
			throw Toolbox::FException("Invalid 3D pending impulse");
		}
		RequireDynamic_Internal();
		m_PendingImpulse += Impulse;
	}
	/**
	 * 角速度へ即時反映する力積モーメントを予約する。Dynamic以外は例外で通知する。
	 * @param Impulse ワールド軸回りのニュートンメートル秒単位の力積モーメント。
	 */
	void AddAngularImpulse(Toolbox::FVector3 Impulse)
	{
		if (!Impulse.IsValid())
		{
			throw Toolbox::FException("Invalid 3D pending angular impulse");
		}
		RequireDynamic_Internal();
		m_PendingAngularImpulse += Impulse;
	}
	/**
	 * 重心外の点への力積を予約する。Dynamic以外は例外で通知する。
	 * @param Impulse ニュートン秒単位の力積。
	 * @param WorldPoint 力積を与えるワールド位置。
	 */
	void AddImpulseAt(Toolbox::FVector3 Impulse, Toolbox::FVector3 WorldPoint)
	{
		if (!Impulse.IsValid() || !WorldPoint.IsValid())
		{
			throw Toolbox::FException("Invalid 3D pending impulse point");
		}
		RequireDynamic_Internal();
		FPendingPointImpulse Pending;
		Pending.Impulse = Impulse;
		Pending.Point = WorldPoint;
		m_PendingPoints.PushBack(Pending);
	}
	/**
	 * 描画用の補間位置を返す。
	 */
	Toolbox::FVector3 GetRenderPosition() const noexcept
	{
		const Toolbox::f32 Alpha = static_cast<Toolbox::f32>(m_Alpha);
		return m_PrevPosition * (1 - Alpha) + m_CurrentPosition * Alpha;
	}
	/**
	 * 描画用の補間姿勢を返す。
	 */
	Toolbox::FQuaternion GetRenderOrientation() const noexcept
	{
		const Toolbox::f32 Alpha = static_cast<Toolbox::f32>(m_Alpha);
		return Toolbox::FQuaternion::Slerp(m_PrevOrientation, m_CurrentOrientation, Alpha);
	}

protected:
	/**
	 * 物理登録の確保と固定更新前の同期を行う。
	 * @param Context 固定更新の実行環境。
	 */
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		if (Context.Physics3D == nullptr)
		{
			return;
		}
		m_pWorld = Context.Physics3D;
		if (!m_bHasBody)
		{
			EnsureBody_Internal(*m_pWorld);
		}
		if (m_Type == EBodyType::Kinematic)
		{
			m_pWorld->SetVelocity(m_Body, m_GameVelocity);
			m_pWorld->SetAngularVelocity(m_Body, m_GameAngularVelocity);
		}
		// 更新前の物理姿勢を補間開始点へ保存する。
		m_PrevPosition = m_pWorld->GetPosition(m_Body);
		m_PrevOrientation = m_pWorld->GetOrientation(m_Body);
		FlushPending_Internal(*m_pWorld);
		m_Alpha = Context.InterpolationAlpha;
	}
	/**
	 * 物理状態をゲーム側へ取り込む。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext&) override
	{
		if (m_bHasBody && m_pWorld != nullptr && m_Type == EBodyType::Dynamic)
		{
			m_CurrentPosition = m_pWorld->GetPosition(m_Body);
			m_CurrentOrientation = m_pWorld->GetOrientation(m_Body);
		}
		else
		{
			m_CurrentPosition = m_GamePosition;
			m_CurrentOrientation = m_GameOrientation;
		}
	}
	/**
	 * 物理登録を破棄する。
	 */
	void OnDeinitialize() noexcept override
	{
		if (m_bHasBody && m_pWorld != nullptr)
		{
			m_pWorld->DestroyBody(m_Body);
		}
		m_bHasBody = false;
		m_pWorld = nullptr;
	}

private:
	/**
	 * 予約した重心外力積。
	 */
	struct FPendingPointImpulse
	{
		/**
		 * 与える力積。
		 */
		Toolbox::FVector3 Impulse;
		/**
		 * 力積を与える位置。
		 */
		Toolbox::FVector3 Point;
	};
	/**
	 * Dynamic以外への外力予約を拒否する。
	 */
	void RequireDynamic_Internal() const
	{
		if (m_Type != EBodyType::Dynamic)
		{
			throw Toolbox::FException("Only dynamic 3D bodies accept inputs");
		}
	}
	/**
	 * 物理登録を確保する。
	 * @param World 取り付け先の物理ワールド。
	 */
	void EnsureBody_Internal(FPhysicsWorld3D& World)
	{
		FBodyDescription3D Description = m_Description;
		Description.Position = m_GamePosition;
		Description.Orientation = m_GameOrientation;
		Description.Velocity = m_GameVelocity;
		Description.AngularVelocity = m_GameAngularVelocity;
		m_Body = World.CreateBody(Description);
		m_bHasBody = true;
		m_PrevPosition = m_GamePosition;
		m_PrevOrientation = World.GetOrientation(m_Body);
		m_CurrentPosition = m_GamePosition;
		m_CurrentOrientation = m_PrevOrientation;
	}
	/**
	 * 予約した外力を物理へ渡して消去する。
	 * @param World 取り付け先の物理ワールド。
	 */
	void FlushPending_Internal(FPhysicsWorld3D& World)
	{
		if (m_Type != EBodyType::Dynamic)
		{
			return;
		}
		World.ApplyForce(m_Body, m_PendingForce);
		World.ApplyTorque(m_Body, m_PendingTorque);
		World.ApplyLinearImpulse(m_Body, m_PendingImpulse);
		World.ApplyAngularImpulse(m_Body, m_PendingAngularImpulse);
		for (Toolbox::size_t Index = 0; Index < m_PendingPoints.Size(); ++Index)
		{
			World.ApplyImpulseAtPoint(m_Body, m_PendingPoints[Index].Impulse, m_PendingPoints[Index].Point);
		}
		m_PendingForce = {};
		m_PendingTorque = {};
		m_PendingImpulse = {};
		m_PendingAngularImpulse = {};
		m_PendingPoints.Clear();
	}
	/**
	 * 生成時の初期条件。
	 */
	FBodyDescription3D m_Description;
	/**
	 * 物理登録のID。
	 */
	FBodyId3D m_Body;
	/**
	 * 物理登録が済んでいるか。
	 */
	bool m_bHasBody = false;
	/**
	 * 最後に参照した物理ワールド。所有しない。
	 */
	FPhysicsWorld3D* m_pWorld = nullptr;
	/**
	 * 運動区分。
	 */
	EBodyType m_Type = EBodyType::Dynamic;
	/**
	 * ゲーム側の重心位置。
	 */
	Toolbox::FVector3 m_GamePosition;
	/**
	 * ゲーム側の姿勢。
	 */
	Toolbox::FQuaternion m_GameOrientation;
	/**
	 * ゲーム側の速度。
	 */
	Toolbox::FVector3 m_GameVelocity;
	/**
	 * ゲーム側の角速度。
	 */
	Toolbox::FVector3 m_GameAngularVelocity;
	/**
	 * 補間開始点の位置。
	 */
	Toolbox::FVector3 m_PrevPosition;
	/**
	 * 補間開始点の姿勢。
	 */
	Toolbox::FQuaternion m_PrevOrientation;
	/**
	 * 補間終了点の位置。
	 */
	Toolbox::FVector3 m_CurrentPosition;
	/**
	 * 補間終了点の姿勢。
	 */
	Toolbox::FQuaternion m_CurrentOrientation;
	/**
	 * 描画補間用の残余割合。
	 */
	Toolbox::f64 m_Alpha = 0;
	/**
	 * 予約した力。
	 */
	Toolbox::FVector3 m_PendingForce;
	/**
	 * 予約したトルク。
	 */
	Toolbox::FVector3 m_PendingTorque;
	/**
	 * 予約した力積。
	 */
	Toolbox::FVector3 m_PendingImpulse;
	/**
	 * 予約した力積モーメント。
	 */
	Toolbox::FVector3 m_PendingAngularImpulse;
	/**
	 * 予約した重心外力積。
	 */
	Toolbox::TVector<FPendingPointImpulse> m_PendingPoints;
};
/**
 * 兄弟の剛体へコライダーを取り付ける。剛体の生成を待って遅延接続する。
 */
class DCollider3DComponent : public DGameObjectComponent
{
public:
	/**
	 * 形状と材質を受け取り、取り付け前の状態を構築する。
	 * @param Description 取り付ける形状と材質。
	 */
	explicit DCollider3DComponent(FColliderDescription3D Description = {}) : m_Description(Description)
	{
	}
	/**
	 * 取り付けが済んでいるかを調べる。
	 */
	FORCEINLINE bool HasCollider() const noexcept
	{
		return m_bAttached;
	}

protected:
	/**
	 * 兄弟の剛体へ遅延接続する。
	 * @param Context 固定更新の実行環境。
	 */
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		if (Context.Physics3D == nullptr)
		{
			return;
		}
		m_pWorld = Context.Physics3D;
		DGameObject* Owner = GetOwner();
		if (Owner == nullptr)
		{
			return;
		}
		auto BodyHandle = Owner->FindComponent<DRigidBody3DComponent>();
		if (BodyHandle.Get() == nullptr)
		{
			return;
		}
		DRigidBody3DComponent* Body = BodyHandle.Get();
		if (!Body->HasBody() || Body->GetWorld() != Context.Physics3D)
		{
			m_bAttached = false;
			return;
		}
		if (!m_bAttached || !m_pWorld->IsColliderAlive(m_Collider))
		{
			m_Collider = m_pWorld->AttachCollider(Body->GetBodyId(), m_Description);
			m_bAttached = true;
		}
	}
	/**
	 * 取り付けを外す。
	 */
	void OnDeinitialize() noexcept override
	{
		if (m_bAttached && m_pWorld != nullptr)
		{
			m_pWorld->DetachCollider(m_Collider);
		}
		m_bAttached = false;
		m_pWorld = nullptr;
	}

private:
	/**
	 * 取り付ける形状と材質。
	 */
	FColliderDescription3D m_Description;
	/**
	 * 取り付け済みのコライダー。
	 */
	FColliderId3D m_Collider;
	/**
	 * 取り付けが済んでいるか。
	 */
	bool m_bAttached = false;
	/**
	 * 最後に参照した物理ワールド。所有しない。
	 */
	FPhysicsWorld3D* m_pWorld = nullptr;
};
} // namespace Dxf
#endif
