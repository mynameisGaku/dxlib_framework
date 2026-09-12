#pragma once
#include "Dxf/LifecycleObject.h"
#include "Dxf/SlotMap.h"
#include "Toolbox/Algorithm.h"
namespace Dxf
{
/**
 * 優先順位に従うオブジェクト更新を管理する型。
 */
template <typename T> class TManagedUpdater
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Storage オブジェクトの格納先。
	 */
	explicit TManagedUpdater(TSlotMap<T>& Storage) : m_pStorage(&Storage)
	{
	}
	/**
	 * 更新対象へフレーム更新を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> Tick_Internal(const FTickContext& Context)
	{
		// 所有するオブジェクト群。
		auto Objects = m_pStorage->Snapshot();
		Toolbox::StableSort(Objects.Begin(), Objects.End(),
		                    [&](const auto& A, const auto& B)
		                    {
			                    // 最初の要素。
			                    const auto* First = m_pStorage->Find_Internal(A.GetId());
			                    // 二番目の要素。
			                    const auto* Second = m_pStorage->Find_Internal(B.GetId());
			                    return Toolbox::TPair(First->GetUpdateOrder(), First->GetCreationOrder_Internal()) <
			                           // 必要な依存関係を受け取り、初期状態を構築する。
			                           Toolbox::TPair(Second->GetUpdateOrder(), Second->GetCreationOrder_Internal());
		                    });
		// ハンドルを順に処理する。
		for (auto Handle : Objects)
		{
			// オブジェクト。
			T* Object = m_pStorage->Find_Internal(Handle.GetId());
			if (!Object)
			{
				continue;
			}
			// 処理結果。
			auto Result = Object->Tick_Internal(Context);
			if (!Result)
			{
				return Result;
			}
		}
		return {};
	}
	/**
	 * 更新対象へ固定時間更新を通知する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> FixedTick_Internal(const FFixedTickContext& Context)
	{
		// 所有するオブジェクト群。
		auto Objects = m_pStorage->Snapshot();
		Toolbox::StableSort(Objects.Begin(), Objects.End(),
		                    [&](const auto& A, const auto& B)
		                    {
			                    // 最初の要素。
			                    const auto* First = m_pStorage->Find_Internal(A.GetId());
			                    // 二番目の要素。
			                    const auto* Second = m_pStorage->Find_Internal(B.GetId());
			                    return Toolbox::TPair(First->GetUpdateOrder(), First->GetCreationOrder_Internal()) <
			                           // 必要な依存関係を受け取り、初期状態を構築する。
			                           Toolbox::TPair(Second->GetUpdateOrder(), Second->GetCreationOrder_Internal());
		                    });
		// ハンドルを順に処理する。
		for (auto Handle : Objects)
		{
			// オブジェクト。
			T* Object = m_pStorage->Find_Internal(Handle.GetId());
			if (!Object)
			{
				continue;
			}
			// 処理結果。
			auto Result = Object->FixedTick_Internal(Context);
			if (!Result)
			{
				return Result;
			}
		}
		return {};
	}

private:
	/**
	 * オブジェクトの格納先。
	 */
	TSlotMap<T>* m_pStorage;
};
} // namespace Dxf
