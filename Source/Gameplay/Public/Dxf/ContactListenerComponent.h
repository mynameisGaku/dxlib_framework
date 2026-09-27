#pragma once
#include "Dxf/GameObject.h"
#include "Dxf/GameObjectComponent.h"
#include "Dxf/PostPhysicsStep.h"
#include "Toolbox/Function.h"
#include "Toolbox/SharedPtr.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 既存のBody（所有者の剛体・キャラクター移動、または指定したBody）が関わる接触・Triggerの状態遷移を、
 * 物理Stepの成功後（World.Stepの外）にゲーム側へ配送するComponent。Body・Colliderを作らず、複製もしない。
 * WorldのイベントはシーンでSetEventSettingsにより明示的に有効化しておく必要がある（無効なら固定更新で例外）。
 * 固定更新ごとに、その固定更新の一つのバッチを一度だけ配送する（1描画フレームで3回なら3つのバッチ、0回なら配送なし）。
 * 配送の直前に自身・所有者の破棄の要求を確かめ、通知の中で破棄を要求した後は残りを配送しない。
 * 通知の中でWorldのイベントの設定を変えることはできない（変わった場合は残りを配送しない）。
 * @tparam TTraits 次元の型と操作（FPhysicsEventTraits2D／FPhysicsEventTraits3D）。
 */
template <typename TTraits> class TContactListenerComponent : public DGameObjectComponent, private IPostPhysicsStep
{
public:
	/**
	 * 受け取る側から見た状態遷移。
	 */
	using FNotice = typename TTraits::FNotice;
	/**
	 * BodyのID。
	 */
	using FBodyId = typename TTraits::FBodyId;
	/**
	 * @param bWatchOwner 所有者の剛体・キャラクター移動のBodyを自動で対象にするか。
	 */
	explicit TContactListenerComponent(bool bWatchOwner = true) : m_bWatchOwner(bWatchOwner)
	{
	}
	/**
	 * 状態遷移を受け取る関数を設定する。空の関数で解除する。例外は固定更新の失敗として伝わる。
	 * 実行中に解除・置換しても、その呼び出しが戻るまで元の関数と捕捉した資源を保つ。
	 * @param Handler 受け取る関数。
	 */
	void SetHandler(Toolbox::TFunction<void(const FNotice&)> Handler)
	{
		if (Handler)
		{
			m_Handler = Toolbox::MakeShared<Toolbox::TFunction<void(const FNotice&)>>(Toolbox::Move(Handler));
		}
		else
		{
			m_Handler.Reset();
		}
	}
	/**
	 * 指定のBodyを対象に加える。通知の中で加えた場合、同じバッチの以降のイベントから対象になる。
	 * @param Body 対象にするBody。
	 */
	void WatchBody(FBodyId Body)
	{
		if (!Contains_Internal(m_Watched, Body))
		{
			m_Watched.PushBack(Body);
		}
	}
	/**
	 * 指定のBodyを対象から外す。外したかを返す。
	 * @param Body 外すBody。
	 */
	bool UnwatchBody(FBodyId Body) noexcept
	{
		for (Toolbox::size_t Index = 0; Index < m_Watched.Size(); ++Index)
		{
			if (m_Watched[Index] == Body)
			{
				m_Watched[Index] = m_Watched.Back();
				m_Watched.PopBack();
				return true;
			}
		}
		return false;
	}
	/**
	 * 指定のBodyが対象か（指定・所有者のどちらでも）。
	 * @param Body 調べるBody。
	 */
	bool IsWatching(FBodyId Body) const noexcept
	{
		return Contains_Internal(m_Watched, Body) || Contains_Internal(m_OwnerBodies, Body);
	}
	/**
	 * 配送した状態遷移の累計。
	 */
	FORCEINLINE Toolbox::uint64 GetDeliveredCount() const noexcept
	{
		return m_Delivered;
	}
	/**
	 * 最後に配送したバッチの通算番号（未配送は0）。
	 */
	FORCEINLINE Toolbox::uint64 GetLastBatchId() const noexcept
	{
		return m_LastBatch;
	}
	/**
	 * 組の上限を超えてイベントのないバッチを受け取った回数（MaxPairsの見直しの目安）。
	 */
	FORCEINLINE Toolbox::uint64 GetOverflowCount() const noexcept
	{
		return m_Overflows;
	}

protected:
	/**
	 * 状態遷移を受け取る（派生先で上書きする場合）。関数を設定した場合はその前に呼ぶ。
	 * @param Notice 受け取る側から見た状態遷移。
	 */
	virtual void OnContact(const FNotice& Notice)
	{
		(void)Notice;
	}
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		typename TTraits::FWorld* World = TTraits::GetWorld(Context);
		if (World == nullptr || Context.PostPhysicsStep == nullptr)
		{
			throw Toolbox::FException(Toolbox::FString("Contact listener requires DPhysicsScene") + TTraits::Name);
		}
		if (m_pWorld != nullptr && m_pWorld != World)
		{
			throw Toolbox::FException("Contact listener world changed");
		}
		if (!World->GetEventSettings().bEnabled)
		{
			throw Toolbox::FException(
			    "Contact listener requires world events (call SetEventSettings on the physics world)");
		}
		m_pWorld = World;
		Context.PostPhysicsStep->Enqueue(*this);
	}
	void OnDeinitialize() noexcept override
	{
		m_pWorld = nullptr;
	}

