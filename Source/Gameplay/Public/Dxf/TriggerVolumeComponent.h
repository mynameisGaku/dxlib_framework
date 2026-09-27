#pragma once
#include "Dxf/GameObject.h"
#include "Dxf/GameObjectComponent.h"
#include "Dxf/PostPhysicsStep.h"
#include "Toolbox/Function.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * Triggerの領域の登録内容。
 * @tparam TTraits 次元の型と操作。
 */
template <typename TTraits> struct TTriggerVolumeDescription
{
	/**
	 * 領域の形状（Bodyの位置からの相対）。
	 */
	typename TTraits::FShape Shape;
	/**
	 * Bodyの初期位置。
	 */
	typename TTraits::FVector Position;
	/**
	 * 領域を動かすか。trueはKinematic（SetPositionで置き直せる）、falseはStatic（Static同士の組は調べない）。
	 */
	bool bMovable = false;
	/**
	 * 接触・Triggerの組を調べるかを決める衝突カテゴリとマスク。
	 */
	FColliderCollisionFilter Collision;
	/**
	 * 問い合わせ用のカテゴリ。既定の0は問い合わせ（Raycast等）の対象にしない。
	 */
	Toolbox::uint32 QueryCategory = 0;
};
/**
 * 自分で作ったSensorの領域へ入っているBodyの集合を、物理Stepの成功後に更新するComponent。
 * 同じBodyの複数のColliderが入っていても一つとして数え、全てのColliderが出た時に出たとする（早く空にならない）。
 * 領域のBody・Colliderはこの型だけが登録・更新・削除する（既存の剛体のColliderは監視しない）。
 * 入った・出た相手のIDは値として残り、削除済みの相手は現在のWorldで解決できない（出た理由がRemoved）。
 * WorldのイベントはシーンでSetEventSettingsにより明示的に有効化しておく必要がある。
 * @tparam TTraits 次元の型と操作（FPhysicsEventTraits2D／FPhysicsEventTraits3D）。
 */
template <typename TTraits> class TTriggerVolumeComponent : public DGameObjectComponent, private IPostPhysicsStep
{
public:
	/**
	 * BodyのID。
	 */
	using FBodyId = typename TTraits::FBodyId;
	/**
	 * ColliderのID。
	 */
	using FColliderId = typename TTraits::FColliderId;
	/**
	 * 位置の型。
	 */
	using FVector = typename TTraits::FVector;
	/**
	 * @param Description 領域の登録内容。
	 */
	explicit TTriggerVolumeComponent(TTriggerVolumeDescription<TTraits> Description) : m_Description(Description)
	{
	}
	/**
	 * Bodyが入った時の関数を設定する（Bodyの最初のColliderが入った時に一度）。
	 * @param Handler 入ったBodyを受け取る関数。
	 */
	void SetEnterHandler(Toolbox::TFunction<void(FBodyId)> Handler)
	{
		m_OnEnter = Toolbox::Move(Handler);
	}
	/**
	 * Bodyが出た時の関数を設定する（Bodyの最後のColliderが出た時に一度。理由は最後のEndの理由）。
	 * @param Handler 出たBodyと理由を受け取る関数。
	 */
	void SetExitHandler(Toolbox::TFunction<void(FBodyId, EWorldEventEndReason)> Handler)
	{
		m_OnExit = Toolbox::Move(Handler);
	}
	/**
	 * 領域を置き直す（Kinematicの場合だけ）。次の物理Stepの完了時点の位置で判定する。
	 * @param Position 新しい位置。
	 */
	void SetPosition(FVector Position)
	{
		if (!m_Description.bMovable)
		{
			throw Toolbox::FException("Static trigger volume cannot move");
		}
		m_Description.Position = Position;
		if (m_bHasBody && m_pWorld != nullptr)
		{
			TTraits::Place(*m_pWorld, m_Body, Position);
		}
	}
	/**
	 * 入っているBodyの数。
	 */
	FORCEINLINE Toolbox::size_t GetOccupantCount() const noexcept
	{
		return m_Occupants.Size();
	}
	/**
	 * 入っているか。
	 */
	FORCEINLINE bool IsOccupied() const noexcept
	{
		return !m_Occupants.IsEmpty();
	}
	/**
	 * 指定のBodyが入っているか。
	 * @param Body 調べるBody。
	 */
	bool Contains(FBodyId Body) const noexcept
	{
		return Find_Internal(Body) < m_Occupants.Size();
	}
	/**
	 * 入っている指定番目のBody。
	 * @param Index 0からGetOccupantCount()未満。
	 */
	FBodyId GetOccupant(Toolbox::size_t Index) const
	{
		if (Index >= m_Occupants.Size())
		{
			throw Toolbox::FException("Trigger occupant index out of range");
		}
		return m_Occupants[Index].Body;
	}
	/**
	 * 領域のSensorのCollider（未作成なら空）。
	 */
	Toolbox::TOptional<FColliderId> GetColliderId() const noexcept
	{
		if (!m_bHasBody)
		{
			return {};
		}
		return m_Collider;
	}

protected:
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		typename TTraits::FWorld* World = TTraits::GetWorld(Context);
		if (World == nullptr || Context.PostPhysicsStep == nullptr)
		{
			throw Toolbox::FException(Toolbox::FString("Trigger volume requires DPhysicsScene") + TTraits::Name);
		}
		if (m_pWorld != nullptr && m_pWorld != World)
		{
			throw Toolbox::FException("Trigger volume world changed");
		}
		if (!World->GetEventSettings().bEnabled)
		{
			throw Toolbox::FException(
			    "Trigger volume requires world events (call SetEventSettings on the physics world)");
		}
		m_pWorld = World;
		if (!m_bHasBody)
		{
			// 自分のSensorを作る（登録に失敗したら何も残さない）。
			m_Body = World->CreateBody(TTraits::BodyDescription(
			    m_Description.bMovable ? EBodyType::Kinematic : EBodyType::Static, m_Description.Position));
			try
			{
				auto Collider = TTraits::ColliderDescription(m_Description.Shape);
				Collider.Response = EColliderResponse::Sensor;
				Collider.Collision = m_Description.Collision;
				Collider.QueryCategory = m_Description.QueryCategory;
				m_Collider = World->AttachCollider(m_Body, Collider);
			}
			catch (...)
			{
				(void)World->DestroyBody(m_Body);
				throw;
			}
			m_bHasBody = true;
		}
		Context.PostPhysicsStep->Enqueue(*this);
	}
	void OnDeinitialize() noexcept override
	{
		if (m_bHasBody && m_pWorld != nullptr)
		{
			(void)m_pWorld->DestroyBody(m_Body);
		}
		m_bHasBody = false;
		m_pWorld = nullptr;
		m_Occupants.Clear();
	}

