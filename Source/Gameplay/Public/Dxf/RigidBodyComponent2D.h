// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_RIGID_BODY_COMPONENT_2D_H
#define DXF_GAMEPLAY_RIGID_BODY_COMPONENT_2D_H
#include "Dxf/GameObjectComponent.h"
#include "Dxf/GameObject.h"
#include "Dxf/RigidBody2D.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 物理シーンの剛体へゲーム側の姿勢と外力を中継する。
 * Dynamicは物理状態が正、Kinematicはゲーム側の速度指示が正になる。
 */
class DRigidBody2DComponent : public DGameObjectComponent
{
public:
	/**
	 * 初期条件を受け取り、物理登録前の状態を構築する。
	 * @param Description 初期条件。
	 */
	explicit DRigidBody2DComponent(FBodyDescription2D Description = {}) : m_Description(Description)
	{
		m_GamePosition = Description.Position;
		m_GameAngle = Description.Angle;
		m_GameVelocity = Description.Velocity;
		m_GameAngularVelocity = Description.AngularVelocity;
		m_PrevPosition = m_GamePosition;
		m_PrevAngle = m_GameAngle;
		m_CurrentPosition = m_GamePosition;
		m_CurrentAngle = m_GameAngle;
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
	FORCEINLINE FBodyId2D GetBodyId() const
	{
		if (!m_bHasBody)
		{
			throw Toolbox::FException("2D rigid body is not created");
		}
		return m_Body;
	}
	/**
	 * 最後に参照した物理ワールドを返す。未参照はnullptr。
	 */
	FORCEINLINE FPhysicsWorld2D* GetWorld() noexcept
	{
		return m_pWorld;
	}
	/**
	 * ゲーム側の重心位置を返す。
	 */
	FORCEINLINE Toolbox::FVector2 GetGamePosition() const noexcept
	{
		return m_GamePosition;
	}
	/**
	 * ゲーム側の姿勢角を返す。
	 */
	FORCEINLINE Toolbox::f32 GetGameAngle() const noexcept
	{
		return m_GameAngle;
	}
	/**
	 * ゲーム側の重心位置と姿勢を指定する。Kinematicの移動指示に使う。
	 * @param Position メートル単位の重心位置。
	 * @param Angle ラジアン単位の姿勢角。
	 */
	void SetGameTransform(Toolbox::FVector2 Position, Toolbox::f32 Angle)
	{
		if (!Position.IsValid() || !Toolbox::IsFinite(Angle))
		{
			throw Toolbox::FException("Invalid 2D game transform");
		}
		m_GamePosition = Position;
		m_GameAngle = Angle;
	}
	/**
	 * ゲーム側の速度を指定する。Kinematicの運動指示に使う。
	 * @param Velocity 毎秒メートル単位の速度。
	 * @param AngularVelocity 毎秒ラジアン単位の角速度。
	 */
	void SetGameVelocity(Toolbox::FVector2 Velocity, Toolbox::f32 AngularVelocity)
	{
		if (!Velocity.IsValid() || !Toolbox::IsFinite(AngularVelocity))
		{
			throw Toolbox::FException("Invalid 2D game velocity");
		}
		m_GameVelocity = Velocity;
		m_GameAngularVelocity = AngularVelocity;
	}
	/**
	 * 物理姿勢を直接移し、補間履歴と接触記録を破棄する。
	 * @param Position メートル単位の重心位置。
	 * @param Angle ラジアン単位の姿勢角。
	 */
	void Teleport(Toolbox::FVector2 Position, Toolbox::f32 Angle)
	{
		if (!Position.IsValid() || !Toolbox::IsFinite(Angle))
		{
			throw Toolbox::FException("Invalid 2D teleport transform");
		}
		m_GamePosition = Position;
		m_GameAngle = Angle;
		m_PrevPosition = Position;
		m_PrevAngle = Angle;
		m_CurrentPosition = Position;
		m_CurrentAngle = Angle;
		m_Alpha = 0;
		if (m_bHasBody && m_pWorld != nullptr)
		{
			m_pWorld->SetBodyTransform(m_Body, Position, Angle);
			m_pWorld->ClearContactCache();
		}
	}
	/**
	 * 次の固定更新で使う力を加算する。Dynamic以外は例外で通知する。
	 * @param Force ニュートン単位の力。
	 */
	void AddForce(Toolbox::FVector2 Force)
	{
		if (!Force.IsValid())
		{
			throw Toolbox::FException("Invalid 2D pending force");
		}
		RequireDynamic_Internal();
		m_PendingForce += Force;
	}
	/**
	 * 次の固定更新で使うトルクを加算する。Dynamic以外は例外で通知する。
	 * @param Torque ニュートンメートル単位のトルク。
	 */
	void AddTorque(Toolbox::f32 Torque)
	{
		if (!Toolbox::IsFinite(Torque))
		{
			throw Toolbox::FException("Invalid 2D pending torque");
		}
		RequireDynamic_Internal();
		m_PendingTorque += Torque;
	}
	/**
	 * 速度へ即時反映する力積を予約する。Dynamic以外は例外で通知する。
	 * @param Impulse ニュートン秒単位の力積。
	 */
	void AddImpulse(Toolbox::FVector2 Impulse)
	{
		if (!Impulse.IsValid())
		{
			throw Toolbox::FException("Invalid 2D pending impulse");
		}
		RequireDynamic_Internal();
		m_PendingImpulse += Impulse;
	}
	/**
	 * 角速度へ即時反映する力積モーメントを予約する。Dynamic以外は例外で通知する。
	 * @param Impulse ニュートンメートル秒単位の力積モーメント。
	 */
	void AddAngularImpulse(Toolbox::f32 Impulse)
	{
		if (!Toolbox::IsFinite(Impulse))
		{
			throw Toolbox::FException("Invalid 2D pending angular impulse");
		}
		RequireDynamic_Internal();
		m_PendingAngularImpulse += Impulse;
	}
	/**
	 * 重心外の点への力積を予約する。Dynamic以外は例外で通知する。
	 * @param Impulse ニュートン秒単位の力積。
	 * @param WorldPoint 力積を与えるワールド位置。
	 */
	void AddImpulseAt(Toolbox::FVector2 Impulse, Toolbox::FVector2 WorldPoint)
	{
		if (!Impulse.IsValid() || !WorldPoint.IsValid())
		{
			throw Toolbox::FException("Invalid 2D pending impulse point");
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
	Toolbox::FVector2 GetRenderPosition() const noexcept
	{
		const Toolbox::f32 Alpha = static_cast<Toolbox::f32>(m_Alpha);
		return m_PrevPosition * (1 - Alpha) + m_CurrentPosition * Alpha;
	}
	/**
	 * 描画用の補間姿勢を返す。
	 */
	Toolbox::f32 GetRenderAngle() const noexcept
	{
		const Toolbox::f32 Alpha = static_cast<Toolbox::f32>(m_Alpha);
		return m_PrevAngle * (1 - Alpha) + m_CurrentAngle * Alpha;
	}

protected:
	/**
	 * 物理登録の確保と固定更新前の同期を行う。
	 * @param Context 固定更新の実行環境。
	 */
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		if (Context.Physics2D == nullptr)
		{
			return;
		}
		m_pWorld = Context.Physics2D;
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
		m_PrevAngle = m_pWorld->GetAngle(m_Body);
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
			m_CurrentAngle = m_pWorld->GetAngle(m_Body);
		}
		else
		{
			m_CurrentPosition = m_GamePosition;
			m_CurrentAngle = m_GameAngle;
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
		Toolbox::FVector2 Impulse;
		/**
		 * 力積を与える位置。
		 */
		Toolbox::FVector2 Point;
	};
	/**
	 * Dynamic以外への外力予約を拒否する。
	 */
	void RequireDynamic_Internal() const
	{
		if (m_Type != EBodyType::Dynamic)
		{
			throw Toolbox::FException("Only dynamic 2D bodies accept inputs");
		}
	}
	/**
	 * 物理登録を確保する。
	 * @param World 取り付け先の物理ワールド。
	 */
	void EnsureBody_Internal(FPhysicsWorld2D& World)
	{
		FBodyDescription2D Description = m_Description;
		Description.Position = m_GamePosition;
		Description.Angle = m_GameAngle;
		Description.Velocity = m_GameVelocity;
		Description.AngularVelocity = m_GameAngularVelocity;
		m_Body = World.CreateBody(Description);
		m_bHasBody = true;
		m_PrevPosition = m_GamePosition;
		m_PrevAngle = m_GameAngle;
		m_CurrentPosition = m_GamePosition;
		m_CurrentAngle = m_GameAngle;
	}
	/**
	 * 予約した外力を物理へ渡して消去する。
	 * @param World 取り付け先の物理ワールド。
	 */
	void FlushPending_Internal(FPhysicsWorld2D& World)
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
		m_PendingTorque = 0;
		m_PendingImpulse = {};
		m_PendingAngularImpulse = 0;
		m_PendingPoints.Clear();
	}
	/**
	 * 生成時の初期条件。
	 */
	FBodyDescription2D m_Description;
	/**
	 * 物理登録のID。
	 */
	FBodyId2D m_Body;
	/**
	 * 物理登録が済んでいるか。
	 */
	bool m_bHasBody = false;
	/**
	 * 最後に参照した物理ワールド。所有しない。
	 */
	FPhysicsWorld2D* m_pWorld = nullptr;
	/**
	 * 運動区分。
	 */
	EBodyType m_Type = EBodyType::Dynamic;
	/**
	 * ゲーム側の重心位置。
	 */
	Toolbox::FVector2 m_GamePosition;
	/**
	 * ゲーム側の姿勢角。
	 */
	Toolbox::f32 m_GameAngle = 0;
	/**
	 * ゲーム側の速度。
	 */
	Toolbox::FVector2 m_GameVelocity;
	/**
	 * ゲーム側の角速度。
	 */
	Toolbox::f32 m_GameAngularVelocity = 0;
	/**
	 * 補間開始点の位置。
	 */
	Toolbox::FVector2 m_PrevPosition;
	/**
	 * 補間開始点の姿勢角。
	 */
	Toolbox::f32 m_PrevAngle = 0;
	/**
	 * 補間終了点の位置。
	 */
	Toolbox::FVector2 m_CurrentPosition;
	/**
	 * 補間終了点の姿勢角。
	 */
	Toolbox::f32 m_CurrentAngle = 0;
	/**
	 * 描画補間用の残余割合。
	 */
	Toolbox::f64 m_Alpha = 0;
	/**
	 * 予約した力。
	 */
	Toolbox::FVector2 m_PendingForce;
	/**
	 * 予約したトルク。
	 */
	Toolbox::f32 m_PendingTorque = 0;
	/**
	 * 予約した力積。
	 */
	Toolbox::FVector2 m_PendingImpulse;
	/**
	 * 予約した力積モーメント。
	 */
	Toolbox::f32 m_PendingAngularImpulse = 0;
	/**
	 * 予約した重心外力積。
	 */
	Toolbox::TVector<FPendingPointImpulse> m_PendingPoints;
};
/**
 * 兄弟の剛体へコライダーを取り付ける。剛体の生成を待って遅延接続する。
 */
class DCollider2DComponent : public DGameObjectComponent
{
public:
	/**
	 * 形状と材質を受け取り、取り付け前の状態を構築する。
	 * @param Description 取り付ける形状と材質。
	 */
	explicit DCollider2DComponent(FColliderDescription2D Description = {}) : m_Description(Description)
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
		if (Context.Physics2D == nullptr)
		{
			return;
		}
		m_pWorld = Context.Physics2D;
		DGameObject* Owner = GetOwner();
		if (Owner == nullptr)
		{
			return;
		}
		auto BodyHandle = Owner->FindComponent<DRigidBody2DComponent>();
		if (BodyHandle.Get() == nullptr)
		{
			return;
		}
		DRigidBody2DComponent* Body = BodyHandle.Get();
		if (!Body->HasBody() || Body->GetWorld() != Context.Physics2D)
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
	FColliderDescription2D m_Description;
	/**
	 * 取り付け済みのコライダー。
	 */
	FColliderId2D m_Collider;
	/**
	 * 取り付けが済んでいるか。
	 */
	bool m_bAttached = false;
	/**
	 * 最後に参照した物理ワールド。所有しない。
	 */
	FPhysicsWorld2D* m_pWorld = nullptr;
};
} // namespace Dxf
#endif