private:
	bool IsPostPhysicsStepAlive_Internal() const noexcept override
	{
		const DGameObject* Owner = GetOwner();
		return !IsDestroyRequested() && m_pWorld != nullptr && (Owner == nullptr || !Owner->IsDestroyRequested());
	}
	void OnPostPhysicsStep_Internal(const FFixedTickContext& Context) override
	{
		(void)Context;
		// 同じ固定更新で作られた所有者のBodyも対象にするため、配送の直前に所有者のBodyを集め直す。
		m_OwnerBodies.Clear();
		if (m_bWatchOwner && GetOwner() != nullptr)
		{
			TTraits::CollectOwnerBodies(*GetOwner(), m_OwnerBodies);
		}
		const auto& Batch = m_pWorld->GetEventBatch();
		// 同じバッチを重ねて配送しない（読んでも消費しないため、番号で区別する）。
		if (!Batch.bPublished || Batch.BatchId == m_LastBatch)
		{
			return;
		}
		m_LastBatch = Batch.BatchId;
		const Toolbox::uint64 BatchId = Batch.BatchId;
		const Toolbox::uint64 StepIndex = Batch.StepIndex;
		m_Overflows += Batch.bOverflowed ? 1 : 0;
		for (Toolbox::size_t Index = 0; Index < Batch.Events.Size(); ++Index)
		{
			// 通知の中で自身・所有者の破棄を要求した、またはWorldの設定を変えた場合は残りを配送しない。
			if (!IsPostPhysicsStepAlive_Internal() || !m_pWorld->GetEventBatch().bPublished ||
			    m_pWorld->GetEventBatch().BatchId != BatchId)
			{
				return;
			}
			// イベントは値で写してから渡す（通知の中の変更で参照先が変わらないように）。
			const auto Event = Batch.Events[Index];
			if (IsWatching(Event.ColliderA.Body))
			{
				Deliver_Internal(MakeContactNotice(Event, true, StepIndex, BatchId));
			}
			if (IsWatching(Event.ColliderB.Body) && IsPostPhysicsStepAlive_Internal() &&
			    m_pWorld->GetEventBatch().BatchId == BatchId)
			{
				Deliver_Internal(MakeContactNotice(Event, false, StepIndex, BatchId));
			}
		}
	}
	void Deliver_Internal(const FNotice& Notice)
	{
		++m_Delivered;
		OnContact(Notice);
		// 実行中に関数を解除・置換しても、現在の捕捉状態を呼び出し終了まで保つ。
		const auto Handler = m_Handler;
		if (Handler && IsPostPhysicsStepAlive_Internal())
		{
			(*Handler)(Notice);
		}
	}
	static bool Contains_Internal(const Toolbox::TVector<FBodyId>& Bodies, FBodyId Body) noexcept
	{
		for (Toolbox::size_t Index = 0; Index < Bodies.Size(); ++Index)
		{
			if (Bodies[Index] == Body)
			{
				return true;
			}
		}
		return false;
	}
	/**
	 * 所有者のBodyを自動で対象にするか。
	 */
	bool m_bWatchOwner = true;
	/**
	 * 指定した対象のBody。
	 */
	Toolbox::TVector<FBodyId> m_Watched;
	/**
	 * 配送の直前に集めた所有者のBody。
	 */
	Toolbox::TVector<FBodyId> m_OwnerBodies;
	/**
	 * 受け取る関数。実行中だけ呼び出し側と所有を共有する。
	 */
	Toolbox::TSharedPtr<Toolbox::TFunction<void(const FNotice&)>> m_Handler;
	/**
	 * 参照するWorld（所有しない）。
	 */
	typename TTraits::FWorld* m_pWorld = nullptr;
	/**
	 * 最後に配送したバッチの通算番号。
	 */
	Toolbox::uint64 m_LastBatch = 0;
	/**
	 * 配送した状態遷移の累計。
	 */
	Toolbox::uint64 m_Delivered = 0;
	/**
	 * 上限超過のバッチを受け取った回数。
	 */
	Toolbox::uint64 m_Overflows = 0;
};
} // namespace Dxf