private:
	/**
	 * 入っているBodyと、入っているColliderの数。
	 */
	struct FOccupant
	{
		FBodyId Body;
		Toolbox::int32 Colliders = 0;
	};
	bool IsPostPhysicsStepAlive_Internal() const noexcept override
	{
		const DGameObject* Owner = GetOwner();
		return !IsDestroyRequested() && m_pWorld != nullptr && m_bHasBody &&
		       (Owner == nullptr || !Owner->IsDestroyRequested());
	}
	void OnPostPhysicsStep_Internal(const FFixedTickContext& Context) override
	{
		(void)Context;
		const auto& Batch = m_pWorld->GetEventBatch();
		if (!Batch.bPublished || Batch.BatchId == m_LastBatch)
		{
			return;
		}
		m_LastBatch = Batch.BatchId;
		const Toolbox::uint64 BatchId = Batch.BatchId;
		for (Toolbox::size_t Index = 0; Index < Batch.Events.Size(); ++Index)
		{
			if (!IsPostPhysicsStepAlive_Internal() || m_pWorld->GetEventBatch().BatchId != BatchId ||
			    !m_pWorld->GetEventBatch().bPublished)
			{
				return;
			}
			const auto Event = Batch.Events[Index];
			if (Event.Kind != EWorldEventKind::Trigger || Event.Phase == EWorldEventPhase::Stay)
			{
				continue;
			}
			// 自分のSensorが関わる組だけ。相手は自分以外のBody。
			const bool bSelfIsA = Event.ColliderA == m_Collider;
			if (!bSelfIsA && !(Event.ColliderB == m_Collider))
			{
				continue;
			}
			const FColliderId Other = bSelfIsA ? Event.ColliderB : Event.ColliderA;
			if (Event.Phase == EWorldEventPhase::Begin)
			{
				Enter_Internal(Other.Body);
			}
			else
			{
				Exit_Internal(Other.Body, Event.EndReason);
			}
		}
	}
	void Enter_Internal(FBodyId Body)
	{
		const Toolbox::size_t Found = Find_Internal(Body);
		if (Found < m_Occupants.Size())
		{
			++m_Occupants[Found].Colliders;
			return;
		}
		FOccupant Occupant;
		Occupant.Body = Body;
		Occupant.Colliders = 1;
		m_Occupants.PushBack(Occupant);
		if (m_OnEnter)
		{
			m_OnEnter(Body);
		}
	}
	void Exit_Internal(FBodyId Body, EWorldEventEndReason Reason)
	{
		const Toolbox::size_t Found = Find_Internal(Body);
		if (Found >= m_Occupants.Size())
		{
			return;
		}
		if (--m_Occupants[Found].Colliders > 0)
		{
			return;
		}
		// 入った順を保って外す。
		for (Toolbox::size_t Index = Found; Index + 1 < m_Occupants.Size(); ++Index)
		{
			m_Occupants[Index] = m_Occupants[Index + 1];
		}
		m_Occupants.PopBack();
		if (m_OnExit)
		{
			m_OnExit(Body, Reason);
		}
	}
	Toolbox::size_t Find_Internal(FBodyId Body) const noexcept
	{
		for (Toolbox::size_t Index = 0; Index < m_Occupants.Size(); ++Index)
		{
			if (m_Occupants[Index].Body == Body)
			{
				return Index;
			}
		}
		return m_Occupants.Size();
	}
	/**
	 * 領域の登録内容。
	 */
	TTriggerVolumeDescription<TTraits> m_Description;
	/**
	 * 入っているBody（入った順）。
	 */
	Toolbox::TVector<FOccupant> m_Occupants;
	/**
	 * 入った・出た時の関数。
	 */
	Toolbox::TFunction<void(FBodyId)> m_OnEnter;
	Toolbox::TFunction<void(FBodyId, EWorldEventEndReason)> m_OnExit;
	/**
	 * 参照するWorld（所有しない）。
	 */
	typename TTraits::FWorld* m_pWorld = nullptr;
	/**
	 * 自分で作ったBodyとSensor。
	 */
	FBodyId m_Body;
	FColliderId m_Collider;
	bool m_bHasBody = false;
	/**
	 * 最後に処理したバッチの通算番号。
	 */
	Toolbox::uint64 m_LastBatch = 0;
};
} // namespace Dxf
