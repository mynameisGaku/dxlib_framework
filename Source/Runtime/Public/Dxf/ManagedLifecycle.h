#pragma once
#include "Dxf/LifecycleObject.h"
#include "Dxf/SlotMap.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * 利用者のコールバックを実行する前に、要求済みの変更を子まで再帰的に固定する。
 */
template <typename T> class TManagedLifecycle
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Storage オブジェクトの格納先。
	 */
	explicit TManagedLifecycle(TSlotMap<T>& Storage) : m_pStorage(&Storage)
	{
	}
	/**
	 * 利用者の処理を呼ぶ前に今回の変更対象を固定する。
	 */
	void FreezeBoundary_Internal()
	{
		if (m_bFrozen)
		{
			return;
		}
		m_Batch.Clear();
		/**
		 * ハンドルを順に処理する。
		 */
		for (auto Handle : m_pStorage->Snapshot())
		{
			/**
			 * オブジェクト。
			 */
			T* Object = m_pStorage->Find_Internal(Handle.GetId());
			/**
			 * 実行するバックエンド処理。
			 */
			const auto Operation =
			    Object->IsDestroyRequested()
			        ? EOperation::Destroy
			        : (Object->GetState() == ELifecycleState::Pending ? EOperation::Initialize : EOperation::Keep);
			m_Batch.PushBack({Handle, Operation});
			if (Operation == EOperation::Keep && Object->GetChildren_Internal())
			{
				Object->GetChildren_Internal()->FreezeBoundary_Internal();
			}
		}
		m_bFrozen = true;
	}
	/**
	 * 更新境界でシーンの変更を反映する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> CommitBoundary_Internal(const FInitContext& Context)
	{
		if (!m_bFrozen)
		{
			FreezeBoundary_Internal();
		}
		/**
		 * 境界で固定した処理対象。
		 */
		auto Batch = Toolbox::Move(m_Batch);
		m_Batch.Clear();
		m_bFrozen = false;
		/**
		 * 最初に発生したエラー。
		 */
		Toolbox::TOptional<FError> FirstError;
		/**
		 * 現在処理する登録項目を順に処理する。
		 */
		for (const auto& Entry : Batch)
		{
			if (Entry.Operation != EOperation::Destroy)
			{
				continue;
			}
			/**
			 * 取得したオブジェクトが有効な場合だけ処理する。
			 */
			if (T* Object = m_pStorage->Find_Internal(Entry.Handle.GetId()))
			{
				Object->Shutdown_Internal();
				m_pStorage->Remove(Entry.Handle);
			}
		}
		/**
		 * 現在処理する登録項目を順に処理する。
		 */
		for (const auto& Entry : Batch)
		{
			/**
			 * オブジェクト。
			 */
			T* Object = m_pStorage->Find_Internal(Entry.Handle.GetId());
			if (!Object || Object->IsDestroyRequested() || Entry.Operation == EOperation::Destroy)
			{
				continue;
			}
			/**
			 * 処理結果。
			 */
			TResult<void> Result;
			if (Entry.Operation == EOperation::Initialize)
			{
				Result = Object->Initialize_Internal(Context);
				if (!Result)
				{
					m_pStorage->Remove(Entry.Handle);
				}
			}
			/**
			 * 取得したオブジェクトが有効な場合だけ処理する。
			 */
			else if (auto* Children = Object->GetChildren_Internal())
			{
				Result = Children->CommitBoundary_Internal(Context);
			}
			if (!Result && !FirstError)
			{
				FirstError = Result.Error();
			}
		}
		return FirstError ? TResult<void>::Failure(*FirstError) : TResult<void>{};
	}
	/**
	 * 管理する処理とリソースを順序どおり終了する。
	 */
	void Shutdown_Internal() noexcept
	{
		m_Batch.Clear();
		m_bFrozen = false;
		m_pStorage->ForEach_Internal(
		    [](T& Object)
		    {
			    Object.Shutdown_Internal();
		    });
		m_pStorage->RemoveIf_Internal(
		    [](const T&)
		    {
			    return true;
		    });
	}

private:
	/**
	 * 実行するバックエンド処理を管理する型。
	 */
	enum class EOperation
	{
		Initialize,
		Keep,
		Destroy
	};
	/**
	 * 現在処理する登録項目を管理する型。
	 */
	struct FEntry
	{
		/**
		 * ハンドル。
		 */
		TObjectHandle<T> Handle;
		/**
		 * 実行するバックエンド処理。
		 */
		EOperation Operation;
	};
	/**
	 * オブジェクトの格納先。
	 */
	TSlotMap<T>* m_pStorage;
	/**
	 * 境界で固定した処理対象。
	 */
	Toolbox::TVector<FEntry> m_Batch;
	/**
	 * この境界で処理対象を固定済みか。
	 */
	bool m_bFrozen = false;
};
} // namespace Dxf
